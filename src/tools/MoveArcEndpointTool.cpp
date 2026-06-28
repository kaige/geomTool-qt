#include "tools/MoveArcEndpointTool.h"
#include "tools/ToolManager.h"
#include "CanvasWidget.h"
#include "GeometryStore.h"
#include "SnapManager.h"

void MoveArcEndpointTool::onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    cv->isDraggingEndpoint = true;
    cv->setCanvasCursor(Qt::ClosedHandCursor);
}

void MoveArcEndpointTool::onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    if (!cv->isDraggingEndpoint) return;

    Vec3 worldPos = cv->screenToWorldOnZ0Plane(pos.x(), pos.y());
    // Slide the endpoint on the circle (keep center and radius)
    g_store.slideArcEndpointOnCircle(arcId, isStart, worldPos);
}

void MoveArcEndpointTool::onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    cv->isDraggingEndpoint = false;
    cv->setCanvasCursor(Qt::ArrowCursor);
    tm->activateTool(ToolType::Select);
}
