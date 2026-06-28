#include "tools/CreateShape3DTool.h"
#include "tools/ToolManager.h"
#include "CanvasWidget.h"
#include "GeometryStore.h"
#include <QDateTime>

void CreateShape3DTool::onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    mouseDownTime = QDateTime::currentMSecsSinceEpoch();
    cv->setCanvasCursor(Qt::CrossCursor);
}

void CreateShape3DTool::onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - mouseDownTime;
    if (elapsed < CLICK_THRESHOLD) {
        Vec3 worldPos = cv->screenToWorldOnZ0Plane(pos.x(), pos.y());
        g_store.addShape3D(shapeType, {worldPos.x, 1, worldPos.y});
    }
}

void CreateShape3DTool::onKeyDown(QKeyEvent* event, CanvasWidget* cv) {
    if (event->key() == Qt::Key_Escape) {
        cv->toolManager->activateTool(ToolType::Select);
    }
}
