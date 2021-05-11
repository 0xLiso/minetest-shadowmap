#pragma once

#include <string>
#include <vector>
#include <irrlichttypes.h>

#include "client/shadows/dynamicshadows.h"
#include "client/shadows/shadowsshadercallbacks.h"
#include "client/shadows/shadowsScreenQuad.h"

enum E_SHADOW_MODE : u8
{
	ESM_RECEIVE = 0,
	ESM_BOTH,
};

struct NodeToApply
{
	NodeToApply(irr::scene::ISceneNode *n,
			E_SHADOW_MODE m = E_SHADOW_MODE::ESM_BOTH) :
			node(n),
			shadowMode(m){};
	bool operator<(const NodeToApply &other) const { return node < other.node; };

	irr::scene::ISceneNode *node;

	E_SHADOW_MODE shadowMode{E_SHADOW_MODE::ESM_BOTH};
	bool dirty{false};
};

class ShadowRenderer
{
public:
	ShadowRenderer(irr::IrrlichtDevice *irrlichtDevice, Client *client);

	~ShadowRenderer();

	void initialize();

	/// Adds a directional light shadow map (Usually just one (the sun) except in
	/// Tattoine ).
	size_t addDirectionalLight();
	DirectionalLight &getDirectionalLight(irr::u32 index = 0);
	size_t getDirectionalLightCount() const;
	irr::f32 getMaxShadowFar() const;
	/// Adds a shadow to the scene node.
	/// The shadow mode can be ESM_BOTH, or ESM_RECEIVE.
	/// ESM_BOTH casts and receives shadows
	/// ESM_RECEIVE only receives but does not cast shadows.
	///
	void addNodeToShadowList(irr::scene::ISceneNode *node,
			E_SHADOW_MODE shadowMode = ESM_BOTH);
	void removeNodeFromShadowList(irr::scene::ISceneNode *node);

	void setClearColor(irr::video::SColor ClearColor);

	/// Returns the device that ShadowRenderer was initialized with.
	irr::IrrlichtDevice *getIrrlichtDevice();

	irr::scene::ISceneManager *getSceneManager();
	void update(irr::video::ITexture *outputTarget = nullptr);

	irr::video::ITexture *get_texture();

	bool is_active() const { return m_shadows_enabled; }
	void setTimeOfDay(float isDay) { m_time_day = isDay; };

	s32 getShadowSamples() const { return m_shadow_samples; }
	float getShadowStrengh() const { return m_shadow_strength; }
	float getTimeofDay() const { return m_time_day; }

private:
	irr::video::ITexture *getSMTexture(const std::string &shadow_map_name,
			irr::video::ECOLOR_FORMAT texture_format,
			bool force_creation = false);

	void renderShadowMap(irr::video::ITexture *target, DirectionalLight &light,
			irr::scene::E_SCENE_NODE_RENDER_PASS pass =
					irr::scene::ESNRP_SOLID);
	void renderShadowObjects(irr::video::ITexture *target, DirectionalLight &light);
	void mixShadowsQuad();

	// a bunch of variables
	irr::IrrlichtDevice *m_device{nullptr};
	irr::scene::ISceneManager *m_smgr{nullptr};
	irr::video::IVideoDriver *m_driver{nullptr};
	Client *m_client{nullptr};
	irr::core::dimension2du _screenRTT_resolution;
	irr::video::ITexture *shadowMapClientMap{nullptr};
	irr::video::ITexture *shadowMapTextureFinal{nullptr};
	irr::video::ITexture *shadowMapTextureDynamicObjects{nullptr};
	irr::video::ITexture *shadowMapTextureColors{nullptr};
	bool _use_32bit_depth{false};
	irr::video::SColor m_clear_color{0x0};

	std::vector<DirectionalLight> m_light_list;
	std::vector<NodeToApply> m_shadow_node_array;

	float m_shadow_strength{0.25f};
	float m_shadow_map_max_distance{4096.0f}; // arbitrary 4096 blocks
	float m_shadow_map_texture_size{2048.0f};
	float m_time_day{false};
	int m_shadow_samples{4};
	bool m_shadow_map_texture_32bit{true};
	bool m_shadows_enabled{false};
	bool m_shadow_map_colored{false};
	bool m_shadow_psm{false};

	irr::video::ECOLOR_FORMAT m_texture_format{irr::video::ECOLOR_FORMAT::ECF_R16F};
	irr::video::ECOLOR_FORMAT m_texture_format_color{
			irr::video::ECOLOR_FORMAT::ECF_R16G16};

	// Shadow Shader stuff

	void createShaders();
	std::string readFile(const std::string &path);

	irr::s32 depth_shader{-1};
	irr::s32 depth_shader_trans{-1};
	irr::s32 mixcsm_shader{-1};

	ShadowDepthShaderCB *m_shadow_depth_cb{nullptr};
	ShadowDepthShaderCB *m_shadow_depth_trans_cb{nullptr};

	shadowScreenQuad *m_screen_quad{nullptr};
	shadowScreenQuadCB *m_shadow_mix_cb{nullptr};
};
