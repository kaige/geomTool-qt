#include "tools/MoveArcTool.h"
#include "tools/ToolManager.h"
#include "CanvasWidget.h"
#include "GeometryStore.h"
#include <cmath>

void MoveArcTool::onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    cv->isDraggingObject = true;
    cv->setCanvasCursor(Qt::ClosedHandCursor);

    BaseShape* sel = g_store.getSelectedShape();
    if (!sel || sel->type != ShapeType::CircularArc) return;
    auto* arc = static_cast<CircularArcShape*>(sel);

    Vertex* cv_ = g_store.getVertexById(arc->centerVertexId);
    Vertex* sv = g_store.getVertexById(arc->startVertexId);
    if (!cv_ || !sv) return;

    dragStartWorldPos = cv->screenToWorldOnZ0Plane(pos.x(), pos.y());
    dragStartRadius = sv->position.distanceTo(cv_->position);
}

void MoveArcTool::onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    if (!cv->isDraggingObject) return;

    BaseShape* sel = g_store.getSelectedShape();
    if (!sel || sel->type != ShapeType::CircularArc) return;
    auto* arc = static_cast<CircularArcShape*>(sel);

    Vec3 currentPos = cv->screenToWorldOnZ0Plane(pos.x(), pos.y());
    Vec3 delta = currentPos - dragStartWorldPos;

    // Scale the radius based on drag distance from original center
    Vertex* cv_ = g_store.getVertexById(arc->centerVertexId);
    if (!cv_) return;

    // Calculate new radius scale factor
    float currentDist = currentPos.distanceTo(cv_->position);
    if (dragStartRadius > 0.001f) {
        float scaleFactor = 1.0f + (delta.x + delta.y) * 0.5f / dragStartRadius;
        // Use a simpler approach: drag distance from center controls radius
        float newRadius = dragStartRadius + delta.x;
        if (newRadius > 0.1f) {
            float scale = newRadius / dragStartRadius;
            g_store.updateArcRadius(arc->id, scale);
        }
    }
}

void MoveArcTool::onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    cv->isDraggingObject = false;
    cv->setCanvasCursor(Qt::ArrowCursor);
    tm->activateTool(ToolType::Select);
}
