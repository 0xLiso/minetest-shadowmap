#include "client/shadows/dynamicshadowsrender.h"

#include <type_traits>
#include <utility>
#include "settings.h"
#include "filesys.h"
#include "porting.h"
#include "client/shader.h"

#include "client/clientmap.h"

ShadowRenderer::ShadowRenderer(irr::IrrlichtDevice *irrlichtDevice, Client *client) :
		m_device(irrlichtDevice), m_smgr(irrlichtDevice->getSceneManager()),
		m_driver(irrlichtDevice->getVideoDriver()), m_client(client)
{
	m_shadows_enabled = g_settings->getBool("enable_shaders");
	m_shadows_enabled &= g_settings->getBool("enable_dynamic_shadows");
	if (!m_shadows_enabled)
		return;

	m_shadow_strength = g_settings->getFloat("shadow_strength");

	m_shadow_map_max_distance = g_settings->getFloat("shadow_map_max_distance");

	m_shadow_map_texture_size = g_settings->getFloat("shadow_map_texture_size");

	m_shadow_map_texture_32bit = g_settings->getBool("shadow_map_texture_32bit");
	m_shadow_map_colored = g_settings->getBool("shadow_map_color");
	m_shadow_samples = g_settings->getS32("shadow_filters");
	m_shadow_psm = true;
	m_update_delta = g_settings->getFloat("shadow_update_time");
}

ShadowRenderer::~ShadowRenderer()
{
	if (m_shadow_depth_cb)
		delete m_shadow_depth_cb;
	if (m_shadow_mix_cb)
		delete m_shadow_mix_cb;
	m_shadow_node_array.clear();
	m_light_list.clear();
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

void ShadowRenderer::initialize()
{
	bool tempTexFlagMipMaps = m_driver->getTextureCreationFlag(
			irr::video::ETCF_CREATE_MIP_MAPS);
	bool tempTexFlag32 =
			m_driver->getTextureCreationFlag(irr::video::ETCF_ALWAYS_32_BIT);
	m_driver->setTextureCreationFlag(
			irr::video::ETCF_CREATE_MIP_MAPS, tempTexFlagMipMaps);
	m_driver->setTextureCreationFlag(irr::video::ETCF_ALWAYS_32_BIT, tempTexFlag32);

	irr::video::IGPUProgrammingServices *gpu = m_driver->getGPUProgrammingServices();

	// we need glsl
	if (m_shadows_enabled && gpu &&
			m_driver->queryFeature(irr::video::EVDF_ARB_GLSL)) {
		createShaders();
	} else {
		m_shadows_enabled = false;
		m_device->getLogger()->log(
				"Shadows: GLSL Shader not supported on this system.",
				ELL_WARNING);
		return;
	}

	m_texture_format = m_shadow_map_texture_32bit
					   ? irr::video::ECOLOR_FORMAT::ECF_R32F
					   : irr::video::ECOLOR_FORMAT::ECF_R16F;

	m_texture_format_color = m_shadow_map_texture_32bit
						 ? irr::video::ECOLOR_FORMAT::ECF_G32R32F
						 : irr::video::ECOLOR_FORMAT::ECF_G16R16F;
}


float ShadowRenderer::getUpdateDelta() const
{
	return m_update_delta;
}

size_t ShadowRenderer::addDirectionalLight()
{

	m_light_list.emplace_back(DirectionalLight(m_shadow_map_texture_size,
			irr::core::vector3df(0.f, 0.f, 0.f),
			video::SColor(255, 255, 255, 255), m_shadow_map_max_distance));
	return m_light_list.size() - 1;
}

DirectionalLight &ShadowRenderer::getDirectionalLight(irr::u32 index)
{
	return m_light_list[index];
}

size_t ShadowRenderer::getDirectionalLightCount() const
{
	return m_light_list.size();
}
irr::f32 ShadowRenderer::getMaxShadowFar() const
{
	if (!m_light_list.empty()) {
		float wanted_range = m_client->getEnv().getClientMap().getWantedRange();

		float zMax = m_light_list[0].getMaxFarValue() > wanted_range
					     ? wanted_range
					     : m_light_list[0].getMaxFarValue();
		return zMax * MAP_BLOCKSIZE;
	}
	return 0.0f;
}
void ShadowRenderer::addNodeToShadowList(
		irr::scene::ISceneNode *node, E_SHADOW_MODE shadowMode)
{
	m_shadow_node_array.emplace_back(NodeToApply(node, shadowMode));
}

void ShadowRenderer::removeNodeFromShadowList(irr::scene::ISceneNode *node)
{
	for (auto it = m_shadow_node_array.begin(); it != m_shadow_node_array.end();) {
		if (it->node == node) {
			it = m_shadow_node_array.erase(it);
			break;
		} else {
			++it;
		}
	}
}

void ShadowRenderer::setClearColor(irr::video::SColor ClearColor)
{
	m_clear_color = ClearColor;
}

irr::IrrlichtDevice *ShadowRenderer::getIrrlichtDevice()
{
	return m_device;
}

irr::scene::ISceneManager *ShadowRenderer::getSceneManager()
{
	return m_smgr;
}

void ShadowRenderer::update(irr::video::ITexture *outputTarget)
{
	if (!m_shadows_enabled || m_smgr->getActiveCamera() == nullptr) {
		m_smgr->drawAll();
		return;
	}

	if (!shadowMapTextureDynamicObjects) {

		shadowMapTextureDynamicObjects = getSMTexture(
				std::string("shadow_dynamic_") +
						std::to_string(m_shadow_map_texture_size),
				m_texture_format, true);
	}

	if (!shadowMapClientMap) {
		std::string shadowMapName(std::string("shadow_clientmap_") +
					  std::to_string(m_shadow_map_texture_size));

		shadowMapClientMap = getSMTexture(shadowMapName,
				m_shadow_map_colored ? m_texture_format_color
						     : m_texture_format,
				true);
	}

	if (m_shadow_map_colored && !shadowMapTextureColors) {
		shadowMapTextureColors = getSMTexture(
				std::string("shadow_colored_") +
						std::to_string(m_shadow_map_texture_size),
				m_shadow_map_colored ? m_texture_format_color
						     : m_texture_format,
				true);
	}

	// The merge all shadowmaps texture
	if (!shadowMapTextureFinal) {
		irr::video::ECOLOR_FORMAT frt;
		if (m_shadow_map_texture_32bit) {
			if (m_shadow_map_colored)
				frt = irr::video::ECOLOR_FORMAT::ECF_A32B32G32R32F;
			else
				frt = irr::video::ECOLOR_FORMAT::ECF_R32F;
		} else {
			if (m_shadow_map_colored)
				frt = irr::video::ECOLOR_FORMAT::ECF_A16B16G16R16F;
			else
				frt = irr::video::ECOLOR_FORMAT::ECF_R16F;
		}
		shadowMapTextureFinal = getSMTexture(
				std::string("shadowmap_final_") +
						std::to_string(m_shadow_map_texture_size),
				frt, true);
	}

	if (!m_shadow_node_array.empty() && !m_light_list.empty()) {
		// for every directional light:
		for (DirectionalLight &light : m_light_list) {
			// Static shader values.
			m_shadow_depth_cb->MapRes = (f32)m_shadow_map_texture_size;
			m_shadow_depth_cb->MaxFar = (f32)m_shadow_map_max_distance * BS;

			// set the Render Target
			// right now we can only render in usual RTT, not
			// Depth texture is available in irrlicth maybe we
			// should put some gl* fn here

			if (light.should_update_map_shadow) {
				light.should_update_map_shadow = false;

				m_driver->setRenderTarget(shadowMapClientMap, true, true,
						irr::video::SColor(255, 255, 255, 255));
				renderShadowMap(shadowMapClientMap, light);

				if (m_shadow_map_colored) {
					m_driver->setRenderTarget(shadowMapTextureColors,
							true, false,
							irr::video::SColor(255, 255, 255,
									255));
				}
				renderShadowMap(shadowMapTextureColors, light,
						irr::scene::ESNRP_TRANSPARENT);
				m_driver->setRenderTarget(0, false, false);
			}

			// render shadows for the n0n-map objects.
			m_driver->setRenderTarget(shadowMapTextureDynamicObjects, true,
					true, irr::video::SColor(255, 255, 255, 255));
			renderShadowObjects(shadowMapTextureDynamicObjects, light);
			// clear the Render Target
			m_driver->setRenderTarget(0, false, false);

			// in order to avoid too many map shadow renders,
			// we should make a second pass to mix clientmap shadows and
			// entities shadows :(
			m_screen_quad->getMaterial().setTexture(0, shadowMapClientMap);
			// dynamic objs shadow texture.
			if (m_shadow_map_colored) {
				m_screen_quad->getMaterial().setTexture(
						1, shadowMapTextureColors);
			}
			m_screen_quad->getMaterial().setTexture(
					2, shadowMapTextureDynamicObjects);

			m_driver->setRenderTarget(shadowMapTextureFinal, false, false,
					irr::video::SColor(255, 255, 255, 255));
			m_screen_quad->render(m_driver);
			m_driver->setRenderTarget(0, false, false);

		} // end for lights

		// now render the actual MT render pass
		m_driver->setRenderTarget(outputTarget, true, true, m_clear_color);
		m_smgr->drawAll();

		/**/
		if (false) {
			// this is debug, ignore for now.
			m_driver->draw2DImage(shadowMapTextureFinal,
					irr::core::rect<s32>(0, 50, 128, 128 + 50),
					irr::core::rect<s32>(0, 0,
							shadowMapTextureFinal->getSize()
									.Width,
							shadowMapTextureFinal->getSize()
									.Height));

			m_driver->draw2DImage(shadowMapClientMap,
					irr::core::rect<s32>(
							0, 50 + 128, 128, 128 + 50 + 128),
					irr::core::rect<s32>(0, 0,
							shadowMapClientMap->getSize()
									.Width,
							shadowMapClientMap->getSize()
									.Height));
			m_driver->draw2DImage(shadowMapTextureDynamicObjects,
					irr::core::rect<s32>(0, 128 + 50 + 128, 128,
							128 + 50 + 128 + 128),
					irr::core::rect<s32>(0, 0,
							shadowMapTextureDynamicObjects
									->getSize()
									.Width,
							shadowMapTextureDynamicObjects
									->getSize()
									.Height));

			if (m_shadow_map_colored) {

				m_driver->draw2DImage(shadowMapTextureColors,
						irr::core::rect<s32>(128,
								128 + 50 + 128 + 128,
								128 + 128,
								128 + 50 + 128 + 128 +
										128),
						irr::core::rect<s32>(0, 0,
								shadowMapTextureColors
										->getSize()
										.Width,
								shadowMapTextureColors
										->getSize()
										.Height));
			}
		}
		m_driver->setRenderTarget(0, false, false);
	}
}

irr::video::ITexture *ShadowRenderer::get_texture()
{
	return shadowMapTextureFinal;
}

irr::video::ITexture *ShadowRenderer::getSMTexture(const std::string &shadow_map_name,
		irr::video::ECOLOR_FORMAT texture_format, bool force_creation)
{
	irr::video::ITexture *shadowMapTexture = nullptr;
	if (force_creation) {
		shadowMapTexture = m_driver->addRenderTargetTexture(
				irr::core::dimension2du(m_shadow_map_texture_size,
						m_shadow_map_texture_size),
				shadow_map_name.c_str(), texture_format);
		return shadowMapTexture;
	}
	shadowMapTexture = m_driver->getTexture(shadow_map_name.c_str());

	/*if (shadowMapTexture == nullptr) {

		shadowMapTexture = m_driver->addRenderTargetTexture(
				irr::core::dimension2du(m_shadow_map_texture_size,
						m_shadow_map_texture_size),
				shadow_map_name.c_str(), texture_format);
	}*/

	return shadowMapTexture;
}

void ShadowRenderer::renderShadowMap(irr::video::ITexture *target,
		DirectionalLight &light, irr::scene::E_SCENE_NODE_RENDER_PASS pass)
{

	m_driver->setTransform(irr::video::ETS_VIEW, light.getViewMatrix());
	m_driver->setTransform(irr::video::ETS_PROJECTION, light.getProjectionMatrix());

	/// Render all shadow casters
	///
	for (const auto &shadow_node : m_shadow_node_array) {
		// If it's the Map, we have to handle it
		// differently.
		// F$%�ck irrlicht and it�s u8 chars :/
		if (std::string(shadow_node.node->getName()) == "ClientMap") {

			ClientMap *map_node = static_cast<ClientMap *>(shadow_node.node);

			// lets go with the actual render.

			irr::video::SMaterial material;
			if (map_node->getMaterialCount() > 0) {
				// we only want the first
				// material, which is the
				// one with the albedo
				// info ;)
				material = map_node->getMaterial(0);
			}


			material.BackfaceCulling = false;
			material.FrontfaceCulling = true;
			material.PolygonOffsetFactor = 4.0f;
			material.PolygonOffsetDirection = video::EPO_BACK;
			//material.PolygonOffsetDepthBias = 1.0f/4.0f;
			//material.PolygonOffsetSlopeScale = -1.f;

			if (m_shadow_map_colored && pass != irr::scene::ESNRP_SOLID) {
				material.MaterialType = (irr::video::E_MATERIAL_TYPE)
						depth_shader_trans;
			} else {
				material.MaterialType =
						(irr::video::E_MATERIAL_TYPE)depth_shader;
			}

			map_node->OnAnimate(m_device->getTimer()->getTime());

			m_driver->setTransform(irr::video::ETS_WORLD,
					map_node->getAbsoluteTransformation());

			map_node->renderMapShadows(m_driver, material, pass,
					light.getPosition(), light.getDirection(),
					m_shadow_map_max_distance * BS, false);
			break;
		} // end clientMap render
	}
}

void ShadowRenderer::renderShadowObjects(
		irr::video::ITexture *target, DirectionalLight &light)
{

	m_driver->setTransform(irr::video::ETS_VIEW, light.getViewMatrix());
	m_driver->setTransform(irr::video::ETS_PROJECTION, light.getProjectionMatrix());

	for (const auto &shadow_node : m_shadow_node_array) {
		// we only take care of the shadow casters
		if (shadow_node.shadowMode == ESM_RECEIVE || !shadow_node.node ||
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
			BufferMaterialList.push_back(
					shadow_node.node->getMaterial(m).MaterialType);

			auto &current_mat = shadow_node.node->getMaterial(m);
			current_mat.setTexture(3, shadowMapTextureFinal);

			current_mat.MaterialType =
					(irr::video::E_MATERIAL_TYPE)depth_shader;
			/**/ BufferMaterialCullingList.push_back(std::make_pair<bool, bool>(
					current_mat.BackfaceCulling ? true : false,
					current_mat.FrontfaceCulling ? true : false));
			current_mat.BackfaceCulling = true;
			current_mat.FrontfaceCulling = false;
			current_mat.PolygonOffsetFactor = 1.0f/2048.0f;
			current_mat.PolygonOffsetDirection = video::EPO_BACK;
			//current_mat.PolygonOffsetDepthBias = 1.0 * 2.8e-6;
			//current_mat.PolygonOffsetSlopeScale = -1.f;
		}

		m_driver->setTransform(irr::video::ETS_WORLD,
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
	irr::video::IGPUProgrammingServices *gpu = m_driver->getGPUProgrammingServices();

	if (depth_shader == -1) {
		std::string depth_shader_vs =
				getShaderPath("shadow_shaders", "shadow_pass1.vs");
		if (depth_shader_vs.empty()) {
			m_shadows_enabled = false;
			m_device->getLogger()->log("Error shadow mapping vs "
						   "shader not found.",
					ELL_WARNING);
			return;
		}
		std::string depth_shader_fs =
				getShaderPath("shadow_shaders", "shadow_pass1.fs");
		if (depth_shader_fs.empty()) {
			m_shadows_enabled = false;
			m_device->getLogger()->log("Error shadow mapping fs "
						   "shader not found.",
					ELL_WARNING);
			return;
		}
		m_shadow_depth_cb = new ShadowDepthShaderCB();

		depth_shader = gpu->addHighLevelShaderMaterial(
				readFile(depth_shader_vs).c_str(), "vertexMain",
				irr::video::EVST_VS_1_1,
				readFile(depth_shader_fs).c_str(), "pixelMain",
				irr::video::EPST_PS_1_2, m_shadow_depth_cb);

		if (depth_shader == -1) {
			// upsi, something went wrong loading shader.
			delete m_shadow_depth_cb;
			m_shadows_enabled = false;
			m_device->getLogger()->log(
					"Error compiling shadow mapping shader.",
					ELL_WARNING);
			return;
		}

		// HACK, TODO: investigate this better
		// Grab the material renderer once more so minetest doesn't crash
		// on exit
		m_driver->getMaterialRenderer(depth_shader)->grab();
	}

	if (true) { //_enable_csm && mixcsm_shader == -1) {
		std::string depth_shader_vs =
				getShaderPath("shadow_shaders", "shadow_pass2.vs");
		if (depth_shader_vs.empty()) {
			m_shadows_enabled = false;
			m_device->getLogger()->log("Error cascade shadow mapping fs "
						   "shader not found.",
					ELL_WARNING);
			return;
		}

		std::string depth_shader_fs =
				getShaderPath("shadow_shaders", "shadow_pass2.fs");
		if (depth_shader_fs.empty()) {
			m_shadows_enabled = false;
			m_device->getLogger()->log("Error cascade shadow mapping fs "
						   "shader not found.",
					ELL_WARNING);
			return;
		}
		m_shadow_mix_cb = new shadowScreenQuadCB();
		m_screen_quad = new shadowScreenQuad();
		mixcsm_shader = gpu->addHighLevelShaderMaterial(
				readFile(depth_shader_vs).c_str(), "vertexMain",
				irr::video::EVST_VS_1_1,
				readFile(depth_shader_fs).c_str(), "pixelMain",
				irr::video::EPST_PS_1_2, m_shadow_mix_cb);

		m_screen_quad->getMaterial().MaterialType =
				(irr::video::E_MATERIAL_TYPE)mixcsm_shader;

		if (mixcsm_shader == -1) {
			// upsi, something went wrong loading shader.
			delete m_shadow_mix_cb;
			delete m_screen_quad;
			m_shadows_enabled = false;
			m_device->getLogger()->log("Error compiling cascade "
						   "shadow mapping shader.",
					ELL_WARNING);
			return;
		}

		// HACK, TODO: investigate this better
		// Grab the material renderer once more so minetest doesn't crash
		// on exit
		m_driver->getMaterialRenderer(mixcsm_shader)->grab();
	}

	if (m_shadow_map_colored && depth_shader_trans == -1) {
		std::string depth_shader_vs =
				getShaderPath("shadow_shaders", "shadow_pass1_trans.vs");
		if (depth_shader_vs.empty()) {
			m_shadows_enabled = false;
			m_device->getLogger()->log("Error shadow mapping vs "
						   "shader not found.",
					ELL_WARNING);
			return;
		}
		std::string depth_shader_fs =
				getShaderPath("shadow_shaders", "shadow_pass1_trans.fs");
		if (depth_shader_fs.empty()) {
			m_shadows_enabled = false;
			m_device->getLogger()->log("Error shadow mapping fs "
						   "shader not found.",
					ELL_WARNING);
			return;
		}
		m_shadow_depth_trans_cb = new ShadowDepthShaderCB();

		depth_shader_trans = gpu->addHighLevelShaderMaterial(
				readFile(depth_shader_vs).c_str(), "vertexMain",
				irr::video::EVST_VS_1_1,
				readFile(depth_shader_fs).c_str(), "pixelMain",
				irr::video::EPST_PS_1_2, m_shadow_depth_trans_cb);

		if (depth_shader_trans == -1) {
			// upsi, something went wrong loading shader.
			delete m_shadow_depth_trans_cb;
			m_shadow_map_colored = false;
			m_shadows_enabled = false;
			m_device->getLogger()->log(
					"Error compiling colored shadow mapping shader.",
					ELL_WARNING);
			return;
		}

		// HACK, TODO: investigate this better
		// Grab the material renderer once more so minetest doesn't crash
		// on exit
		m_driver->getMaterialRenderer(depth_shader_trans)->grab();
	}
}

std::string ShadowRenderer::readFile(const std::string &path)
{
	std::ifstream is(path.c_str(), std::ios::binary);
	if (!is.is_open())
		return "";
	std::ostringstream tmp_os;
	if (m_shadow_map_colored) {
		tmp_os << "#define COLORED_SHADOWS 1\n";
	}

	if (m_shadow_psm) {
		tmp_os << "#define SHADOWS_PSM 1\n";
	}
	tmp_os << is.rdbuf();
	return tmp_os.str();
}
