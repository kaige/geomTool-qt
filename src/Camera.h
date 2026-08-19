#pragma once
#include "Math3D.h"

// ============================================================
// Camera - Oblique (斜二测) camera with orbit controls
//
// Z-up convention (CAD / textbook 立体几何): the projection plane
// stays flush against the XZ plane (default azimuth = elevation =
// 0, camera at -Y facing the XZ plane), so on screen X points
// right, Z points up, and Y points INTO the scene. The PROJECTOR
// direction is tilted — the 斜二测 / cabinet setup of textbook
// 直观图 (x horizontal, z vertical, y receding up-right at 45°
// with 0.5 foreshortening):
//   * everything in the XZ plane (grid, 2D shapes, a cube's front
//     face) projects undistorted: a square stays a true square;
//   * depth (+Y) recedes to the upper-right at 45° with 0.5
//     foreshortening (√2/4 ≈ 0.3536 shear per screen axis);
//   * an axis-aligned cube shows front + top + right, with
//     left / back / bottom hidden (dashed).
// ============================================================
class Camera {
public:
    // Tilt of the projector direction away from the screen normal,
    // applied in both screen axes: -0.5*cos(45°) = -√2/4. Negative so
    // that depth BEHIND the screen plane (zs < 0) shifts up-right.
    static constexpr float OBLIQUE_SHEAR = -0.35355339059327373f;

    float azimuth = 0;      // rotation about the world Z (up) axis;
                            // 0 = camera at -Y, facing the XZ plane
    float elevation = 0;    // angle above the XZ plane (+ = up)
    float distance = 15.0f; // distance from origin
    float frustumSize = 20.0f;
    float aspect = 1.0f;
    Vec3 target = {0, 0, 0};

    // Pan offset
    Vec3 panOffset = {0, 0, 0};

    void reset() {
        azimuth = 0;
        elevation = 0;
        distance = 15.0f;
        frustumSize = 20.0f;
        panOffset = {0, 0, 0};
        target = {0, 0, 0};
    }

    Vec3 getPosition() const {
        // Z-up spherical: azimuth spins about the world Z axis
        // (0 → camera at -Y), elevation lifts towards +Z.
        float ca = std::cos(azimuth), sa = std::sin(azimuth);
        float ce = std::cos(elevation), se = std::sin(elevation);
        return {
            target.x + distance * sa * ce,
            target.y - distance * ca * ce,
            target.z + distance * se
        };
    }

    // Screen-plane frame normal (unit vector from target towards the
    // camera, negated): the direction a PERPENDICULAR camera would view
    // along. Derived purely from azimuth/elevation.
    Vec3 getFrameForward() const {
        return (target - getPosition()).normalized();
    }

    // Get right vector (perpendicular to frame forward, in screen plane).
    // World-up reference is +Z (the vertical axis of the scene).
    Vec3 getRight() const {
        Vec3 ff = getFrameForward();
        Vec3 up = {0, 0, 1};
        return ff.cross(up).normalized();
    }

    // Get up vector
    Vec3 getUp() const {
        return getRight().cross(getFrameForward()).normalized();
    }

    // Projector direction (into the scene): the frame forward tilted by
    // the oblique shear. Visibility / hidden-line tests use this, which
    // is what makes a cube show front + top + right under the default
    // frontal view (left / back / bottom face away from the projectors).
    Vec3 getForward() const {
        return (getFrameForward()
                + getRight() * OBLIQUE_SHEAR
                + getUp() * OBLIQUE_SHEAR).normalized();
    }

    // World-to-screen projection (returns screen pixel coordinates)
    // Returns false if point is behind camera
    bool project(const Vec3& worldPos, int screenW, int screenH, float& sx, float& sy) const {
        Vec3 camPos = getPosition();
        Vec3 ff = getFrameForward();
        Vec3 right = getRight();
        Vec3 up = getUp();

        Vec3 rel = worldPos - camPos;

        // Depth from camera (near-plane culling only)
        float depth = rel.dot(ff);
        if (depth < 0.1f) return false;

        // Signed distance from the screen plane through `target`
        // (positive = on the camera side). Drives the oblique shear:
        // depth behind the plane shifts a point up-right on screen.
        float zs = distance - depth;

        float xCam = rel.dot(right) + OBLIQUE_SHEAR * zs;
        float yCam = rel.dot(up)    + OBLIQUE_SHEAR * zs;

        // Orthographic projection of the (obliquely mapped) screen plane
        float halfW = frustumSize * aspect / 2;
        float halfH = frustumSize / 2;

        float ndcX = xCam / halfW;
        float ndcY = yCam / halfH;

        // NDC to screen (flip Y for screen coordinates)
        sx = (ndcX + 1.0f) * screenW / 2.0f;
        sy = (1.0f - ndcY) * screenH / 2.0f;

        return true;
    }

    // Screen-to-world: point on the screen plane (through target) that
    // projects to this pixel. Points of the screen plane are the fixed
    // points of the oblique map, so this is exact for work-plane
    // (y = 0, XZ) content.
    Vec3 screenToWorld(float sx, float sy, int screenW, int screenH) const {
        float ndcX = (2.0f * sx / screenW) - 1.0f;
        float ndcY = 1.0f - (2.0f * sy / screenH);

        float halfW = frustumSize * aspect / 2;
        float halfH = frustumSize / 2;

        return target
            + getRight() * (ndcX * halfW)
            + getUp() * (ndcY * halfH);
    }

    // Screen-to-world on an arbitrary plane (normal + point)
    Vec3 screenToWorldOnPlane(float sx, float sy, int screenW, int screenH,
                               const Vec3& planeNormal, const Vec3& planePoint) const {
        float ndcX = (2.0f * sx / screenW) - 1.0f;
        float ndcY = 1.0f - (2.0f * sy / screenH);

        float halfW = frustumSize * aspect / 2;
        float halfH = frustumSize / 2;

        Vec3 right = getRight();
        Vec3 up = getUp();

        // Ray origin = the screen-plane point behind this pixel (it
        // projects to itself), ray direction = the oblique projector.
        // This ray is exactly the fiber of project(), so the map inverts
        // consistently for every plane.
        Vec3 rayOrigin = target
            + right * (ndcX * halfW)
            + up * (ndcY * halfH);

        Vec3 rayDir = getForward();

        // Intersect ray with plane
        float denom = rayDir.dot(planeNormal);
        if (std::abs(denom) < 1e-10f) return rayOrigin;

        float t = (planePoint - rayOrigin).dot(planeNormal) / denom;
        return rayOrigin + rayDir * t;
    }

    void zoom(float delta) {
        frustumSize += delta * 0.1f * frustumSize;
        frustumSize = clampf(frustumSize, 1.0f, 100.0f);
    }

    void orbit(float deltaAzimuth, float deltaElevation) {
        azimuth -= deltaAzimuth;
        elevation = clampf(elevation + deltaElevation,
                           -M_PI_2 + 0.1f, M_PI_2 - 0.1f);
    }

    void pan(float deltaX, float deltaY) {
        float halfW = frustumSize * aspect / 2;
        float halfH = frustumSize / 2;

        Vec3 right = getRight();
        Vec3 up = getUp();

        Vec3 moveVec = right * (-deltaX * halfW * 2.0f) + up * (deltaY * halfH * 2.0f);

        target += moveVec;
        panOffset += moveVec;
    }
};
