#pragma once
#include "tools/BaseTool.h"
#include <memory>
#include <map>

class CanvasWidget;

// ============================================================
// ToolManager - manages all tools and dispatches events
// ============================================================
class ToolManager {
public:
    CanvasWidget* canvas;
    ToolType currentToolType = ToolType::Select;
    BaseTool* currentTool = nullptr;

    // Tools
    std::unique_ptr<SelectTool> selectTool;
    std::unique_ptr<MoveShapeTool> moveShapeTool;
    std::unique_ptr<RotateShapeTool> rotateShapeTool;
    std::unique_ptr<MoveLineEndpointTool> moveLineEndpointTool;
    std::unique_ptr<CircularArcTool> circularArcTool;
    std::unique_ptr<MoveArcEndpointTool> moveArcEndpointTool;
    std::unique_ptr<MoveArcTool> moveArcTool;
    std::unique_ptr<LineSegmentTool> lineSegmentTool;
    std::unique_ptr<CreateShape3DTool> createSphereTool;
    std::unique_ptr<CreateShape3DTool> createCubeTool;
    std::unique_ptr<CreateShape3DTool> createCylinderTool;
    std::unique_ptr<CreateShape3DTool> createConeTool;
    std::unique_ptr<CreateShape3DTool> createTorusTool;

    explicit ToolManager(CanvasWidget* canvas);
    ~ToolManager();

    void activateTool(ToolType type);
    void deactivateCurrentTool();

    void handleMouseDown(QPointF pos, Qt::KeyboardModifiers mods);
    void handleMouseMove(QPointF pos, Qt::KeyboardModifiers mods);
    void handleMouseUp(QPointF pos, Qt::KeyboardModifiers mods);
    void handleWheel(int delta);
    void handleKeyDown(QKeyEvent* event);
};
