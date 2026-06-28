#pragma once
#include "tools/BaseTool.h"
#include "Math3D.h"

class ToolManager;

class MoveLineEndpointTool : public BaseTool {
public:
    std::string lineId;
    bool isStart = false;
    Vec3 otherVertexStart;

    ToolManager* tm;

    MoveLineEndpointTool(ToolManager* t)
        : BaseTool("Move Line Endpoint", ToolType::MoveLineEndpoint), tm(t) {}

    void setEndpointInfo(const std::string& id, bool start) {
        lineId = id;
        isStart = start;
    }

    void onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
};
