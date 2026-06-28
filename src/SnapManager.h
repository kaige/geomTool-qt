#pragma once
#include "Math3D.h"
#include "GeometryStore.h"
#include "Types.h"

// ============================================================
// SnapManager - Snap to endpoints, midpoints, arc centers
// ============================================================

struct SnapPoint {
    Vec3 position;
    int type; // 0=endpoint, 1=arc_center, 2=midpoint
    std::string sourceShapeId;
};

class SnapManager {
public:
    float snapThreshold = 0.5f;

    struct SnapResult {
        Vec3 snappedPosition;
        bool snapped = false;
        int snapType = -1; // -1 = none, 0=endpoint, 1=center, 2=midpoint
    };

    SnapResult findSnapPoint(const Vec3& worldPos) {
        auto snapPoints = collectAllSnapPoints();
        SnapResult result;
        result.snappedPosition = worldPos;
        result.snapped = false;

        float minDist = snapThreshold;
        const SnapPoint* closest = nullptr;

        for (auto& sp : snapPoints) {
            float dist = worldPos.distanceTo(sp.position);
            if (dist < minDist) {
                minDist = dist;
                closest = &sp;
            }
        }

        if (closest) {
            result.snappedPosition = closest->position;
            result.snapped = true;
            result.snapType = closest->type;
        }

        return result;
    }

    void resetVisualState() {}

private:
    std::vector<SnapPoint> collectAllSnapPoints() {
        std::vector<SnapPoint> points;

        for (auto& shape : g_store.shapes) {
            if (shape->type == ShapeType::LineSegment) {
                auto* ls = static_cast<LineSegmentShape*>(shape.get());
                Vertex* sv = g_store.getVertexById(ls->startVertexId);
                Vertex* ev = g_store.getVertexById(ls->endVertexId);
                if (sv && ev) {
                    points.push_back({sv->position, 0, ls->id});
                    points.push_back({ev->position, 0, ls->id});
                    Vec3 mid = {(sv->position.x + ev->position.x)/2,
                                (sv->position.y + ev->position.y)/2,
                                (sv->position.z + ev->position.z)/2};
                    points.push_back({mid, 2, ls->id});
                }
            } else if (shape->type == ShapeType::CircularArc) {
                auto* arc = static_cast<CircularArcShape*>(shape.get());
                Vertex* cv = g_store.getVertexById(arc->centerVertexId);
                Vertex* sv = g_store.getVertexById(arc->startVertexId);
                Vertex* ev = g_store.getVertexById(arc->endVertexId);
                if (cv && sv && ev) {
                    points.push_back({cv->position, 1, arc->id});
                    points.push_back({sv->position, 0, arc->id});
                    points.push_back({ev->position, 0, arc->id});
                    Vec3 mid = {(sv->position.x + ev->position.x)/2,
                                (sv->position.y + ev->position.y)/2,
                                (sv->position.z + ev->position.z)/2};
                    points.push_back({mid, 2, arc->id});
                }
            }
        }
        return points;
    }
};
