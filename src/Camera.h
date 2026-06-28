#pragma once
#include "Math3D.h"

// ============================================================
// Camera - Orthographic camera with orbit controls
// ============================================================
class Camera {
public:
    float azimuth = 0;      // horizontal angle
    float elevation = 0;    // vertical angle
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
        float ca = std::cos(azimuth), sa = std::sin(azimuth);
        float ce = std::cos(elevation), se = std::sin(elevation);
        return {
            target.x + distance * sa * ce,
            target.y + distance * se,
            target.z + distance * ca * ce
        };
    }

    // Get view direction (from camera towards target)
    Vec3 getForward() const {
        return (target - getPosition()).normalized();
    }

    // Get right vector (perpendicular to forward, in screen plane)
    Vec3 getRight() const {
        Vec3 forward = getForward();
        Vec3 up = {0, 1, 0};
        return forward.cross(up).normalized();
    }

    // Get up vector
    Vec3 getUp() const {
        Vec3 forward = getForward();
        Vec3 right = getRight();
        return right.cross(forward).normalized();
    }

    // World-to-screen projection (returns screen pixel coordinates)
    // Returns false if point is behind camera
    bool project(const Vec3& worldPos, int screenW, int screenH, float& sx, float& sy) const {
        Vec3 camPos = getPosition();
        Vec3 forward = getForward();
        Vec3 right = getRight();
        Vec3 up = getUp();

        Vec3 rel = worldPos - camPos;

        // Project onto camera axes
        float depth = rel.dot(forward);  // depth from camera
        if (depth < 0.1f) return false;

        float xCam = rel.dot(right);
        float yCam = rel.dot(up);

        // Orthographic projection
        float halfW = frustumSize * aspect / 2;
        float halfH = frustumSize / 2;

        float ndcX = xCam / halfW;
        float ndcY = yCam / halfH;

        // NDC to screen (flip Y for screen coordinates)
        sx = (ndcX + 1.0f) * screenW / 2.0f;
        sy = (1.0f - ndcY) * screenH / 2.0f;

        return true;
    }

    // Screen-to-world on Z=0 plane
    Vec3 screenToWorld(float sx, float sy, int screenW, int screenH) const {
        float ndcX = (2.0f * sx / screenW) - 1.0f;
        float ndcY = 1.0f - (2.0f * sy / screenH);

        float halfW = frustumSize * aspect / 2;
        float halfH = frustumSize / 2;

        Vec3 camPos = getPosition();
        Vec3 right = getRight();
        Vec3 up = getUp();

        // Point in world space at camera plane
        Vec3 worldPos = camPos
            + right * (ndcX * halfW)
            + up * (ndcY * halfH);

        return worldPos;
    }

    // Screen-to-world on an arbitrary plane (normal + point)
    Vec3 screenToWorldOnPlane(float sx, float sy, int screenW, int screenH,
                               const Vec3& planeNormal, const Vec3& planePoint) const {
        Vec3 camPos = getPosition();
        Vec3 forward = getForward();

        float ndcX = (2.0f * sx / screenW) - 1.0f;
        float ndcY = 1.0f - (2.0f * sy / screenH);

        float halfW = frustumSize * aspect / 2;
        float halfH = frustumSize / 2;

        Vec3 right = getRight();
        Vec3 up = getUp();

        // Ray origin = point on near plane
        Vec3 rayOrigin = camPos
            + right * (ndcX * halfW)
            + up * (ndcY * halfH);

        // Ray direction = forward (orthographic)
        Vec3 rayDir = forward;

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
