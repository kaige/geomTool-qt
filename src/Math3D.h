#pragma once
#include <cmath>
#include <array>
#include <algorithm>
#include <string>

// ============================================================
// Vec3 - 3D Vector
// ============================================================
struct Vec3 {
    float x = 0, y = 0, z = 0;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3 operator*(const Vec3& o) const { return {x*o.x, y*o.y, z*o.z}; }

    Vec3& operator+=(const Vec3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x-=o.x; y-=o.y; z-=o.z; return *this; }

    float dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
    Vec3 cross(const Vec3& o) const {
        return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
    }
    float length() const { return std::sqrt(x*x + y*y + z*z); }
    float length2() const { return x*x + y*y + z*z; }
    Vec3 normalized() const {
        float len = length();
        if (len < 1e-10f) return {0,0,0};
        return *this / len;
    }
    float distanceTo(const Vec3& o) const { return (*this - o).length(); }
};

// ============================================================
// Mat4 - 4x4 Matrix (column-major, OpenGL-style)
// ============================================================
struct Mat4 {
    std::array<float, 16> m = {
        1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1
    };

    static Mat4 identity() { return {}; }

    static Mat4 translation(const Vec3& t) {
        Mat4 r;
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }

    static Mat4 scaling(const Vec3& s) {
        Mat4 r;
        r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
        return r;
    }

    static Mat4 rotationX(float a) {
        Mat4 r;
        float c = std::cos(a), s = std::sin(a);
        r.m[5] = c;  r.m[6] = s;
        r.m[9] = -s; r.m[10] = c;
        return r;
    }

    static Mat4 rotationY(float a) {
        Mat4 r;
        float c = std::cos(a), s = std::sin(a);
        r.m[0] = c;  r.m[2] = -s;
        r.m[8] = s;  r.m[10] = c;
        return r;
    }

    static Mat4 rotationZ(float a) {
        Mat4 r;
        float c = std::cos(a), s = std::sin(a);
        r.m[0] = c;  r.m[1] = s;
        r.m[4] = -s; r.m[5] = c;
        return r;
    }

    // Combine model transforms: T * Rz * Ry * Rx * S
    static Mat4 modelMatrix(const Vec3& pos, const Vec3& rot, const Vec3& scale) {
        return translation(pos)
            * rotationZ(rot.z) * rotationY(rot.y) * rotationX(rot.x)
            * scaling(scale);
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int c = 0; c < 4; c++) {
            for (int row = 0; row < 4; row++) {
                float sum = 0;
                for (int k = 0; k < 4; k++)
                    sum += m[k*4 + row] * o.m[c*4 + k];
                r.m[c*4 + row] = sum;
            }
        }
        return r;
    }

    Vec3 transformPoint(const Vec3& p) const {
        return {
            m[0]*p.x + m[4]*p.y + m[8]*p.z  + m[12],
            m[1]*p.x + m[5]*p.y + m[9]*p.z  + m[13],
            m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14],
        };
    }

    Vec3 transformDir(const Vec3& d) const {
        return {
            m[0]*d.x + m[4]*d.y + m[8]*d.z,
            m[1]*d.x + m[5]*d.y + m[9]*d.z,
            m[2]*d.x + m[6]*d.y + m[10]*d.z,
        };
    }
};

// Utility
inline float degToRad(float d) { return d * 3.14159265358979f / 180.0f; }
inline float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

// Parse hex color "#RRGGBB" to RGB floats 0..1
inline void parseColor(const std::string& hex, float& r, float& g, float& b) {
    if (hex.size() >= 7 && hex[0] == '#') {
        unsigned int val = 0;
        sscanf(hex.c_str() + 1, "%x", &val);
        r = ((val >> 16) & 0xFF) / 255.0f;
        g = ((val >> 8) & 0xFF) / 255.0f;
        b = (val & 0xFF) / 255.0f;
    } else {
        r = 0; g = 0.48f; b = 0.83f; // fallback blue
    }
}
