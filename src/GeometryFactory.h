#pragma once
#include "Math3D.h"
#include "Types.h"
#include "GeometryStore.h"
#include <vector>

// ============================================================
// GeometryFactory - Generate vertex/edge data for shapes
// ============================================================

// A rendered shape's geometry: a set of line segments (edges) or polylines
struct RenderGeometry {
    std::vector<Vec3> linePoints;      // For line/arc shapes (polyline)
    std::vector<Vec3> edgePoints;      // For wireframe (pairs of points)
    std::vector<Vec3> faceTriangles;   // For solid fill (optional)
};

struct GeometryFactory {
    // Generate line points for a line segment shape
    static std::vector<Vec3> createLineSegmentPoints(LineSegmentShape* shape) {
        std::vector<Vec3> pts;
        Vertex* sv = g_store.getVertexById(shape->startVertexId);
        Vertex* ev = g_store.getVertexById(shape->endVertexId);
        if (sv && ev) {
            pts.push_back(sv->position);
            pts.push_back(ev->position);
        }
        return pts;
    }

    // Generate line points for a circular arc
    static std::vector<Vec3> createArcPoints(CircularArcShape* shape) {
        std::vector<Vec3> pts;
        Vertex* cv = g_store.getVertexById(shape->centerVertexId);
        Vertex* sv = g_store.getVertexById(shape->startVertexId);
        Vertex* ev = g_store.getVertexById(shape->endVertexId);
        if (!cv || !sv || !ev) return pts;

        float radius = std::sqrt(
            (sv->position.x - cv->position.x) * (sv->position.x - cv->position.x) +
            (sv->position.z - cv->position.z) * (sv->position.z - cv->position.z)
        );
        float startAngle = std::atan2(sv->position.z - cv->position.z, sv->position.x - cv->position.x);
        float endAngle = std::atan2(ev->position.z - cv->position.z, ev->position.x - cv->position.x);

        float angleDiff = endAngle - startAngle;
        if (shape->clockwise) {
            if (angleDiff > 0) angleDiff -= 2 * M_PI;
        } else {
            if (angleDiff < 0) angleDiff += 2 * M_PI;
        }

        int numPoints = 32;
        float angleStep = angleDiff / numPoints;
        for (int i = 0; i <= numPoints; i++) {
            float angle = startAngle + angleStep * i;
            pts.push_back({
                cv->position.x + radius * std::cos(angle),
                0.0f,
                cv->position.z + radius * std::sin(angle)
            });
        }
        return pts;
    }

    // Generate edge points for 3D shapes (wireframe)
    static std::vector<Vec3> create3DEdges(ShapeType type) {
        switch (type) {
            case ShapeType::Sphere:   return createSphereEdges(1.0f, 16, 12);
            case ShapeType::Cube:     return createBoxEdges(1.0f);
            case ShapeType::Cylinder: return createCylinderEdges(1.0f, 1.0f, 2.0f, 16);
            case ShapeType::Cone:     return createConeEdges(1.0f, 2.0f, 16);
            case ShapeType::Torus:    return createTorusEdges(1.0f, 0.4f, 16, 24);
            default: return {};
        }
    }

    // Generate triangle faces (every 3 consecutive points = 1 triangle) for
    // 3D shapes, used for face-based hit testing. Triangle winding is not
    // significant - hit testing treats a triangle as filled regardless of
    // orientation, so the faces only need to cover the surface.
    static std::vector<Vec3> create3DFaces(ShapeType type) {
        switch (type) {
            case ShapeType::Sphere:   return createSphereFaces(1.0f, 16, 12);
            case ShapeType::Cube:     return createBoxFaces(1.0f);
            case ShapeType::Cylinder: return createCylinderFaces(1.0f, 1.0f, 2.0f, 16);
            case ShapeType::Cone:     return createConeFaces(1.0f, 2.0f, 16);
            case ShapeType::Torus:    return createTorusFaces(1.0f, 0.4f, 16, 24);
            default: return {};
        }
    }

    // --- Primitive edge generators ---
    // All primitives are Z-axis aligned (central axis = world Z, the
    // vertical axis of the Z-up scene): a cylinder/cone stands upright,
    // a torus lies flat, a sphere has its poles at ±Z.
    static std::vector<Vec3> createSphereEdges(float r, int segH, int segV) {
        std::vector<Vec3> edges;
        // Latitude lines
        for (int j = 1; j < segV; j++) {
            float phi = M_PI * j / segV;
            float z = r * std::cos(phi);
            float ringR = r * std::sin(phi);
            for (int i = 0; i < segH; i++) {
                float a1 = 2 * M_PI * i / segH;
                float a2 = 2 * M_PI * (i + 1) / segH;
                edges.push_back({ringR * std::cos(a1), ringR * std::sin(a1), z});
                edges.push_back({ringR * std::cos(a2), ringR * std::sin(a2), z});
            }
        }
        // Longitude lines
        for (int i = 0; i < segH; i++) {
            float a = 2 * M_PI * i / segH;
            for (int j = 0; j < segV; j++) {
                float phi1 = M_PI * j / segV;
                float phi2 = M_PI * (j + 1) / segV;
                edges.push_back({r * std::sin(phi1) * std::cos(a), r * std::sin(phi1) * std::sin(a), r * std::cos(phi1)});
                edges.push_back({r * std::sin(phi2) * std::cos(a), r * std::sin(phi2) * std::sin(a), r * std::cos(phi2)});
            }
        }
        return edges;
    }

    static std::vector<Vec3> createBoxEdges(float size) {
        float s = size / 2;
        Vec3 c[8] = {
            {-s,-s,-s}, { s,-s,-s}, { s, s,-s}, {-s, s,-s},
            {-s,-s, s}, { s,-s, s}, { s, s, s}, {-s, s, s}
        };
        // 12 edges of a cube
        int idx[][2] = {
            {0,1},{1,2},{2,3},{3,0}, // bottom
            {4,5},{5,6},{6,7},{7,4}, // top
            {0,4},{1,5},{2,6},{3,7}  // sides
        };
        std::vector<Vec3> edges;
        for (auto& e : idx) {
            edges.push_back(c[e[0]]);
            edges.push_back(c[e[1]]);
        }
        return edges;
    }

    static std::vector<Vec3> createCylinderEdges(float rTop, float rBot, float h, int seg) {
        std::vector<Vec3> edges;
        float halfH = h / 2;
        // Top and bottom circles
        for (int i = 0; i < seg; i++) {
            float a1 = 2 * M_PI * i / seg;
            float a2 = 2 * M_PI * (i + 1) / seg;
            // Bottom ring
            edges.push_back({rBot * std::cos(a1), rBot * std::sin(a1), -halfH});
            edges.push_back({rBot * std::cos(a2), rBot * std::sin(a2), -halfH});
            // Top ring
            edges.push_back({rTop * std::cos(a1), rTop * std::sin(a1), halfH});
            edges.push_back({rTop * std::cos(a2), rTop * std::sin(a2), halfH});
        }
        // Vertical edges (every 90 degrees)
        for (int i = 0; i < 4; i++) {
            float a = M_PI_2 * i;
            edges.push_back({rBot * std::cos(a), rBot * std::sin(a), -halfH});
            edges.push_back({rTop * std::cos(a), rTop * std::sin(a),  halfH});
        }
        return edges;
    }

    static std::vector<Vec3> createConeEdges(float r, float h, int seg) {
        std::vector<Vec3> edges;
        float halfH = h / 2;
        // Bottom circle
        for (int i = 0; i < seg; i++) {
            float a1 = 2 * M_PI * i / seg;
            float a2 = 2 * M_PI * (i + 1) / seg;
            edges.push_back({r * std::cos(a1), r * std::sin(a1), -halfH});
            edges.push_back({r * std::cos(a2), r * std::sin(a2), -halfH});
        }
        // Lines from apex to base (every 90 degrees)
        Vec3 apex = {0, 0, halfH};
        for (int i = 0; i < 4; i++) {
            float a = M_PI_2 * i;
            edges.push_back(apex);
            edges.push_back({r * std::cos(a), r * std::sin(a), -halfH});
        }
        return edges;
    }

    static std::vector<Vec3> createTorusEdges(float R, float r, int segMinor, int segMajor) {
        std::vector<Vec3> edges;
        // Draw circles along the major ring
        for (int i = 0; i < segMajor; i++) {
            float u = 2 * M_PI * i / segMajor;
            for (int j = 0; j < segMinor; j++) {
                float v1 = 2 * M_PI * j / segMinor;
                float v2 = 2 * M_PI * (j + 1) / segMinor;
                Vec3 p1 = {
                    (R + r * std::cos(v1)) * std::cos(u),
                    (R + r * std::cos(v1)) * std::sin(u),
                    r * std::sin(v1)
                };
                Vec3 p2 = {
                    (R + r * std::cos(v2)) * std::cos(u),
                    (R + r * std::cos(v2)) * std::sin(u),
                    r * std::sin(v2)
                };
                edges.push_back(p1);
                edges.push_back(p2);
            }
        }
        // Connect adjacent cross-section circles (a few)
        for (int j = 0; j < segMinor; j += segMinor / 4) {
            float v = 2 * M_PI * j / segMinor;
            for (int i = 0; i < segMajor; i++) {
                float u1 = 2 * M_PI * i / segMajor;
                float u2 = 2 * M_PI * (i + 1) / segMajor;
                Vec3 p1 = {
                    (R + r * std::cos(v)) * std::cos(u1),
                    (R + r * std::cos(v)) * std::sin(u1),
                    r * std::sin(v)
                };
                Vec3 p2 = {
                    (R + r * std::cos(v)) * std::cos(u2),
                    (R + r * std::cos(v)) * std::sin(u2),
                    r * std::sin(v)
                };
                edges.push_back(p1);
                edges.push_back(p2);
            }
        }
        return edges;
    }

    // --- Primitive face generators (triangles) ---
    static std::vector<Vec3> createBoxFaces(float size) {
        float s = size / 2;
        Vec3 c[8] = {
            {-s,-s,-s}, { s,-s,-s}, { s, s,-s}, {-s, s,-s},
            {-s,-s, s}, { s,-s, s}, { s, s, s}, {-s, s, s}
        };
        std::vector<Vec3> f;
        auto quad = [&](int a, int b, int cc, int d) {
            f.push_back(c[a]); f.push_back(c[b]); f.push_back(c[cc]);
            f.push_back(c[a]); f.push_back(c[cc]); f.push_back(c[d]);
        };
        quad(0,1,2,3); // -z
        quad(4,5,6,7); // +z
        quad(0,1,5,4); // -y
        quad(3,2,6,7); // +y
        quad(0,3,7,4); // -x
        quad(1,2,6,5); // +x
        return f;
    }

    static std::vector<Vec3> createSphereFaces(float r, int segH, int segV) {
        std::vector<Vec3> f;
        auto P = [&](float phi, float a) {
            return Vec3(r * std::sin(phi) * std::cos(a),
                        r * std::sin(phi) * std::sin(a),
                        r * std::cos(phi));
        };
        for (int j = 0; j < segV; j++) {
            float phi1 = M_PI * j / segV, phi2 = M_PI * (j + 1) / segV;
            for (int i = 0; i < segH; i++) {
                float a1 = 2 * M_PI * i / segH, a2 = 2 * M_PI * (i + 1) / segH;
                Vec3 p00 = P(phi1, a1), p10 = P(phi1, a2), p11 = P(phi2, a2), p01 = P(phi2, a1);
                f.push_back(p00); f.push_back(p10); f.push_back(p11);
                f.push_back(p00); f.push_back(p11); f.push_back(p01);
            }
        }
        return f;
    }

    static std::vector<Vec3> createCylinderFaces(float rTop, float rBot, float h, int seg) {
        std::vector<Vec3> f;
        float halfH = h / 2;
        Vec3 Cb(0, 0, -halfH), Ct(0, 0, halfH);
        for (int i = 0; i < seg; i++) {
            float a1 = 2 * M_PI * i / seg, a2 = 2 * M_PI * (i + 1) / seg;
            Vec3 b1(rBot * std::cos(a1), rBot * std::sin(a1), -halfH);
            Vec3 b2(rBot * std::cos(a2), rBot * std::sin(a2), -halfH);
            Vec3 t1(rTop * std::cos(a1), rTop * std::sin(a1),  halfH);
            Vec3 t2(rTop * std::cos(a2), rTop * std::sin(a2),  halfH);
            f.push_back(Cb); f.push_back(b1); f.push_back(b2);    // bottom cap
            f.push_back(Ct); f.push_back(t2); f.push_back(t1);    // top cap
            f.push_back(b1); f.push_back(b2); f.push_back(t2);    // side
            f.push_back(b1); f.push_back(t2); f.push_back(t1);
        }
        return f;
    }

    static std::vector<Vec3> createConeFaces(float r, float h, int seg) {
        std::vector<Vec3> f;
        float halfH = h / 2;
        Vec3 apex(0, 0, halfH), Cb(0, 0, -halfH);
        for (int i = 0; i < seg; i++) {
            float a1 = 2 * M_PI * i / seg, a2 = 2 * M_PI * (i + 1) / seg;
            Vec3 b1(r * std::cos(a1), r * std::sin(a1), -halfH);
            Vec3 b2(r * std::cos(a2), r * std::sin(a2), -halfH);
            f.push_back(Cb);   f.push_back(b1); f.push_back(b2);   // bottom cap
            f.push_back(apex); f.push_back(b2); f.push_back(b1);   // side
        }
        return f;
    }

    static std::vector<Vec3> createTorusFaces(float R, float r, int segMinor, int segMajor) {
        std::vector<Vec3> f;
        auto P = [&](float u, float v) {
            return Vec3((R + r * std::cos(v)) * std::cos(u),
                        (R + r * std::cos(v)) * std::sin(u),
                        r * std::sin(v));
        };
        for (int i = 0; i < segMajor; i++) {
            float u1 = 2 * M_PI * i / segMajor, u2 = 2 * M_PI * (i + 1) / segMajor;
            for (int j = 0; j < segMinor; j++) {
                float v1 = 2 * M_PI * j / segMinor, v2 = 2 * M_PI * (j + 1) / segMinor;
                Vec3 p00 = P(u1, v1), p10 = P(u2, v1), p11 = P(u2, v2), p01 = P(u1, v2);
                f.push_back(p00); f.push_back(p10); f.push_back(p11);
                f.push_back(p00); f.push_back(p11); f.push_back(p01);
            }
        }
        return f;
    }
};
