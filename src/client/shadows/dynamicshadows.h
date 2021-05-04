#pragma once

#include <string>
#include <vector>
#include <irrlicht.h>
#include <cmath>
#include <irrlicht.h>
#include "client/camera.h"
#include "client/client.h"


struct BSphere
{
	irr::core::vector3df center;
	float radius{0.0f};
};
struct shadowFrustum
{

	float zNear{0.0f};
	float zFar{0.0f};
	float length{0.0f};
	irr::core::matrix4 ProjOrthMat;
	irr::core::matrix4 ViewMat;
	irr::core::matrix4 WorldViewProj;
	irr::core::vector3df position;
	BSphere sphere;
	bool should_update_map_shadow{true};
	 
};


class DirectionalLight
{
public:
	DirectionalLight(const irr::u32 shadowMapResolution,
			const irr::core::vector3df &position,
			irr::video::SColorf lightColor = irr::video::SColor(0xffffffff),
			irr::f32 farValue = 100.0);
	~DirectionalLight() = default;

	DirectionalLight(const DirectionalLight &) = default;
	DirectionalLight(DirectionalLight &&) = default;
	DirectionalLight &operator=(const DirectionalLight &) = delete;
	DirectionalLight &operator=(DirectionalLight &&) = delete;

	void update_frustum(const Camera *cam, Client *client);
	
	// when set  direction is updated to negative normalized(direction)
	void setDirection(const irr::core::vector3df &dir);
	const irr::core::vector3df &getDirection();
	const irr::core::vector3df &getPosition( );

	/// Gets the light's matrices.
	const irr::core::matrix4 &getViewMatrix( ) const;
	const irr::core::matrix4 &getProjectionMatrix( ) const;
	irr::core::matrix4 getViewProjMatrix( );

	/// Gets the light's far value.
	irr::f32 getMaxFarValue() const;
	
	
	/// Gets the light's color.
	const irr::video::SColorf &getLightColor() const;

	/// Sets the light's color.
	void setLightColor(const irr::video::SColorf &lightColor);

	/// Gets the shadow map resolution for this light.
	irr::u32 getMapResolution() const;

	bool should_update_map_shadow{true};
	
private:
	
	void createSplitMatrices( const Camera *cam);
	
	irr::video::SColorf diffuseColor{0xffffffff};

	irr::f32 farPlane;
	irr::u32 mapRes;
	v3s16 m_camera_offset;


	irr::core::vector3df pos;
	irr::core::vector3df direction{0};
	irr::core::vector3df  lastcampos{0};
	shadowFrustum shadow_frustum;
	

	irr::core::vector3df v3zero{0.0f, 0.0f, 0.0f};
	irr::core::vector3df v3Yone{0.0f, 1.0f, 0.0f};
	


};
