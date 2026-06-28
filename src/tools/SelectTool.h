#pragma once
#include "tools/BaseTool.h"

class CanvasWidget;
class ToolManager;

class SelectTool : public BaseTool {
public:
    CanvasWidget* canvas;
    ToolManager* tm;

    SelectTool(CanvasWidget* c, ToolManager* t)
        : BaseTool("Select", ToolType::Select), canvas(c), tm(t) {}

    void onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onWheel(int delta, CanvasWidget* canvas) override;
    void onKeyDown(QKeyEvent* event, CanvasWidget* canvas) override;

private:
    void handleClick(float sx, float sy);
};
