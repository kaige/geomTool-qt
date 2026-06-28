#pragma once
#include "Types.h"
#include "Camera.h"
#include <QPointF>
#include <QKeyEvent>

class CanvasWidget;

// ============================================================
// Base Tool
// ============================================================
class BaseTool {
public:
    std::string name;
    bool isActive = false;
    ToolType toolType;

    BaseTool(const std::string& n, ToolType tt) : name(n), toolType(tt) {}
    virtual ~BaseTool() = default;

    virtual void activate() { isActive = true; }
    virtual void deactivate() { isActive = false; }

    virtual void onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) {}
    virtual void onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) {}
    virtual void onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) {}
    virtual void onWheel(int delta, CanvasWidget* canvas) {}
    virtual void onKeyDown(QKeyEvent* event, CanvasWidget* canvas) {}
};

// Forward declarations of tools
class ToolManager;
class SelectTool;
class MoveShapeTool;
class RotateShapeTool;
class CreateShape3DTool;
class LineSegmentTool;
class CircularArcTool;
class MoveLineEndpointTool;
class MoveArcEndpointTool;
class MoveArcTool;
