#pragma once
#include "Math3D.h"
#include "Types.h"
#include <QPointF>

class CanvasWidget;

// ============================================================
// SnapManager - snap points for all drawing tools
//
// Snapping is measured in SCREEN space: each candidate point is
// projected with the canvas camera and compared to the cursor in
// pixels. Points that live off the y=0 work plane (e.g. the back
// corners of a cube, a cone apex) snap just as naturally as
// on-plane points — the cursor only has to be visually on top of
// the point. This is the standard CAD behavior.
//
// Candidates:
//   line segments : endpoints, midpoint
//   arcs          : center, endpoints, chord midpoint
//   circles       : center
//   rect/tri/poly : corner vertices
//   3D solids     : feature points (local space, transformed by
//                   the shape's model matrix — see SnapManager.cpp):
//     cube      8 corners
//     cylinder  2 cap centers + 8 rim quadrant points
//     cone      apex + base center + 4 rim quadrant points
//     sphere    center + 2 poles
//     torus     center + 4 outer + 4 inner equator points
// ============================================================

struct SnapPoint {
    Vec3 position;
    int type; // 0=endpoint, 1=center, 2=midpoint, 3=vertex(feature point)
    std::string sourceShapeId;
};

class SnapManager {
public:
    // Snap radius in screen pixels (feels the same at every zoom level).
    float snapThreshold = 16.0f;

    struct SnapResult {
        Vec3 snappedPosition;   // the snapped 3D point, or the cursor's
                                // work-plane position when nothing snapped
        bool snapped = false;
        int snapType = -1; // -1 = none, 0=endpoint, 1=center, 2=midpoint, 3=vertex
    };

    // screenPos: cursor position in widget pixel coordinates.
    SnapResult findSnapPoint(const QPointF& screenPos, CanvasWidget* cv);

    void resetVisualState() {}

private:
    std::vector<SnapPoint> collectAllSnapPoints();
};
