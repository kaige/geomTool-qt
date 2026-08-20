#include "tools/MoveLineEndpointTool.h"
#include "tools/ToolManager.h"
#include "CanvasWidget.h"
#include "GeometryStore.h"
#include "SnapManager.h"

void MoveLineEndpointTool::onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    cv->isDraggingEndpoint = true;
    cv->setCanvasCursor(Qt::ClosedHandCursor);

    // Record the other endpoint position
    BaseShape* shape = g_store.getShapeById(lineId);
    if (!shape || shape->type != ShapeType::LineSegment) return;
    auto* ls = static_cast<LineSegmentShape*>(shape);
    std::string otherId = isStart ? ls->endVertexId : ls->startVertexId;
    Vertex* other = g_store.getVertexById(otherId);
    if (other) otherVertexStart = other->position;
}

void MoveLineEndpointTool::onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    if (!cv->isDraggingEndpoint) return;

    BaseShape* shape = g_store.getShapeById(lineId);
    if (!shape || shape->type != ShapeType::LineSegment) return;
    auto* ls = static_cast<LineSegmentShape*>(shape);
    std::string vertexId = isStart ? ls->startVertexId : ls->endVertexId;

    auto snapResult = cv->snapManager->findSnapPoint(pos, cv);

    // Snap positioning is used, but the green marker is NOT shown during drag
    // to keep the UI clean (consistent with arc endpoint dragging).
    cv->snapVisible = false;

    g_store.updateVertex(vertexId, snapResult.snappedPosition);
}

void MoveLineEndpointTool::onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    cv->isDraggingEndpoint = false;
    cv->snapVisible = false;
    cv->setCanvasCursor(Qt::ArrowCursor);
    tm->activateTool(ToolType::Select);
}
