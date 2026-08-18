#pragma once
#include "Types.h"
#include <map>
#include <memory>
#include <functional>

// ============================================================
// GeometryStore - Central state management (mirrors MobX store)
// ============================================================

class GeometryStore {
public:
    std::vector<Vertex> vertices;
    std::vector<std::unique_ptr<BaseShape>> shapes;
    std::string selectedShapeId;
    std::string selectedVertexId;
    ToolType activeToolType = ToolType::Select;

    // Callback for UI to know when state changed
    std::function<void()> onChange;

    void notifyChange() { if (onChange) onChange(); }
    void setActiveToolType(ToolType t) { activeToolType = t; notifyChange(); }

    // --- Vertex management ---
    std::string addVertex(const Vec3& pos) {
        std::string id = std::to_string(nextId++);
        vertices.push_back({id, pos, true});
        return id;
    }

    Vertex* getVertexById(const std::string& id) {
        for (auto& v : vertices)
            if (v.id == id) return &v;
        return nullptr;
    }

    void updateVertex(const std::string& id, const Vec3& pos) {
        for (auto& v : vertices) {
            if (v.id == id) {
                v.position = pos;
                v.hasChanged = true;
                // Mark dependent shapes changed
                for (auto& shape : shapes)
                    markShapeIfUsesVertex(*shape, id);
                return;
            }
        }
    }

    // --- Shape management ---
    template<typename T>
    T* addShape(std::unique_ptr<T> shape) {
        shape->id = std::to_string(nextId++);
        shape->hasChanged = true;
        T* ptr = shape.get();
        shapes.push_back(std::move(shape));
        notifyChange();
        return ptr;
    }

    void addShape3D(ShapeType type, const Vec3& position = {0, 0, -0.5f}) {
        auto shape = std::make_unique<Shape3D>();
        shape->type = type;
        shape->position = position;
        // Axis-aligned, unrotated. Under the default 斜二测 camera
        // (projection plane flush with XY + oblique projectors) an
        // unrotated cube reads exactly like the textbook 直观图:
        // front face is a true square (bottom ∥ X, sides ∥ Y) lying in
        // the XY plane, depth recedes up-right at 45° × 0.5, so top and
        // right faces are visible while left / back / bottom are hidden.
        // Default position puts the unit cube's front face exactly on
        // the z = 0 grid plane (half-size 0.5 → center at z = -0.5).
        shape->rotation = {0, 0, 0};
        shape->scale = {1, 1, 1};
        shape->color = "#0078d4";
        shape->visible = true;
        shape->hasChanged = true;
        shape->id = std::to_string(nextId++);
        shapes.push_back(std::move(shape));
        notifyChange();
    }

    void addLineSegment(const Vec3& start, const Vec3& end) {
        std::string startId = addVertex(start);
        std::string endId = addVertex(end);
        auto shape = std::make_unique<LineSegmentShape>();
        shape->type = ShapeType::LineSegment;
        shape->startVertexId = startId;
        shape->endVertexId = endId;
        shape->scale = {1, 1, 1};
        shape->color = "#0078d4";
        shape->id = std::to_string(nextId++);
        shapes.push_back(std::move(shape));
        notifyChange();
    }

    void addRectangle(const std::vector<Vec3>& positions) {
        if (positions.size() != 4) return;
        std::vector<std::string> ids;
        for (auto& p : positions) ids.push_back(addVertex(p));
        auto shape = std::make_unique<RectangleShape>();
        shape->type = ShapeType::Rectangle;
        shape->vertexIds = ids;
        shape->id = std::to_string(nextId++);
        shapes.push_back(std::move(shape));
        notifyChange();
    }

    void addCircle(const Vec3& center, float radius) {
        std::string centerId = addVertex(center);
        auto shape = std::make_unique<CircleShape>();
        shape->type = ShapeType::Circle;
        shape->centerVertexId = centerId;
        shape->radius = radius;
        shape->id = std::to_string(nextId++);
        shapes.push_back(std::move(shape));
        notifyChange();
    }

    void addTriangle(const std::vector<Vec3>& positions) {
        if (positions.size() != 3) return;
        std::vector<std::string> ids;
        for (auto& p : positions) ids.push_back(addVertex(p));
        auto shape = std::make_unique<TriangleShape>();
        shape->type = ShapeType::Triangle;
        shape->vertexIds = ids;
        shape->id = std::to_string(nextId++);
        shapes.push_back(std::move(shape));
        notifyChange();
    }

    void addPolygon(const std::vector<Vec3>& positions) {
        if (positions.size() < 3) return;
        std::vector<std::string> ids;
        for (auto& p : positions) ids.push_back(addVertex(p));
        auto shape = std::make_unique<PolygonShape>();
        shape->type = ShapeType::Polygon;
        shape->vertexIds = ids;
        shape->id = std::to_string(nextId++);
        shapes.push_back(std::move(shape));
        notifyChange();
    }

    // Circular arc from 3 points: start, end, arcPoint
    void addCircularArc(const Vec3& start, const Vec3& end, const Vec3& arcPoint);

    BaseShape* getShapeById(const std::string& id) {
        for (auto& s : shapes)
            if (s->id == id) return s.get();
        return nullptr;
    }

    BaseShape* getSelectedShape() {
        if (selectedShapeId.empty()) return nullptr;
        return getShapeById(selectedShapeId);
    }

    void removeShape(const std::string& id) {
        shapes.erase(std::remove_if(shapes.begin(), shapes.end(),
            [&](auto& s) { return s->id == id; }), shapes.end());
        if (selectedShapeId == id) selectedShapeId.clear();
        notifyChange();
    }

    void selectShape(const std::string& id) {
        if (!selectedShapeId.empty()) {
            auto* s = getShapeById(selectedShapeId);
            if (s) s->hasSelectionChanged = true;
        }
        if (!id.empty()) {
            auto* s = getShapeById(id);
            if (s) s->hasSelectionChanged = true;
        }
        selectedShapeId = id;
        notifyChange();
    }

    void selectShapeNull() {
        if (!selectedShapeId.empty()) {
            auto* s = getShapeById(selectedShapeId);
            if (s) s->hasSelectionChanged = true;
        }
        selectedShapeId.clear();
        notifyChange();
    }

    template<typename T>
    void updateShape(const std::string& id, const T& updates) {
        auto* s = getShapeById(id);
        if (s) {
            applyShapeUpdate(s, updates);
            s->hasChanged = true;
            notifyChange();
        }
    }

    void clearAll() {
        vertices.clear();
        shapes.clear();
        selectedShapeId.clear();
        selectedVertexId.clear();
        nextId = 0;
        notifyChange();
    }

    void clearShapes() { clearAll(); }

    void resetChangeFlags() {
        for (auto& s : shapes) { s->hasChanged = false; s->hasSelectionChanged = false; }
        for (auto& v : vertices) v.hasChanged = false;
    }

    // Arc endpoint operations
    void updateArcEndpoint(const std::string& arcId, bool isStart, const Vec3& position);
    void slideArcEndpointOnCircle(const std::string& arcId, bool isStart, const Vec3& position);
    void updateArcRadius(const std::string& arcId, float scale);

    int shapeCount() const { return (int)shapes.size(); }

private:
    int nextId = 0;

    void markShapeIfUsesVertex(BaseShape& shape, const std::string& vid) {
        if (shape.type == ShapeType::LineSegment) {
            auto& ls = static_cast<LineSegmentShape&>(shape);
            if (ls.startVertexId == vid || ls.endVertexId == vid)
                shape.hasChanged = true;
        } else if (shape.type == ShapeType::Rectangle || shape.type == ShapeType::Triangle || shape.type == ShapeType::Polygon) {
            auto* poly = static_cast<PolygonShape*>(&shape);
            for (auto& id : poly->vertexIds)
                if (id == vid) { shape.hasChanged = true; break; }
        } else if (shape.type == ShapeType::Circle) {
            auto& circ = static_cast<CircleShape&>(shape);
            if (circ.centerVertexId == vid) shape.hasChanged = true;
        } else if (shape.type == ShapeType::CircularArc) {
            auto& arc = static_cast<CircularArcShape&>(shape);
            if (arc.centerVertexId == vid || arc.startVertexId == vid || arc.endVertexId == vid)
                shape.hasChanged = true;
        }
    }

    // Generic shape update helpers
    void applyShapeUpdate(BaseShape* s, const Vec3& pos) { s->position = pos; }
    void applyShapeUpdate(BaseShape* s, const std::pair<std::string, Vec3>& update) {
        if (update.first == "position") s->position = update.second;
    }
};

// Specialization for position update
template<>
inline void GeometryStore::updateShape<Vec3>(const std::string& id, const Vec3& pos) {
    auto* s = getShapeById(id);
    if (s) { s->position = pos; s->hasChanged = true; notifyChange(); }
}

// Struct for rotation update
struct RotationUpdate { Vec3 rotation; };
template<>
inline void GeometryStore::updateShape<RotationUpdate>(const std::string& id, const RotationUpdate& upd) {
    auto* s = getShapeById(id);
    if (s) { s->rotation = upd.rotation; s->hasChanged = true; notifyChange(); }
}

// Struct for visibility update
struct VisibilityUpdate { bool visible; };
template<>
inline void GeometryStore::updateShape<VisibilityUpdate>(const std::string& id, const VisibilityUpdate& upd) {
    auto* s = getShapeById(id);
    if (s) { s->visible = upd.visible; s->hasChanged = true; notifyChange(); }
}

// Global store instance
extern GeometryStore g_store;
