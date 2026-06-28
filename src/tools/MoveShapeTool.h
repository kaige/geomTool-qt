#pragma once
#include "tools/BaseTool.h"

class ToolManager;

class MoveShapeTool : public BaseTool {
public:
    Vec3 dragStartWorldPos;
    Vec3 dragStartObjectPos;
    // For line/arc shapes
    Vec3 startVertexStart, endVertexStart, centerVertexStart;
    bool hasVertexPositions = false;

    ToolManager* tm;

    MoveShapeTool(ToolManager* t)
        : BaseTool("Move Shape", ToolType::MoveShape), tm(t) {}

    void onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
};
