#pragma once

#include <string>
#include <vector>
#include <irrlicht.h>
#include <cmath>
#include <irrlicht.h>
#include "client/camera.h"
#include "client/client.h"




enum E_SHADOW_RENDER_MODE:u8
{
	ERM_SHADOWMAP=1,
	ERM_CSM=3,
	ERM_COUNT
};



struct BSphere
{
	irr::core::vector3df center;
	float radius{0.0f};
};
struct csmfrustum
{

	float zNear{0.0f};
	float zFar{0.0f};
	float length{0.0f};
	irr::core::matrix4 csmProjOrthMat;
	irr::core::matrix4 csmViewMat;
	irr::core::matrix4 cmsWorldViewProj;
	irr::core::vector3df position;
	BSphere sphere;
	bool should_update_map_shadow{true};
	s8 id{-1};
};


class DirectionalLight
{
public:
	DirectionalLight(const irr::u32 shadowMapResolution,
			const irr::core::vector3df &position,
			irr::video::SColorf lightColor = irr::video::SColor(0xffffffff),
			irr::f32 farValue = 100.0, irr::u8 nSplits = ERM_SHADOWMAP);
	~DirectionalLight() = default;

	DirectionalLight(const DirectionalLight &) = default;
	DirectionalLight(DirectionalLight &&) = default;
	DirectionalLight &operator=(const DirectionalLight &) = delete;
	DirectionalLight &operator=(DirectionalLight &&) = delete;

	void update_frustum(const Camera *cam, Client *client);
	
	// when set position, the direction is updated to normalized(position)
	void setPosition(const irr::core::vector3df &position);
	const irr::core::vector3df &getDirection();
	const irr::core::vector3df &getPosition(int n_split);

	/// Gets the light's matrices.
	const irr::core::matrix4 &getViewMatrix(int id=0) const;
	const irr::core::matrix4 &getProjectionMatrix(int id=0) const;
	irr::core::matrix4 getViewProjMatrix(int id=0);

	/// Gets the light's far value.
	irr::f32 getMaxFarValue() const;
	
	
	/// Gets the light's color.
	const irr::video::SColorf &getLightColor() const;

	/// Sets the light's color.
	void setLightColor(const irr::video::SColorf &lightColor);

	/// Gets the shadow map resolution for this light.
	irr::u32 getMapResolution() const;
	
 
	void getSplitDistances(float splitArray[4]);
	bool should_update_map_shadow{true};
	
private:
	
	void createSplitMatrices(csmfrustum &subfrusta, const Camera *cam);
	
	irr::video::SColorf diffuseColor{0xffffffff};

	irr::f32 farPlane{128};
	irr::u32 mapRes{512};
	v3s16 m_camera_offset;


	irr::core::vector3df pos{0}, direction{0}, lastcampos{0};
	std::array<csmfrustum, 3> csm_frustum;
	s32 nsplits{1};
	


};
