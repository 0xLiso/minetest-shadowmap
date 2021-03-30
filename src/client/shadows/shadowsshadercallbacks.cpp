#include "client/shadows/shadowsshadercallbacks.h"

void ShadowDepthShaderCB::OnSetConstants(
		irr::video::IMaterialRendererServices *services, irr::s32 userData)
{
	irr::video::IVideoDriver *driver = services->getVideoDriver();

	irr::core::matrix4 lightMVP = driver->getTransform(irr::video::ETS_PROJECTION);
	lightMVP *= driver->getTransform(irr::video::ETS_VIEW);
	lightMVP *= driver->getTransform(irr::video::ETS_WORLD);
	bool res = services->setVertexShaderConstant(
			"LightMVP", (irr::f32 *)lightMVP.pointer(), 16);

	services->setVertexShaderConstant("CascadeIdx", &idx, 1);
	services->setVertexShaderConstant("MapResolution", &MapRes, 1);
	services->setVertexShaderConstant("MaxFar", &MaxFar, 1);
	
}
 
