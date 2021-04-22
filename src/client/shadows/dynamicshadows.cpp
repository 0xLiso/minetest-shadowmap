#include "client/shadows/dynamicshadows.h"
#include "client/clientenvironment.h"
#include "client/clientmap.h"

using v3f = irr::core::vector3df;
using m4f = irr::core::matrix4;


void DirectionalLight::createSplitMatrices(csmfrustum &subfrusta, const Camera *cam) {
    float radius;
    v3f newCenter;
    v3f look = cam->getDirection();
    look.normalize();
    v3f camPos2 = cam->getPosition();
    v3f camPos = v3f(camPos2.X - cam->getOffset().X * BS,
                     camPos2.Y - cam->getOffset().Y * BS,
                     camPos2.Z - cam->getOffset().Z * BS);  //= cam->getCameraNode()->getPosition();
    camPos += look * subfrusta.zNear;
    camPos2 += look * subfrusta.zNear;
    float end = subfrusta.zNear + subfrusta.zFar;
    newCenter = camPos + look * (subfrusta.zNear + 0.5f * end);
    v3f world_center = camPos2 + look * (subfrusta.zNear + 0.5f * end);
    // Create a vector to the frustum far corner
    // @Liso: move all vars we can outside the loop.
    float tanFovY = tanf(cam->getFovY() * 0.5f);
    float tanFovX = tanf(cam->getFovX() * 0.5f);

    v3f viewUp = cam->getCameraNode()->getUpVector();
    viewUp.normalize();

    v3f viewRight = look.crossProduct(viewUp);
    viewRight.normalize();

    v3f farCorner = look + viewRight * tanFovX + viewUp * tanFovY;
    // Compute the frustumBoundingSphere radius
    v3f boundVec = (camPos + farCorner * subfrusta.zFar) - newCenter;
    radius = boundVec.getLength() * 2.0f;
    //boundVec.getLength();
    float vvolume = radius * 2.0f;

    float texelsPerUnit = getMapResolution() / vvolume;
    m4f mTexelScaling;
    mTexelScaling.setScale(texelsPerUnit);

    m4f mLookAt, mLookAtInv;

    mLookAt.buildCameraLookAtMatrixLH(
        v3f(0.0f, 0.0f, 0.0f), -direction, v3f(0.0f, 1.0f, 0.0f));

    mLookAt *= mTexelScaling;
    mLookAtInv = mLookAt;
    mLookAtInv.makeInverse();

    v3f frustumCenter = newCenter;
    mLookAt.transformVect(frustumCenter);
    frustumCenter.X = (float)std::floor(frustumCenter.X); // clamp to texel increment
    frustumCenter.Y = (float)std::floor(frustumCenter.Y); // clamp to texel increment
    frustumCenter.Z = (float)std::floor(frustumCenter.Z);
    mLookAtInv.transformVect(frustumCenter);
    //probar radius multipliacdor en funcion del I, a menor I mas multiplicador
    v3f eye_displacement = direction * vvolume;

    // we must compute the viewmat with the position - the camera offset
    // but the subfrusta position must be the actual world position
    v3f eye = frustumCenter - eye_displacement;
    subfrusta.position = world_center - eye_displacement;
    subfrusta.length = 2.0f * vvolume;
    subfrusta.csmViewMat.buildCameraLookAtMatrixLH(
        eye, frustumCenter, v3f(0.0f, 1.0f, 0.0f).normalize());
    subfrusta.csmProjOrthMat.buildProjectionMatrixOrthoLH(
        subfrusta.length, subfrusta.length, -subfrusta.length, subfrusta.length);
}
DirectionalLight::DirectionalLight(const irr::u32 shadowMapResolution,
                                   const irr::core::vector3df &position, irr::video::SColorf lightColor,
                                   irr::f32 farValue, irr::u8 nSplits) :
    diffuseColor(lightColor),
    farPlane(farValue   ),
    mapRes(shadowMapResolution),
    pos(position),
    nsplits(nSplits) {
    for (size_t i = 0; i < csm_frustum.size(); i++) {
        csm_frustum[i].id = i;
    }
}
void DirectionalLight::update_frustum(const Camera *cam, Client *client) {

    should_update_map_shadow = true;
    float zNear = cam->getCameraNode()->getNearValue();
    /*float zFar = cam->getCameraNode()->getFarValue() > getMaxFarValue()
                     ? getMaxFarValue()
                     : cam->getCameraNode()->getFarValue();
                     */
    float wanted_range =
        client->getEnv().getClientMap().getWantedRange() ;

    float zFar = getMaxFarValue()  > wanted_range ? wanted_range : getMaxFarValue() ;
    ///////////////////////////////////
    // update splits near and fars
    //#pragma warning "check what values are in here, wanted_range vs getMaxFarValue"
    float nd = zNear;
    float fd = zFar ;

    float lambda = 0.90f;
    float ratio = (zFar / zNear);


    // @Liso: ok, there is a huge problem with the extremely high structures in MT,
    // so we need to add huge offset to the zNear
    // right now, I´ve only find this hack to minimize it.
    csm_frustum[0].zNear = nd   ;
    size_t maxSplits = 1;
    for (size_t i = 1; i < maxSplits; i++) {
        float si = i / (float)maxSplits;

        // Practical Split Scheme:
        // https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
        float t_near = lambda * (nd * powf(ratio, si)) +
                       (1 - lambda) * (nd + (fd - nd) * si);
        float t_far = t_near * 1.05f;
        csm_frustum[i].zNear = t_near;
        csm_frustum[i - (size_t)1].zFar = t_far;
    }

    csm_frustum[maxSplits - (size_t)1].zFar = fd;

    for (int i = 0; i < nsplits; i++) {
        createSplitMatrices(csm_frustum[i], cam);
        //yes, i know this is wrong, but it´s just an approximation to test the concept.
        client->getEnv().getClientMap().updateDrawListShadow(
            csm_frustum[i].position, getDirection(), csm_frustum[i].length);

    }
    should_update_map_shadow = true;

}

void DirectionalLight::setPosition(const irr::core::vector3df &position) {
    pos = position;
    direction = -position;
    direction.normalize();
}

const irr::core::vector3df &DirectionalLight::getDirection() {
    return direction;
}

const irr::core::vector3df &DirectionalLight::getPosition(int n_split) {
    return csm_frustum[n_split].position;
}

const irr::core::matrix4 &DirectionalLight::getViewMatrix(int id) const {
    return csm_frustum[id].csmViewMat;
}

const irr::core::matrix4 &DirectionalLight::getProjectionMatrix(int id) const {
    return csm_frustum[id].csmProjOrthMat;
}

irr::core::matrix4 DirectionalLight::getViewProjMatrix(int id) {
    return csm_frustum[id].csmProjOrthMat * csm_frustum[id].csmViewMat;
}

irr::f32 DirectionalLight::getMaxFarValue() const {
    return farPlane;
}

const irr::video::SColorf &DirectionalLight::getLightColor() const {
    return diffuseColor;
}

void DirectionalLight::setLightColor(const irr::video::SColorf &lightColor) {
    diffuseColor = lightColor;
}

irr::u32 DirectionalLight::getMapResolution() const {
    return mapRes;
}




void DirectionalLight::getSplitDistances(float splitArray[4]) {
    for (size_t i = 0; i < csm_frustum.size(); i++) {
        splitArray[i] = csm_frustum[i].zFar;
    }
}
