/*
Minetest
Copyright (C) 2021 Liso <anlismon@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <cmath>

#include "client/shadows/dynamicshadows.h"
#include "client/client.h"
#include "client/clientenvironment.h"
#include "client/clientmap.h"
#include "client/camera.h"

using m4f = core::matrix4;

void DirectionalLight::createSplitMatrices(shadowFrustum &shadow_subfrusta, const Camera *cam)
{
	float radius;
	v3f newCenter;
	v3f look = cam->getDirection();

	v3f camPos2 = cam->getPosition();
	v3f camPos = v3f(camPos2.X - cam->getOffset().X * BS,
			camPos2.Y - cam->getOffset().Y * BS,
			camPos2.Z - cam->getOffset().Z * BS);
	camPos += look * shadow_subfrusta.zNear;
	camPos2 += look * shadow_subfrusta.zNear;
	float end = shadow_subfrusta.zNear + shadow_subfrusta.zFar;
	newCenter = camPos + look * (shadow_subfrusta.zNear + 0.5f * end);
	v3f world_center = camPos2 + look * (shadow_subfrusta.zNear + 0.5f * end);
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
	v3f boundVec = (camPos + farCorner * shadow_subfrusta.zFar) - newCenter;
	radius = boundVec.getLength() ;
	// boundVec.getLength();
	float vvolume = radius * 2.0f;

	float texelsPerUnit = getMapResolution() / vvolume;
	m4f mTexelScaling;
	mTexelScaling.setScale(texelsPerUnit);

	m4f mLookAt, mLookAtInv;

	mLookAt.buildCameraLookAtMatrixLH(v3f(0.0f, 0.0f, 0.0f), -direction, v3f(0.0f, 1.0f, 0.0f));

	mLookAt *= mTexelScaling;
	mLookAtInv = mLookAt;
	mLookAtInv.makeInverse();

	v3f frustumCenter = newCenter;
	mLookAt.transformVect(frustumCenter);
	frustumCenter.X = floorf(frustumCenter.X); // clamp to texel increment
	frustumCenter.Y = floorf(frustumCenter.Y); // clamp to texel increment
	frustumCenter.Z = floorf(frustumCenter.Z);
	mLookAtInv.transformVect(frustumCenter);
	// probar radius multipliacdor en funcion del I, a menor I mas multiplicador
	v3f eye_displacement = direction * vvolume ;

	// we must compute the viewmat with the position - the camera offset
	// but the shadow_frustum position must be the actual world position
	v3f eye = frustumCenter - eye_displacement;
	shadow_subfrusta.position = world_center - eye_displacement;
	shadow_subfrusta.length = vvolume;
	float arbitrary_big_distance = 20000.0f;
	shadow_subfrusta.ViewMat.buildCameraLookAtMatrixLH(
			eye, frustumCenter, v3f(0.0f, 1.0f, 0.0f));
	shadow_subfrusta.ProjOrthMat.buildProjectionMatrixOrthoLH(shadow_subfrusta.length,
			shadow_subfrusta.length, -arbitrary_big_distance,
			arbitrary_big_distance, false);
}

DirectionalLight::DirectionalLight(const u32 shadowMapResolution,
		const v3f &position, video::SColorf lightColor, f32 farValue, irr::u8 nSplits) :
		diffuseColor(lightColor),
		farPlane(farValue), nearPlane(1.0), mapRes(shadowMapResolution), pos(position),
		m_nSplits(nSplits)
{
	for (u8 i = 0; i < nSplits; i++)
		shadow_frustum.emplace_back(shadowFrustum());
}

void DirectionalLight::update_frustum(const Camera *cam, Client *client)
{
	should_update_map_shadow = true;
	float zNear = cam->getCameraNode()->getNearValue();
	float zFar = getMaxFarValue()*BS;
	nearPlane = zNear;

	float nd = zNear;
	float fd = zFar;

	float lambda = 0.95f;
	float ratio = fd / nd;
	///////////////////////////////////
	// update splits near and fars
	shadow_frustum[0].zNear = zNear;
	shadow_frustum[m_nSplits - 1].zFar = zFar;
	for (int i = 1; i < m_nSplits; i++) {
		float si = i / (float)m_nSplits;

		// Practical Split Scheme:
		// https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
		float t_near = lambda * (nd * powf(ratio, si)) +
			       (1 - lambda) * (nd + (fd - nd) * si);
		float t_far = t_near * 1.005f;
		shadow_frustum[i].zNear = t_near;
		shadow_frustum[i - 1].zFar = t_far;
	}


	// update shadow frustum
	shadow_frustum[m_nSplits - 1].zFar = fd;

	for (int i = 0; i < m_nSplits; i++) {

		createSplitMatrices(shadow_frustum[i], cam);
	}
	should_update_map_shadow = true;
}

void DirectionalLight::setDirection(v3f dir)
{
	direction = -dir;
	direction.normalize();
}

v3f DirectionalLight::getPosition(u8 split_id) const
{
	return shadow_frustum[split_id].position;
}

const m4f &DirectionalLight::getViewMatrix(u8 split_id) const
{
	return shadow_frustum[split_id].ViewMat;
}

const m4f &DirectionalLight::getProjectionMatrix(u8 split_id) const
{
	return shadow_frustum[split_id].ProjOrthMat;
}

m4f DirectionalLight::getViewProjMatrix(u8 split_id)
{
	return shadow_frustum[split_id].ProjOrthMat * shadow_frustum[split_id].ViewMat;
}


s32 DirectionalLight::getNumberSplits()
{
	return m_nSplits;
}

void DirectionalLight::getSplitDistances(float splitArray[3])
{
	for (int i = 0; i < shadow_frustum.size(); i++) {
		splitArray[i] = shadow_frustum[i].zFar;
	}
}

