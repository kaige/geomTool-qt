#pragma once
#include "tools/BaseTool.h"
#include "Math3D.h"
#include "SnapManager.h"

class ToolManager;

class LineSegmentTool : public BaseTool {
public:
    Vec3 startPoint;
    bool hasStart = false;
    bool isDragging = false;
    qint64 mouseDownTime = 0;
    QPointF mouseDownPos;
    static const int CLICK_THRESHOLD = 150;
    static const int MOVE_THRESHOLD = 5;
    static const float DEFAULT_LINE_LENGTH;

    ToolManager* tm;

    LineSegmentTool(ToolManager* t)
        : BaseTool("Line Segment", ToolType::CreateLineSegment), tm(t) {}

    void activate() override { BaseTool::activate(); hasStart = false; isDragging = false; }
    void deactivate() override { BaseTool::deactivate(); hasStart = false; isDragging = false; }

    void onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onKeyDown(QKeyEvent* event, CanvasWidget* canvas) override;
};
