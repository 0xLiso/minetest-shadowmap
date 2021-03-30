#include "client/shadows/dynamicshadowsrender.h"

#include <type_traits>
#include <utility>
#include "settings.h"
#include "filesys.h"
#include "porting.h"
#include "client/shader.h"

#include "client/clientmap.h"

ShadowRenderer::ShadowRenderer(irr::IrrlichtDevice *irrlichtDevice, Client *client) :
		_device(irrlichtDevice), _smgr(irrlichtDevice->getSceneManager()),
		_driver(irrlichtDevice->getVideoDriver()), _client(client)
{
	_shadows_enabled = g_settings->getBool("enable_shaders");
	_shadows_enabled &= g_settings->getBool("enable_dynamic_shadows");
	if (!_shadows_enabled)
		return;

	_shadow_strength = g_settings->getFloat("shadow_strength");

	_shadow_map_max_distance = g_settings->getFloat("shadow_map_max_distance");

	_shadow_map_texture_size = g_settings->getFloat("shadow_map_texture_size");

	_shadow_map_texture_32bit = g_settings->getBool("shadow_map_texture_32bit");

	_enable_csm = g_settings->getBool("enable_csm");

	_shadow_map_use_VMS = g_settings->getBool("shadow_map_use_VMS");
}

ShadowRenderer::~ShadowRenderer()
{
	if (_shadow_depth_cb)
		delete _shadow_depth_cb;
	if (_shadow_mix_cb)
		delete _shadow_mix_cb;

	// we don't have to delete the textures in renderTargets
}

void ShadowRenderer::initialize()
{
	bool tempTexFlagMipMaps =
			_driver->getTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS);
	bool tempTexFlag32 =
			_driver->getTextureCreationFlag(irr::video::ETCF_ALWAYS_32_BIT);
	_driver->setTextureCreationFlag(
			irr::video::ETCF_CREATE_MIP_MAPS, tempTexFlagMipMaps);
	_driver->setTextureCreationFlag(irr::video::ETCF_ALWAYS_32_BIT, tempTexFlag32);

	irr::video::IGPUProgrammingServices *gpu = _driver->getGPUProgrammingServices();

	// we need glsl
	if (_shadows_enabled && gpu &&
			(_driver->getDriverType() == irr::video::EDT_OPENGL &&
					_driver->queryFeature(
							irr::video::EVDF_ARB_GLSL))) {

		createShaders();

	} else {
		_shadows_enabled = false;

		_device->getLogger()->log(
				"Shadows: GLSL Shader not supported on this system.",
				ELL_WARNING);
		return;
	}

	if (_shadow_map_use_VMS) {
		_texture_format =
				_shadow_map_texture_32bit
						? irr::video::ECOLOR_FORMAT::ECF_G32R32F
						: irr::video::ECOLOR_FORMAT::ECF_G16R16F;
	} else {
		_texture_format = _shadow_map_texture_32bit
						  ? irr::video::ECOLOR_FORMAT::ECF_R32F
						  : irr::video::ECOLOR_FORMAT::ECF_R16F;
	}
	_nSplits = _enable_csm ? E_SHADOW_RENDER_MODE::ERM_CSM
			       : E_SHADOW_RENDER_MODE::ERM_SHADOWMAP;
}

size_t ShadowRenderer::addDirectionalLight()
{

	_light_list.push_back(DirectionalLight(_shadow_map_texture_size,
			irr::core::vector3df(0.f, 0.f, 0.f),
			video::SColor(255, 255, 255, 255), _shadow_map_max_distance,
			_nSplits));
	return _light_list.size() - 1;
}

DirectionalLight &ShadowRenderer::getDirectionalLight(irr::u32 index)
{
	return _light_list[index];
}

size_t ShadowRenderer::getDirectionalLightCount() const
{
	return _light_list.size();
}

void ShadowRenderer::addNodeToShadowList(
		irr::scene::ISceneNode *node, E_SHADOW_MODE shadowMode)
{
	ShadowNodeArray.push_back(NodeToApply(node, shadowMode));
}

void ShadowRenderer::removeNodeFromShadowList(irr::scene::ISceneNode *node)
{
	for (auto it = ShadowNodeArray.begin(); it != ShadowNodeArray.end();) {
		if (it->node == node) {
			it = ShadowNodeArray.erase(it);
			break;
		} else {
			++it;
		}
	}
}

void ShadowRenderer::setClearColor(irr::video::SColor ClearColor)
{
	_clear_color = ClearColor;
}

irr::IrrlichtDevice *ShadowRenderer::getIrrlichtDevice()
{
	return _device;
}

irr::scene::ISceneManager *ShadowRenderer::getSceneManager()
{
	return _smgr;
}

void ShadowRenderer::update(irr::video::ITexture *outputTarget)
{
	if (!_shadows_enabled || _smgr->getActiveCamera() == nullptr) {
		_smgr->drawAll();
		return;
	}
	
	
	

			// The merge all shadowmaps texture
	if (!shadowMapTextureFinal) {
		shadowMapTextureFinal = getSMTexture(
				std::string("shadowmap_final_") +
						std::to_string(_shadow_map_texture_size),
				_shadow_map_texture_32bit
						? irr::video::ECOLOR_FORMAT::
								  ECF_A32B32G32R32F
						: irr::video::ECOLOR_FORMAT::
								  ECF_A16B16G16R16F);
	}
	if (!shadowMapTextureDynamicObjects) {

		shadowMapTextureDynamicObjects = getSMTexture(
				std::string("shadow_dynamic_") +
						std::to_string(_shadow_map_texture_size),
				_texture_format);
	}

			if (renderTargets.empty()) {
				 
					for (s32 i = 0; i < _nSplits; i++) {

						renderTargets.push_back(getSMTexture(
								std::string("shadow_") +
										std::to_string(i) +
										"_" +
										std::to_string(_shadow_map_texture_size),
								_texture_format));
					}

			
			}
	

	if (!ShadowNodeArray.empty() && !_light_list.empty()) {
		// for every directional light:
		for (DirectionalLight &light : _light_list) {
			// Static shader values.
			_shadow_depth_cb->MapRes = (f32)_shadow_map_texture_size;
			_shadow_depth_cb->MaxFar = (f32)_shadow_map_max_distance * BS;
		
			if (light.should_update_shadow) {
				light.should_update_shadow = false;
				for (irr::s32 nSplit = 0; nSplit < light.getNumberSplits();
						nSplit++) {

					renderShadowSplit(renderTargets[nSplit], light,
							nSplit);

				}
			}	  // end if should render clientmap shadows

		
			//render shadows for the n0n-map objects.
			renderShadowObjects(shadowMapTextureDynamicObjects,light);

			
			//we should make a second pass :(
			for (int i = 0; i < light.getNumberSplits(); i++) {
				_screen_quad->getMaterial().setTexture(i,
					renderTargets[i]);
			}
			//dynamic objs shadow texture.
			_screen_quad->getMaterial().setTexture(
					3,  shadowMapTextureDynamicObjects );

			_driver->setRenderTarget(shadowMapTextureFinal,
					true, true);
			_screen_quad->render(_driver);
			_driver->setRenderTarget(0, false, false);
			

		} // end for lights

		// now render the actual MT render pass
		_driver->setRenderTarget(outputTarget, true, true, _clear_color);
		_smgr->drawAll();

		/*

		//this is debug, ignore for now.
		_driver->draw2DImage(shadowMapTextureFinal,
				irr::core::rect<s32>(0, 50 , 128, 128 + 50  ),
				irr::core::rect<s32>(0, 0,
						renderTargets[0]->getSize().Width,
						renderTargets[0]->getSize().Height));

		if (_enable_csm) {

			_driver->draw2DImage(renderTargets[E_SHADOW_TEXTURE::SM_CLIENTMAP0],
					irr::core::rect<s32>(
							0, 50 + 128, 128, 128 + 50 + 128),
					irr::core::rect<s32>(0, 0,
							renderTargets[0]->getSize().Width,
							renderTargets[0]->getSize()
									.Height));

			/*_driver->draw2DImage(renderTargets[1],
					irr::core::rect<s32>(
							0, 50 + 256, 128, 128 + 50 + 256),
					irr::core::rect<s32>(0, 0,
							renderTargets[0]->getSize().Width,
							renderTargets[0]->getSize()
									.Height));

			_driver->draw2DImage(renderTargets[2],
					irr::core::rect<s32>(0, 50 + 128 + 256, 128,
							128 + 50 + 128 + 256),
					irr::core::rect<s32>(0, 0,
							renderTargets[0]->getSize().Width,
							renderTargets[0]->getSize()
									.Height));
			_driver->draw2DImage(shadowMapTextureDynamicObjects,
					irr::core::rect<s32>(0, 50 +   256  , 128,
							128 + 50   + 256  ),
					irr::core::rect<s32>(0, 0,
							shadowMapTextureDynamicObjects
									->getSize()
									.Width,
							shadowMapTextureDynamicObjects
									->getSize()
									.Height));
		}*/
		_driver->setRenderTarget(0, false, false);
	}
}

irr::video::ITexture *ShadowRenderer::get_texture()
{
	return shadowMapTextureFinal;
}

irr::video::ITexture *ShadowRenderer::getSMTexture(const std::string &shadowMapName,
		irr::video::ECOLOR_FORMAT texture_format)
{
	irr::video::ITexture *shadowMapTexture =
			_driver->getTexture(shadowMapName.c_str());

	if (shadowMapTexture == nullptr) {

		shadowMapTexture = _driver->addRenderTargetTexture(
				irr::core::dimension2du(_shadow_map_texture_size,
						_shadow_map_texture_size),
				shadowMapName.c_str(), texture_format);
	}

	return shadowMapTexture;
}

void ShadowRenderer::renderShadowSplit(irr::video::ITexture *target,
									DirectionalLight &light, int nSplit)
{

		_driver->setTransform(irr::video::ETS_VIEW, light.getViewMatrix(nSplit));
		_driver->setTransform(irr::video::ETS_PROJECTION,
				light.getProjectionMatrix(nSplit));
		_shadow_depth_cb->idx = nSplit;

		// right now we can only render in usual RTT, not
		// Depth texture is available in irrlicth maybe we
		// should put some gl* fn here
		_driver->setRenderTarget(renderTargets[nSplit], true, true,
				irr::video::SColor(255, 255, 255, 255));

		/// Render all shadow casters
		///
		///
		for (const auto &shadow_node : ShadowNodeArray) {
			// If it's the Map, we have to handle it
			// differently.
			// F$%�ck irrlicht and it�s u8 chars :/
			if (std::string(shadow_node.node->getName()) == "ClientMap") {

				ClientMap *map_node = static_cast<ClientMap *>(
						shadow_node.node);

				// lets go with the actual render.

				irr::video::SMaterial material;
				if (map_node->getMaterialCount() > 0) {
					// we only want the first
					// material, which is the
					// one with the albedo
					// info ;)
					material = map_node->getMaterial(0);
				}

				material.MaterialType =
						(irr::video::E_MATERIAL_TYPE)depth_shader;

				// IDK if we need the back face
				// culling...
				material.BackfaceCulling = false;
				material.FrontfaceCulling = false;
				material.PolygonOffsetFactor = 1;
				map_node->OnAnimate(_device->getTimer()->getTime());

				_driver->setTransform(irr::video::ETS_WORLD,
						map_node->getAbsoluteTransformation());

				map_node->renderMapShadows(_driver, material,
						_smgr->getSceneNodeRenderPass(),
						light.getPosition(nSplit),
						light.getDirection(),
						_shadow_map_max_distance * BS, false);

				// restore material changes.

				material.BackfaceCulling = true;
				material.FrontfaceCulling = false;
				break;
			} // end clientMap render
		}
		// clear the Render Target
		_driver->setRenderTarget(0, false, false );
}

void ShadowRenderer::renderShadowObjects(
		irr::video::ITexture *target, DirectionalLight &light)
{
	// set the Render Target
	_driver->setRenderTarget(
			target, true, true, irr::video::SColor(255, 255, 255, 255));

	_driver->setTransform(irr::video::ETS_VIEW,
		light.getViewMatrix(E_SHADOW_TEXTURE::SM_CLIENTMAP0));
	_driver->setTransform(irr::video::ETS_PROJECTION,
			light.getProjectionMatrix(E_SHADOW_TEXTURE::SM_CLIENTMAP0));
	_shadow_depth_cb->idx = E_SHADOW_TEXTURE::SM_CLIENTMAP0;


	for (const auto &shadow_node : ShadowNodeArray) {
		// we only take care of the shadow casters
		if (shadow_node.shadowMode == ESM_RECEIVE ||
				shadow_node.shadowMode == ESM_EXCLUDE ||
				!shadow_node.node)
			continue;
		if (std::string(shadow_node.node->getName()) == "ClientMap")
			continue;
		// render other objects
		irr::u32 n_node_materials = shadow_node.node->getMaterialCount();
		std::vector<irr::s32> BufferMaterialList;
		std::vector<std::pair<bool, bool>> BufferMaterialCullingList;
		BufferMaterialList.reserve(n_node_materials);
		// backup materialtype for each material
		// (aka shader)
		// and replace it by our "depth" shader
		for (u32 m = 0; m < n_node_materials; m++) {
			BufferMaterialList.push_back(
					shadow_node.node->getMaterial(m).MaterialType);

			auto &current_mat = shadow_node.node->getMaterial(m);

			current_mat.MaterialType =
					(irr::video::E_MATERIAL_TYPE)depth_shader;
			BufferMaterialCullingList.push_back(std::make_pair<bool, bool>(
					current_mat.BackfaceCulling ? true : false,
					current_mat.FrontfaceCulling ? true : false));
			current_mat.BackfaceCulling = false;
			current_mat.FrontfaceCulling = false;
		}

		_driver->setTransform(irr::video::ETS_WORLD,
				shadow_node.node->getAbsoluteTransformation());
		shadow_node.node->render();

		// restore the material.

		for (u32 m = 0; m < n_node_materials; m++) {

			auto &current_mat = shadow_node.node->getMaterial(m);
			current_mat.MaterialType = (irr::video::E_MATERIAL_TYPE)
					BufferMaterialList[m];

			current_mat.BackfaceCulling = BufferMaterialCullingList[m].first;
			current_mat.FrontfaceCulling =
					BufferMaterialCullingList[m].second;

			current_mat.PolygonOffsetFactor = 1;
		}

	} // end for caster shadow nodes


	// clear the Render Target
	_driver->setRenderTarget(0, false, false);

}

void ShadowRenderer::mixShadowsQuad()
{
}

/*
 * @Liso's disclaimer ;) This function loads the Shadow Mapping Shaders.
 * I used a custom loader because I couldn't figure out how to use the base
 * Shaders system with custom IShaderConstantSetCallBack without messing up the
 * code too much. If anyone knows how to integrate this with the standard MT
 * shaders, please feel free to change it.
 */

void ShadowRenderer::createShaders()
{
	irr::video::IGPUProgrammingServices *gpu = _driver->getGPUProgrammingServices();

	if (depth_shader == -1) {
		std::string depth_shader_vs =
				getShaderPath("shadow_shaders", "shadow_pass1.vs");
		if (depth_shader_vs.empty()) {
			_shadows_enabled = false;
			_device->getLogger()->log("Error shadow mapping vs "
						  "shader not found.",
					ELL_WARNING);
			return;
		}
		std::string depth_shader_fs =
				getShaderPath("shadow_shaders", "shadow_pass1.fs");
		if (depth_shader_fs.empty()) {
			_shadows_enabled = false;
			_device->getLogger()->log("Error shadow mapping fs "
						  "shader not found.",
					ELL_WARNING);
			return;
		}
		_shadow_depth_cb = new ShadowDepthShaderCB();

		depth_shader = gpu->addHighLevelShaderMaterial(
				readFile(depth_shader_vs).c_str(), "vertexMain",
				irr::video::EVST_VS_1_1,
				readFile(depth_shader_fs).c_str(), "pixelMain",
				irr::video::EPST_PS_1_2, _shadow_depth_cb);

		if (depth_shader == -1) {
			// upsi, something went wrong loading shader.
			delete _shadow_depth_cb;
			_shadows_enabled = false;
			_device->getLogger()->log(
					"Error compiling shadow mapping shader.",
					ELL_WARNING);
			return;
		}

		// HACK, TODO: investigate this better
		// Grab the material renderer once more so minetest doesn't crash
		// on exit
		_driver->getMaterialRenderer(depth_shader)->grab();
	}

	if (_enable_csm && mixcsm_shader == -1) {
		std::string depth_shader_vs =
				getShaderPath("shadow_shaders", "shadow_pass2.vs");
		if (depth_shader_vs.empty()) {
			_shadows_enabled = false;
			_device->getLogger()->log("Error cascade shadow mapping fs "
						  "shader not found.",
					ELL_WARNING);
			return;
		}

		std::string depth_shader_fs =
				getShaderPath("shadow_shaders", "shadow_pass2.fs");
		if (depth_shader_fs.empty()) {
			_shadows_enabled = false;
			_device->getLogger()->log("Error cascade shadow mapping fs "
						  "shader not found.",
					ELL_WARNING);
			return;
		}
		_shadow_mix_cb = new shadowScreenQuadCB();
		_screen_quad = new shadowScreenQuad();
		mixcsm_shader = gpu->addHighLevelShaderMaterial(
				readFile(depth_shader_vs).c_str(), "vertexMain",
				irr::video::EVST_VS_1_1,
				readFile(depth_shader_fs).c_str(), "pixelMain",
				irr::video::EPST_PS_1_2, _shadow_mix_cb);

		_screen_quad->getMaterial().MaterialType =
				(irr::video::E_MATERIAL_TYPE)mixcsm_shader;

		if (mixcsm_shader == -1) {
			// upsi, something went wrong loading shader.
			delete _shadow_mix_cb;
			delete _screen_quad;
			_shadows_enabled = false;
			_device->getLogger()->log("Error compiling cascade "
						  "shadow mapping shader.",
					ELL_WARNING);
			return;
		}

		// HACK, TODO: investigate this better
		// Grab the material renderer once more so minetest doesn't crash
		// on exit
		_driver->getMaterialRenderer(mixcsm_shader)->grab();
	}
}

std::string ShadowRenderer::readFile(const std::string &path)
{
	std::ifstream is(path.c_str(), std::ios::binary);
	if (!is.is_open())
		return "";
	std::ostringstream tmp_os;
	tmp_os << is.rdbuf();
	return tmp_os.str();
}
