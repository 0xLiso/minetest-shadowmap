#pragma once

#include <string>
#include <vector>
#include <irrlicht.h>
#include <cassert>



class ShadowDepthShaderCB : public irr::video::IShaderConstantSetCallBack
{
public:
	void OnSetMaterial(const irr::video::SMaterial &material) override {}

	void OnSetConstants(irr::video::IMaterialRendererServices *services,
			irr::s32 userData) override;
	
	irr::f32 MaxFar{2048.0f}, MapRes{1024.0f};
	int idx{0};
};

class CSMDirectionalShadowShaderCB : public irr::video::IShaderConstantSetCallBack
{
public:
	void OnSetMaterial(const irr::video::SMaterial &material) override {}

	void OnSetConstants(irr::video::IMaterialRendererServices *services,
			irr::s32 userData) override;

	//@Liso: I have to clear this bunch of vars	 
	irr::core::matrix4 invWorld;
	irr::video::SColorf LightColour;
	irr::core::matrix4 mLightProj0, mLightProj1, mLightProj2;
	irr::core::matrix4 mLightView0, mLightView1, mLightView2;
	irr::core::matrix4 mLightView;
	irr::core::matrix4 mLightProjView0, mLightProjView1, mLightProjView2;
	irr::core::vector3df LightLink;
	irr::core::vector3df farvalues;
	irr::f32 FarLink, MapRes;

	float lightBounds[3];

	irr::core::matrix4 lightworldView;
	irr::video::ITexture *sm0, *sm1, *sm2;
};
