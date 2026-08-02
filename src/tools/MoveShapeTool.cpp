#include "tools/MoveShapeTool.h"
#include "tools/ToolManager.h"
#include "CanvasWidget.h"
#include "GeometryStore.h"

void MoveShapeTool::onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    cv->isDraggingObject = true;
    cv->setCanvasCursor(Qt::ClosedHandCursor);

    BaseShape* sel = g_store.getSelectedShape();
    if (!sel) return;

    // Project to plane through shape position
    Vec3 planePoint = sel->position;
    Vec3 planeNormal = cv->camera.getForward();
    Vec3 worldPos = cv->screenToWorldOnPlane(pos.x(), pos.y(), planeNormal, planePoint);

    dragStartWorldPos = worldPos;
    dragStartObjectPos = sel->position;
    hasVertexPositions = false;

    if (sel->type == ShapeType::LineSegment) {
        auto* ls = static_cast<LineSegmentShape*>(sel);
        Vertex* sv = g_store.getVertexById(ls->startVertexId);
        Vertex* ev = g_store.getVertexById(ls->endVertexId);
        if (sv && ev) {
            startVertexStart = sv->position;
            endVertexStart = ev->position;
            hasVertexPositions = true;
        }
    } else if (sel->type == ShapeType::CircularArc) {
        auto* arc = static_cast<CircularArcShape*>(sel);
        Vertex* cv_ = g_store.getVertexById(arc->centerVertexId);
        Vertex* sv = g_store.getVertexById(arc->startVertexId);
        Vertex* ev = g_store.getVertexById(arc->endVertexId);
        if (cv_ && sv && ev) {
            centerVertexStart = cv_->position;
            startVertexStart = sv->position;
            endVertexStart = ev->position;
            hasVertexPositions = true;
        }
    }
}

void MoveShapeTool::onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    if (!cv->isDraggingObject) return;

    BaseShape* sel = g_store.getSelectedShape();
    if (!sel) return;

    Vec3 planePoint = dragStartObjectPos;
    Vec3 planeNormal = cv->camera.getForward();
    Vec3 currentPos = cv->screenToWorldOnPlane(pos.x(), pos.y(), planeNormal, planePoint);

    Vec3 delta = currentPos - dragStartWorldPos;

    if (sel->type == ShapeType::LineSegment && hasVertexPositions) {
        auto* ls = static_cast<LineSegmentShape*>(sel);
        g_store.updateVertex(ls->startVertexId, {
            startVertexStart.x + delta.x,
            startVertexStart.y + delta.y,
            startVertexStart.z + delta.z
        });
        g_store.updateVertex(ls->endVertexId, {
            endVertexStart.x + delta.x,
            endVertexStart.y + delta.y,
            endVertexStart.z + delta.z
        });
    } else if (sel->type == ShapeType::CircularArc && hasVertexPositions) {
        auto* arc = static_cast<CircularArcShape*>(sel);
        g_store.updateVertex(arc->centerVertexId, {
            centerVertexStart.x + delta.x,
            centerVertexStart.y + delta.y,
            centerVertexStart.z + delta.z
        });
        g_store.updateVertex(arc->startVertexId, {
            startVertexStart.x + delta.x,
            startVertexStart.y + delta.y,
            startVertexStart.z + delta.z
        });
        g_store.updateVertex(arc->endVertexId, {
            endVertexStart.x + delta.x,
            endVertexStart.y + delta.y,
            endVertexStart.z + delta.z
        });
    } else {
        // 3D shape — update position directly without notifyChange() to avoid
        // rebuilding the sidebar table on every mouse move (causes severe lag).
        // The canvas repaint is handled by CanvasWidget::mouseMoveEvent.
        sel->position = Vec3(
            dragStartObjectPos.x + delta.x,
            dragStartObjectPos.y + delta.y,
            dragStartObjectPos.z + delta.z
        );
        sel->hasChanged = true;
    }
}

void MoveShapeTool::onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    cv->isDraggingObject = false;
    cv->setCanvasCursor(Qt::OpenHandCursor);
    tm->activateTool(ToolType::Select);
}
