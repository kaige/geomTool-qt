#pragma once
#include "tools/BaseTool.h"
#include "Math3D.h"

class ToolManager;

class MoveArcEndpointTool : public BaseTool {
public:
    std::string arcId;
    bool isStart = false;

    ToolManager* tm;

    MoveArcEndpointTool(ToolManager* t)
        : BaseTool("Move Arc Endpoint", ToolType::MoveArcEndpoint), tm(t) {}

    void setEndpointInfo(const std::string& id, bool start) {
        arcId = id;
        isStart = start;
    }

    void onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
};
