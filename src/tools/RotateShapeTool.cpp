#include "tools/RotateShapeTool.h"
#include "tools/ToolManager.h"
#include "CanvasWidget.h"
#include "GeometryStore.h"

void RotateShapeTool::onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    cv->isRotatingObject = true;
    cv->setCanvasCursor(Qt::CrossCursor);

    BaseShape* sel = g_store.getSelectedShape();
    if (sel) dragStartRotation = sel->rotation;
}

void RotateShapeTool::onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    if (!cv->isRotatingObject || axis == 0) return;

    BaseShape* sel = g_store.getSelectedShape();
    if (!sel) return;

    float rotSensitivity = 0.01f;
    float dx = (pos.x() - cv->startMousePos.x()) * rotSensitivity;
    float dy = (pos.y() - cv->startMousePos.y()) * rotSensitivity;

    float rotationDelta = 0;
    switch (axis) {
        case 'x': rotationDelta = dy; break;
        case 'y': rotationDelta = dx; break;
        case 'z': rotationDelta = -dx; break;
    }

    Vec3 newRot = dragStartRotation;
    switch (axis) {
        case 'x': newRot.x += rotationDelta; break;
        case 'y': newRot.y += rotationDelta; break;
        case 'z': newRot.z += rotationDelta; break;
    }

    RotationUpdate upd;
    upd.rotation = newRot;
    g_store.updateShape(sel->id, upd);
}

void RotateShapeTool::onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    cv->isRotatingObject = false;
    axis = 0;
    cv->setCanvasCursor(Qt::OpenHandCursor);
    tm->activateTool(ToolType::Select);
}
