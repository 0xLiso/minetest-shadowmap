#include "client/shadows/shadowsshadercallbacks.h"

#include "porting.h"
void ShadowDepthShaderCB::OnSetConstants(
		irr::video::IMaterialRendererServices *services, irr::s32 userData)
{
	irr::video::IVideoDriver *driver = services->getVideoDriver();

	irr::core::matrix4 lightMVP = driver->getTransform(irr::video::ETS_PROJECTION);
	lightMVP *= driver->getTransform(irr::video::ETS_VIEW);
	lightMVP *= driver->getTransform(irr::video::ETS_WORLD);
	services->setVertexShaderConstant(
				services->getPixelShaderConstantID("LightMVP"),
				(irr::f32 *)lightMVP.pointer(), 16);

	services->setVertexShaderConstant(services->getPixelShaderConstantID("MapResolution"), &MapRes, 1);
	services->setVertexShaderConstant(services->getPixelShaderConstantID("MaxFar"), &MaxFar, 1);

	irr::s32 TextureId = 0;
	services->setPixelShaderConstant(services->getPixelShaderConstantID("ColorMapSampler"), &TextureId, 1);
	
}
 
