#pragma once

#include <string>
#include <vector>
#include <irrlicht.h>

#include "client/shadows/dynamicshadows.h"
#include "client/shadows/shadowsshadercallbacks.h"
#include "client/shadows/shadowsScreenQuad.h"

enum E_SHADOW_MODE : u8 {
    ESM_RECEIVE = 0,
    ESM_CAST,
    ESM_BOTH,
    ESM_EXCLUDE,
    ESM_COUNT
};


enum E_SHADOW_TEXTURE : u8 {
    SM_CLIENTMAP0 = 0,
    SM_CLIENTMAP1,
    SM_CLIENTMAP2
};


struct NodeToApply {
    NodeToApply(irr::scene::ISceneNode *n,
                E_SHADOW_MODE m = E_SHADOW_MODE::ESM_BOTH) :
        node(n), shadowMode(m) {};
    bool operator<(const NodeToApply &other) const { return node < other.node; };

    irr::scene::ISceneNode *node;

    E_SHADOW_MODE shadowMode{E_SHADOW_MODE::ESM_BOTH};
    bool dirty{false};
};


class ShadowRenderer {
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
    /// The shadow mode can be ESM_BOTH, ESM_CAST, or ESM_RECEIVE.
    /// ESM_BOTH casts and receives shadows
    /// ESM_CAST only casts shadows, and is unaffected by shadows.
    /// ESM_RECEIVE only receives but does not cast shadows.
    ///
    void addNodeToShadowList(irr::scene::ISceneNode *node,
                             E_SHADOW_MODE shadowMode = ESM_BOTH);
    void removeNodeFromShadowList(irr::scene::ISceneNode *node);

    void setClearColor(irr::video::SColor ClearColor);

    /// Returns the device that this ShadowRenderer was initialized with.
    irr::IrrlichtDevice *getIrrlichtDevice();

    irr::scene::ISceneManager *getSceneManager();
    void update(irr::video::ITexture *outputTarget = nullptr);

    irr::video::ITexture *get_texture();

    bool is_active() const { return _shadows_enabled; }
    void setTimeOfDay(float isDay) { _time_day = isDay ; };


    s32 getShadowSamples() const { return _shadow_samples; }
    float getShadowStrengh() const { return _shadow_strength; }
    float getTimeofDay() const { return _time_day; }

  private:
    irr::video::ITexture *getSMTexture(
        const std::string &shadowMapName, irr::video::ECOLOR_FORMAT texture_format,
        bool forcecreation = false);

    void renderShadowSplit(irr::video::ITexture *target, DirectionalLight &light,
                           int nSplit,
                           irr::scene::E_SCENE_NODE_RENDER_PASS pass = irr::scene::ESNRP_SOLID);
    void renderShadowObjects(irr::video::ITexture *target, DirectionalLight &light);
    void mixShadowsQuad();



    //a bunch of variables
    irr::IrrlichtDevice *_device{nullptr};
    irr::scene::ISceneManager *_smgr{nullptr};
    irr::video::IVideoDriver *_driver{nullptr};    
    Client *_client{nullptr};
    irr::core::dimension2du _screenRTT_resolution;
    irr::core::array<irr::video::ITexture *> renderTargets;
    irr::video::ITexture *shadowMapTextureFinal{nullptr};
    irr::video::ITexture *shadowMapTextureDynamicObjects{nullptr};
    irr::video::ITexture *shadowMapTextureColors{nullptr};
    bool _use_32bit_depth{false};
    irr::video::SColor _clear_color{0x0};

    std::vector<DirectionalLight> _light_list;
    std::vector<NodeToApply> ShadowNodeArray;




    float _shadow_strength{0.25f};
    float _shadow_map_max_distance{4096.0f}; //arbitrary 4096 blocks
    float _shadow_map_texture_size{2048.0f};
    float _time_day{false};
    int _shadow_samples{4};
    bool _shadow_map_texture_32bit{true};
    bool _shadows_enabled{false};
    bool _shadow_map_colored{false};
    bool _shadow_psm{false};

    irr::video::ECOLOR_FORMAT _texture_format{irr::video::ECOLOR_FORMAT::ECF_R16F};
    irr::video::ECOLOR_FORMAT _texture_format_color{irr::video::ECOLOR_FORMAT::ECF_R16G16};

    
    // Shadow Shader stuff

    void createShaders();
    std::string readFile(const std::string &path);

    irr::s32 depth_shader{ -1};
    irr::s32 depth_shader_trans{ -1};
    irr::s32 mixcsm_shader{ -1};
    irr::s32 _nSplits{1};
    ShadowDepthShaderCB *_shadow_depth_cb{nullptr};
    ShadowDepthShaderCB *_shadow_depth_trans_cb{nullptr};


    shadowScreenQuad *_screen_quad{nullptr};
    shadowScreenQuadCB *_shadow_mix_cb{nullptr};
};
