#pragma once
#include <string>
#include <vector>
#include <irrlicht.h>

class shadowScreenQuad
{
public:
	shadowScreenQuad();

	void render(irr::video::IVideoDriver *driver);
	irr::video::SMaterial &getMaterial() { return Material; }

	irr::video::ITexture *textures[3];

private:
	irr::video::S3DVertex Vertices[6];
	irr::video::SMaterial Material;
};

class shadowScreenQuadCB : public irr::video::IShaderConstantSetCallBack
{
public:
	shadowScreenQuadCB(){};

	virtual void OnSetConstants(irr::video::IMaterialRendererServices *services,
			irr::s32 userData);
};