#pragma once
#include <string>
#include <vector>
#include "Math3D.h"

// ============================================================
// Geometry Types - mirrors the TypeScript GeometryTypes.ts
// ============================================================

enum class ShapeType {
    Sphere, Cube, Cylinder, Cone, Torus,
    LineSegment, Rectangle, Circle, Triangle, Polygon, CircularArc
};

// Base shape with common properties
struct BaseShape {
    std::string id;
    ShapeType type;
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;
    std::string color = "#0078d4";
    bool visible = true;
    bool hasChanged = false;
    bool hasSelectionChanged = false;
    // Render-style blend for cylinder/cone caps (textbook 直观图 ellipse ↔
    // strict oblique projection). −1 = unset (snap on first draw); otherwise
    // damped toward the pose-driven target each painted frame.
    float poseBlend = -1.0f;

    virtual ~BaseShape() = default;
};

// 3D Solid shapes (sphere, cube, etc.)
struct Shape3D : BaseShape {};

// 2D shapes stored as vertex references
struct LineSegmentShape : BaseShape {
    std::string startVertexId;
    std::string endVertexId;
};

struct RectangleShape : BaseShape {
    std::vector<std::string> vertexIds; // 4 vertices
};

struct CircleShape : BaseShape {
    std::string centerVertexId;
    float radius = 1.0f;
};

struct TriangleShape : BaseShape {
    std::vector<std::string> vertexIds; // 3 vertices
};

struct PolygonShape : BaseShape {
    std::vector<std::string> vertexIds; // N vertices
};

struct CircularArcShape : BaseShape {
    std::string centerVertexId;
    std::string startVertexId;
    std::string endVertexId;
    bool clockwise = false;
};

// Vertex
struct Vertex {
    std::string id;
    Vec3 position;
    bool hasChanged = false;
};

// ============================================================
// Tool Types
// ============================================================
enum class ToolType {
    Select,
    MoveShape,
    RotateShape,
    MoveLineEndpoint,
    CreateSphere, CreateCube, CreateCylinder, CreateCone, CreateTorus,
    CreateLineSegment,
    CreateCircularArc,
    MoveArcEndpoint,
    MoveArc
};

// Helper: convert shape type to display string key
inline std::string shapeTypeKey(ShapeType t) {
    switch (t) {
        case ShapeType::Sphere:      return "sphere";
        case ShapeType::Cube:        return "cube";
        case ShapeType::Cylinder:    return "cylinder";
        case ShapeType::Cone:        return "cone";
        case ShapeType::Torus:       return "torus";
        case ShapeType::LineSegment: return "lineSegment";
        case ShapeType::Rectangle:   return "rectangle";
        case ShapeType::Circle:      return "circle";
        case ShapeType::Triangle:    return "triangle";
        case ShapeType::Polygon:     return "polygon";
        case ShapeType::CircularArc: return "circularArc";
    }
    return "unknown";
}
