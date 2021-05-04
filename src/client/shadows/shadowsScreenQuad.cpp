#include "shadowsScreenQuad.h"

shadowScreenQuad::shadowScreenQuad()
{
	Material.Wireframe = false;
	Material.Lighting = false;
	
	irr::video::SColor color(0x0); 
	Vertices[0] = irr::video::S3DVertex(-1.0f, -1.0f, 0.0f, 0, 0, 1, color, 0.0f, 1.0f);
	Vertices[1] = irr::video::S3DVertex(-1.0f, 1.0f, 0.0f, 0, 0, 1, color, 0.0f, 0.0f);
	Vertices[2] = irr::video::S3DVertex(1.0f, 1.0f, 0.0f, 0, 0, 1, color, 1.0f, 0.0f);
	Vertices[3] = irr::video::S3DVertex(1.0f, -1.0f, 0.0f, 0, 0, 1, color, 1.0f, 1.0f);
	Vertices[4] = irr::video::S3DVertex(-1.0f, -1.0f, 0.0f, 0, 0, 1, color, 0.0f, 1.0f);
	Vertices[5] = irr::video::S3DVertex(1.0f, 1.0f, 0.0f, 0, 0, 1, color, 1.0f, 0.0f);
}

void shadowScreenQuad::render(irr::video::IVideoDriver *driver)
{
	irr::u16 indices[6] = {0, 1, 2, 3, 4, 5};
	driver->setMaterial(Material);
	driver->setTransform(irr::video::ETS_WORLD, irr::core::matrix4());
	driver->drawIndexedTriangleList(&Vertices[0], 6, &indices[0], 2);
}

void shadowScreenQuadCB::OnSetConstants(irr::video::IMaterialRendererServices *services, irr::s32 userData)
{
	irr::s32 TextureId = 0;
	services->setPixelShaderConstant(services->getPixelShaderConstantID("ShadowMapClientMap"), &TextureId, 1);
	TextureId = 1;
	services->setPixelShaderConstant(services->getPixelShaderConstantID("ShadowMapClientMapTraslucent"), &TextureId, 1);
	TextureId = 2;
	services->setPixelShaderConstant(services->getPixelShaderConstantID("ShadowMapSamplerdynamic"), &TextureId, 1);

}
