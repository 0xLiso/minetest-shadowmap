#include "client/shadows/dynamicshadowsrender.h"

#include <type_traits>
#include <utility>
#include "settings.h"
#include "filesys.h"
#include "porting.h"
#include "client/shader.h"

#include "client/clientmap.h"

ShadowRenderer::ShadowRenderer(irr::IrrlichtDevice *irrlichtDevice, Client *client) :
	_device(irrlichtDevice), 
	_smgr(irrlichtDevice->getSceneManager()),
	_driver(irrlichtDevice->getVideoDriver()), 
	_client(client) {
	_shadows_enabled = g_settings->getBool("enable_shaders");
	_shadows_enabled &= g_settings->getBool("enable_dynamic_shadows");
	if (!_shadows_enabled)
		return;

	_shadow_strength = g_settings->getFloat("shadow_strength");

	_shadow_map_max_distance = g_settings->getFloat("shadow_map_max_distance");

	_shadow_map_texture_size = g_settings->getFloat("shadow_map_texture_size");

	_shadow_map_texture_32bit = g_settings->getBool("shadow_map_texture_32bit");
	_shadow_map_colored = g_settings->getBool("shadow_map_color");
	_shadow_samples = g_settings->getS32("shadow_filters");
	_shadow_psm = g_settings->getBool("shadow_psm");


}

ShadowRenderer::~ShadowRenderer() {
	if (_shadow_depth_cb)
		delete _shadow_depth_cb;
	if (_shadow_mix_cb)
		delete _shadow_mix_cb;
	ShadowNodeArray.clear();
	_light_list.clear();
	// we don't have to delete the textures in renderTargets
	/*
	if (shadowMapTextureDynamicObjects) {
		delete shadowMapTextureDynamicObjects;
		shadowMapTextureDynamicObjects = nullptr;
	}

	if (shadowMapTextureFinal) {
		delete shadowMapTextureFinal;
		shadowMapTextureFinal = nullptr;
	}
	if (shadowMapTextureColors) {
		delete shadowMapTextureColors;
		shadowMapTextureColors = nullptr;
	}
	if (shadowMapClientMap) {
		delete shadowMapClientMap;
		shadowMapClientMap = nullptr;
	}
	*/
}

void ShadowRenderer::initialize() {
	bool tempTexFlagMipMaps = _driver->getTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS);
	bool tempTexFlag32 = _driver->getTextureCreationFlag(irr::video::ETCF_ALWAYS_32_BIT);
	_driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, tempTexFlagMipMaps);
	_driver->setTextureCreationFlag(irr::video::ETCF_ALWAYS_32_BIT, tempTexFlag32);

	irr::video::IGPUProgrammingServices *gpu = _driver->getGPUProgrammingServices();

	// we need glsl
	if (_shadows_enabled && gpu &&
		_driver->queryFeature( irr::video::EVDF_ARB_GLSL))
	{
		createShaders();
	} else 
	{
		_shadows_enabled = false;
		_device->getLogger()->log("Shadows: GLSL Shader not supported on this system.",	ELL_WARNING);
		return;
	}


	_texture_format = _shadow_map_texture_32bit
					  ? irr::video::ECOLOR_FORMAT::ECF_R32F
					  : irr::video::ECOLOR_FORMAT::ECF_R16F;

	_texture_format_color = _shadow_map_texture_32bit
						? irr::video::ECOLOR_FORMAT::ECF_G32R32F
						: irr::video::ECOLOR_FORMAT::ECF_G16R16F;

	
}

size_t ShadowRenderer::addDirectionalLight() {

	_light_list.emplace_back(DirectionalLight(_shadow_map_texture_size,
										   irr::core::vector3df(0.f, 0.f, 0.f),
										   video::SColor(255, 255, 255, 255), _shadow_map_max_distance));
	return _light_list.size() - 1;
}

DirectionalLight &ShadowRenderer::getDirectionalLight(irr::u32 index) {
	return _light_list[index];
}

size_t ShadowRenderer::getDirectionalLightCount() const {
	return _light_list.size();
}
irr::f32 ShadowRenderer::getMaxShadowFar() const
{
	if (!_light_list.empty()) {
		float wanted_range = _client->getEnv().getClientMap().getWantedRange();

		float zMax= _light_list[0].getMaxFarValue() > wanted_range
						 ? wanted_range
						 : _light_list[0].getMaxFarValue();
		return zMax * MAP_BLOCKSIZE;
	}
	return 0.0f;
}
void ShadowRenderer::addNodeToShadowList(irr::scene::ISceneNode *node, E_SHADOW_MODE shadowMode) {
	ShadowNodeArray.emplace_back(NodeToApply(node, shadowMode));
}

void ShadowRenderer::removeNodeFromShadowList(irr::scene::ISceneNode *node) {
	for (auto it = ShadowNodeArray.begin(); it != ShadowNodeArray.end();) {
		if (it->node == node) {
			it = ShadowNodeArray.erase(it);
			break;
		} else {
			++it;
		}
	}
}

void ShadowRenderer::setClearColor(irr::video::SColor ClearColor) {
	_clear_color = ClearColor;
}

irr::IrrlichtDevice *ShadowRenderer::getIrrlichtDevice() {
	return _device;
}

irr::scene::ISceneManager *ShadowRenderer::getSceneManager() {
	return _smgr;
}

void ShadowRenderer::update(irr::video::ITexture *outputTarget) {
	if (!_shadows_enabled || _smgr->getActiveCamera() == nullptr) {
		_smgr->drawAll();
		return;
	}

	if (!shadowMapTextureDynamicObjects) {

		shadowMapTextureDynamicObjects = getSMTexture(
											 std::string("shadow_dynamic_") +
											 std::to_string(_shadow_map_texture_size),
											 _texture_format, true);
	}

	if (!shadowMapClientMap) {
		std::string shadowMapName(
			std::string("shadow_clientmap_") +
			std::to_string(_shadow_map_texture_size));

		shadowMapClientMap = getSMTexture( shadowMapName,
			_shadow_map_colored ? _texture_format_color : _texture_format, true) ;
	}

	if (_shadow_map_colored && !shadowMapTextureColors) {
		shadowMapTextureColors = getSMTexture(
				std::string("shadow_colored_") + std::to_string(_shadow_map_texture_size),
				_shadow_map_colored ? _texture_format_color : _texture_format, true);
	}

	// The merge all shadowmaps texture
	if (!shadowMapTextureFinal) {
		irr::video::ECOLOR_FORMAT frt;
		if (_shadow_map_texture_32bit) {
			if (_shadow_map_colored)
				frt = irr::video::ECOLOR_FORMAT::ECF_A32B32G32R32F;
			else
				frt = irr::video::ECOLOR_FORMAT::ECF_R32F;
		} else {
			if (_shadow_map_colored)
				frt = irr::video::ECOLOR_FORMAT::ECF_A16B16G16R16F;
			else
				frt = irr::video::ECOLOR_FORMAT::ECF_R16F;
		}
		shadowMapTextureFinal = getSMTexture(
									std::string("shadowmap_final_") +
									std::to_string(_shadow_map_texture_size),
									frt,true);
	}
	


	if (!ShadowNodeArray.empty() && !_light_list.empty()) {
		// for every directional light:
		for (DirectionalLight &light : _light_list) {
			// Static shader values.
			_shadow_depth_cb->MapRes = (f32)_shadow_map_texture_size;
			_shadow_depth_cb->MaxFar = (f32)_shadow_map_max_distance * BS;

			// set the Render Target
			// right now we can only render in usual RTT, not
			// Depth texture is available in irrlicth maybe we
			// should put some gl* fn here

			if (light.should_update_map_shadow) {
				light.should_update_map_shadow = false;

				_driver->setRenderTarget(shadowMapClientMap, true, true,
				irr::video::SColor(255, 255, 255, 255));
				renderShadowMap(shadowMapClientMap, light);

				if (_shadow_map_colored) {
					_driver->setRenderTarget(shadowMapTextureColors, true,false,
						irr::video::SColor(255, 255, 255, 255));
				}
				renderShadowMap(shadowMapTextureColors, light,irr::scene::ESNRP_TRANSPARENT);
				_driver->setRenderTarget(0, false, false);
			}

			// render shadows for the n0n-map objects.
			_driver->setRenderTarget(shadowMapTextureDynamicObjects, true, true,
				irr::video::SColor(255, 255, 255, 255));
			renderShadowObjects(shadowMapTextureDynamicObjects, light);
			// clear the Render Target
			_driver->setRenderTarget(0, false, false);

			// in order to avoid too many map shadow renders,
			// we should make a second pass to mix clientmap shadows and entities
			// shadows :(
			_screen_quad->getMaterial().setTexture(0, shadowMapClientMap);
			// dynamic objs shadow texture.
			if (_shadow_map_colored) {
				_screen_quad->getMaterial().setTexture(
					1, shadowMapTextureColors);
			}
			_screen_quad->getMaterial().setTexture(
				2, shadowMapTextureDynamicObjects);

			_driver->setRenderTarget(shadowMapTextureFinal, false, false,
				irr::video::SColor(255, 255, 255, 255));
			_screen_quad->render(_driver);
			_driver->setRenderTarget(0, false, false);

		} // end for lights

		// now render the actual MT render pass
		_driver->setRenderTarget(outputTarget, true, true, _clear_color);
		_smgr->drawAll();

		/**/
		if (false) {
			// this is debug, ignore for now.
			_driver->draw2DImage(shadowMapTextureFinal,
								 irr::core::rect<s32>(0, 50, 128, 128 + 50),
								 irr::core::rect<s32>(0, 0,
										 shadowMapTextureFinal->getSize().Width,
										 shadowMapTextureFinal->getSize().Height));

			_driver->draw2DImage(shadowMapClientMap,
								 irr::core::rect<s32>(0, 50 + 128, 128, 128 + 50 + 128),
								 irr::core::rect<s32>(0, 0,shadowMapClientMap->getSize().Width,
									shadowMapClientMap->getSize().Height));
			_driver->draw2DImage(shadowMapTextureDynamicObjects,
								 irr::core::rect<s32>(
									 0, 128 + 50 + 128, 128, 128 + 50 + 128 + 128),
								 irr::core::rect<s32>(0, 0,
										 shadowMapTextureDynamicObjects->getSize()
										 .Width,
										 shadowMapTextureDynamicObjects->getSize()
										 .Height));

			if (_shadow_map_colored) {

				_driver->draw2DImage(shadowMapTextureColors,
									 irr::core::rect<s32>(128, 128 + 50 + 128 + 128,
											 128 + 128,
											 128 + 50 + 128 + 128 + 128),
									 irr::core::rect<s32>(0, 0,
											 shadowMapTextureColors->getSize()
											 .Width,
											 shadowMapTextureColors->getSize()
											 .Height));
			}
		}
		_driver->setRenderTarget(0, false, false);
	}
}

irr::video::ITexture *ShadowRenderer::get_texture() {
	return shadowMapTextureFinal;
}

irr::video::ITexture *ShadowRenderer::getSMTexture(const std::string &shadowMapName,
		irr::video::ECOLOR_FORMAT texture_format, bool forcecreation) {
	irr::video::ITexture *shadowMapTexture = nullptr;
	if (forcecreation) {
		shadowMapTexture = _driver->addRenderTargetTexture(
							   irr::core::dimension2du(_shadow_map_texture_size,
									   _shadow_map_texture_size),
							   shadowMapName.c_str(), texture_format);
		return shadowMapTexture;
	}
	shadowMapTexture = _driver->getTexture(shadowMapName.c_str());

	/*if (shadowMapTexture == nullptr) {

		shadowMapTexture = _driver->addRenderTargetTexture(
				irr::core::dimension2du(_shadow_map_texture_size,
						_shadow_map_texture_size),
				shadowMapName.c_str(), texture_format);
	}*/

	return shadowMapTexture;
}

void ShadowRenderer::renderShadowMap(irr::video::ITexture *target,
									   DirectionalLight &light, 
									   irr::scene::E_SCENE_NODE_RENDER_PASS pass) {

	_driver->setTransform(irr::video::ETS_VIEW, light.getViewMatrix());
	_driver->setTransform(irr::video::ETS_PROJECTION, light.getProjectionMatrix());
	
	/// Render all shadow casters
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


			// we HAVE TO render back and front faces
			// so we disable both culling...
			 material.BackfaceCulling = false;
			 material.FrontfaceCulling = false;
			//material.PolygonOffsetFactor = -1;
			//material.PolygonOffsetDirection = video::EPO_BACK;
			material.PolygonOffsetDepthBias = 2.0 * 4.8e-7;
			material.PolygonOffsetSlopeScale = -1.f;

			if (_shadow_map_colored && pass != irr::scene::ESNRP_SOLID) {
				material.MaterialType = (irr::video::E_MATERIAL_TYPE)depth_shader_trans;
			} else {
				material.MaterialType = (irr::video::E_MATERIAL_TYPE)depth_shader;
			}

			map_node->OnAnimate(_device->getTimer()->getTime());

			_driver->setTransform(irr::video::ETS_WORLD,
								  map_node->getAbsoluteTransformation());

			map_node->renderMapShadows(_driver, material, pass,
									   light.getPosition(),
									   light.getDirection(),
									   _shadow_map_max_distance * BS, false);
			break;
		} // end clientMap render
	}

}

void ShadowRenderer::renderShadowObjects(
	irr::video::ITexture *target, DirectionalLight &light) {


	_driver->setTransform(irr::video::ETS_VIEW,
						  light.getViewMatrix( ));
	_driver->setTransform(irr::video::ETS_PROJECTION,
						  light.getProjectionMatrix( ));


	for (const auto &shadow_node : ShadowNodeArray) {
		// we only take care of the shadow casters
		if (shadow_node.shadowMode == ESM_RECEIVE || 
			!shadow_node.node ||
			std::string(shadow_node.node->getName()) == "ClientMap")
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
			BufferMaterialList.push_back(shadow_node.node->getMaterial(m).MaterialType);

			auto &current_mat = shadow_node.node->getMaterial(m);
			current_mat.setTexture(3, shadowMapTextureFinal);

			current_mat.MaterialType =
				(irr::video::E_MATERIAL_TYPE)depth_shader;
			/**/ BufferMaterialCullingList.push_back(std::make_pair<bool, bool>(
													current_mat.BackfaceCulling ? true : false,
													current_mat.FrontfaceCulling ? true : false));
			current_mat.BackfaceCulling = false;
			current_mat.FrontfaceCulling = false;
			
			current_mat.PolygonOffsetDepthBias = 2.0 * 4.8e-7;
			current_mat.PolygonOffsetSlopeScale = -1.f;
		}

		_driver->setTransform(irr::video::ETS_WORLD,
							  shadow_node.node->getAbsoluteTransformation());
		shadow_node.node->render();

		// restore the material.

		for (u32 m = 0; m < n_node_materials; m++) {

			auto &current_mat = shadow_node.node->getMaterial(m);
			current_mat.MaterialType = (irr::video::E_MATERIAL_TYPE)
									   BufferMaterialList[m];
			/**/
			current_mat.BackfaceCulling = BufferMaterialCullingList[m].first;
			current_mat.FrontfaceCulling =
				BufferMaterialCullingList[m].second;
				
			
		}

	} // end for caster shadow nodes

}

void ShadowRenderer::mixShadowsQuad() {
}

/*
 * @Liso's disclaimer ;) This function loads the Shadow Mapping Shaders.
 * I used a custom loader because I couldn't figure out how to use the base
 * Shaders system with custom IShaderConstantSetCallBack without messing up the
 * code too much. If anyone knows how to integrate this with the standard MT
 * shaders, please feel free to change it.
 */

void ShadowRenderer::createShaders() {
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

	if (true) { //_enable_csm && mixcsm_shader == -1) {
		std::string depth_shader_vs = getShaderPath("shadow_shaders", "shadow_pass2.vs");
		if (depth_shader_vs.empty()) {
			_shadows_enabled = false;
			_device->getLogger()->log("Error cascade shadow mapping fs "
									  "shader not found.",
									  ELL_WARNING);
			return;
		}

		std::string depth_shader_fs = getShaderPath("shadow_shaders", "shadow_pass2.fs");
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



	if (_shadow_map_colored && depth_shader_trans == -1) {
		std::string depth_shader_vs =
			getShaderPath("shadow_shaders", "shadow_pass1_trans.vs");
		if (depth_shader_vs.empty()) {
			_shadows_enabled = false;
			_device->getLogger()->log("Error shadow mapping vs "
									  "shader not found.",
									  ELL_WARNING);
			return;
		}
		std::string depth_shader_fs =
			getShaderPath("shadow_shaders", "shadow_pass1_trans.fs");
		if (depth_shader_fs.empty()) {
			_shadows_enabled = false;
			_device->getLogger()->log("Error shadow mapping fs "
									  "shader not found.",
									  ELL_WARNING);
			return;
		}
		_shadow_depth_trans_cb = new ShadowDepthShaderCB();

		depth_shader_trans = gpu->addHighLevelShaderMaterial(
								 readFile(depth_shader_vs).c_str(), "vertexMain",
								 irr::video::EVST_VS_1_1,
								 readFile(depth_shader_fs).c_str(), "pixelMain",
								 irr::video::EPST_PS_1_2, _shadow_depth_trans_cb);

		if (depth_shader_trans == -1) {
			// upsi, something went wrong loading shader.
			delete _shadow_depth_trans_cb;
			_shadow_map_colored = false;
			_shadows_enabled = false;
			_device->getLogger()->log(
				"Error compiling colored shadow mapping shader.",
				ELL_WARNING);
			return;
		}

		// HACK, TODO: investigate this better
		// Grab the material renderer once more so minetest doesn't crash
		// on exit
		_driver->getMaterialRenderer(depth_shader_trans)->grab();
	}


}

std::string ShadowRenderer::readFile(const std::string &path) {
	std::ifstream is(path.c_str(), std::ios::binary);
	if (!is.is_open())
		return "";
	std::ostringstream tmp_os;
	if (_shadow_map_colored) {
		tmp_os << "#define COLORED_SHADOWS 1\n";
	}

	if (_shadow_psm) {
		tmp_os << "#define SHADOWS_PSM 1\n";
	}
	tmp_os << is.rdbuf();
	return tmp_os.str();
}
