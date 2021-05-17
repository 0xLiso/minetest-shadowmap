#include "client/shadows/dynamicshadows.h"
#include "client/clientenvironment.h"
#include "client/clientmap.h"

using v3f = irr::core::vector3df;
using m4f = irr::core::matrix4;

void DirectionalLight::createSplitMatrices(const Camera *cam)
{
	float radius;
	v3f newCenter;
	v3f look = cam->getDirection();
	look.Y = 0.0f;
	look.normalize();
	v3f camPos2 = cam->getPosition();
	v3f camPos = v3f(camPos2.X - cam->getOffset().X * BS,
			camPos2.Y - cam->getOffset().Y * BS,
			camPos2.Z - cam->getOffset().Z * BS);
	camPos += look * shadow_frustum.zNear;
	camPos2 += look * shadow_frustum.zNear;
	float end = shadow_frustum.zNear + shadow_frustum.zFar;
	newCenter = camPos + look * (shadow_frustum.zNear + 0.1f * end);
	v3f world_center = camPos2 + look * (shadow_frustum.zNear + 0.1f * end);
	// Create a vector to the frustum far corner
	// @Liso: move all vars we can outside the loop.
	float tanFovY = tanf(cam->getFovY() * 0.5f);
	float tanFovX = tanf(cam->getFovX() * 0.5f);

	const v3f &viewUp = cam->getCameraNode()->getUpVector();
	// viewUp.normalize();

	v3f viewRight = look.crossProduct(viewUp);
	// viewRight.normalize();

	v3f farCorner = look + viewRight * tanFovX + viewUp * tanFovY;
	// Compute the frustumBoundingSphere radius
	v3f boundVec = (camPos + farCorner * shadow_frustum.zFar) - newCenter;
	radius = boundVec.getLength() * 2.0f;
	// boundVec.getLength();
	float vvolume = radius * 2.0f;

	float texelsPerUnit = getMapResolution() / vvolume;
	m4f mTexelScaling;
	mTexelScaling.setScale(texelsPerUnit);

	m4f mLookAt, mLookAtInv;

	mLookAt.buildCameraLookAtMatrixLH(v3zero, -direction, v3Yone);

	mLookAt *= mTexelScaling;
	mLookAtInv = mLookAt;
	mLookAtInv.makeInverse();

	v3f frustumCenter = newCenter;
	mLookAt.transformVect(frustumCenter);
	frustumCenter.X = (float)std::floor(frustumCenter.X); // clamp to texel increment
	frustumCenter.Y = (float)std::floor(frustumCenter.Y); // clamp to texel increment
	frustumCenter.Z = (float)std::floor(frustumCenter.Z);
	mLookAtInv.transformVect(frustumCenter);
	// probar radius multipliacdor en funcion del I, a menor I mas multiplicador
	v3f eye_displacement = direction * vvolume;

	// we must compute the viewmat with the position - the camera offset
	// but the shadow_frustum position must be the actual world position
	v3f eye = frustumCenter - eye_displacement;
	shadow_frustum.position = world_center - eye_displacement;
	shadow_frustum.length =  vvolume;
	shadow_frustum.ViewMat.buildCameraLookAtMatrixLH(eye, frustumCenter, v3Yone);
	shadow_frustum.ProjOrthMat.buildProjectionMatrixOrthoLH(shadow_frustum.length,
			shadow_frustum.length, -shadow_frustum.length,
			shadow_frustum.length,false);
}
DirectionalLight::DirectionalLight(const irr::u32 shadowMapResolution,
		const irr::core::vector3df &position, irr::video::SColorf lightColor,
		irr::f32 farValue) :
		diffuseColor(lightColor),
		farPlane(farValue), mapRes(shadowMapResolution), pos(position)
{

	v3zero = irr::core::vector3df(0.0f, 0.0f, 0.0f);
	v3Yone = irr::core::vector3df(0.0f, 1.0f, 0.0f);
}
void DirectionalLight::update_frustum(const Camera *cam, Client *client)
{

	should_update_map_shadow = true;
	float zNear = cam->getCameraNode()->getNearValue();
	float zFar = getMaxFarValue();

	///////////////////////////////////
	// update splits near and fars
	shadow_frustum.zNear = zNear;
	shadow_frustum.zFar = zFar;

	// update shadow frustum
	createSplitMatrices(cam);
	// get the draw list for shadows
	client->getEnv().getClientMap().updateDrawListShadow(
			getPosition(), getDirection(), shadow_frustum.length);
	should_update_map_shadow = true;
}

void DirectionalLight::setDirection(const irr::core::vector3df &dir)
{
	direction = -dir;
	direction.normalize();
}

const irr::core::vector3df &DirectionalLight::getDirection()
{
	return direction;
}

const irr::core::vector3df &DirectionalLight::getPosition()
{
	return shadow_frustum.position;
}

const irr::core::matrix4 &DirectionalLight::getViewMatrix() const
{
	return shadow_frustum.ViewMat;
}

const irr::core::matrix4 &DirectionalLight::getProjectionMatrix() const
{
	return shadow_frustum.ProjOrthMat;
}

irr::core::matrix4 DirectionalLight::getViewProjMatrix()
{
	return shadow_frustum.ProjOrthMat * shadow_frustum.ViewMat;
}

irr::f32 DirectionalLight::getMaxFarValue() const
{
	return farPlane;
}

const irr::video::SColorf &DirectionalLight::getLightColor() const
{
	return diffuseColor;
}

void DirectionalLight::setLightColor(const irr::video::SColorf &lightColor)
{
	diffuseColor = lightColor;
}

irr::u32 DirectionalLight::getMapResolution() const
{
	return mapRes;
}
