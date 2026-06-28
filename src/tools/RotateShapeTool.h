#pragma once
#include "tools/BaseTool.h"

class ToolManager;

class RotateShapeTool : public BaseTool {
public:
    Vec3 dragStartRotation;
    char axis = 0;
    ToolManager* tm;

    RotateShapeTool(ToolManager* t)
        : BaseTool("Rotate Shape", ToolType::RotateShape), tm(t) {}

    void setRotationAxis(char a) { axis = a; }

    void onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
};
