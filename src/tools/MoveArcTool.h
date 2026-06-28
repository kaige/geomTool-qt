#pragma once
#include "tools/BaseTool.h"

class ToolManager;

class MoveArcTool : public BaseTool {
public:
    Vec3 dragStartWorldPos;
    float dragStartRadius;

    ToolManager* tm;

    MoveArcTool(ToolManager* t)
        : BaseTool("Move Arc Center", ToolType::MoveArc), tm(t) {}

    void onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
};
