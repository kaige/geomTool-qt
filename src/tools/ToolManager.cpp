#include "tools/ToolManager.h"
#include "tools/SelectTool.h"
#include "tools/MoveShapeTool.h"
#include "tools/RotateShapeTool.h"
#include "tools/MoveLineEndpointTool.h"
#include "tools/CircularArcTool.h"
#include "tools/MoveArcEndpointTool.h"
#include "tools/MoveArcTool.h"
#include "tools/LineSegmentTool.h"
#include "tools/CreateShape3DTool.h"
#include "CanvasWidget.h"
#include "GeometryStore.h"

ToolManager::ToolManager(CanvasWidget* c) : canvas(c) {
    selectTool = std::make_unique<SelectTool>(canvas, this);
    moveShapeTool = std::make_unique<MoveShapeTool>(this);
    rotateShapeTool = std::make_unique<RotateShapeTool>(this);
    moveLineEndpointTool = std::make_unique<MoveLineEndpointTool>(this);
    circularArcTool = std::make_unique<CircularArcTool>(this);
    moveArcEndpointTool = std::make_unique<MoveArcEndpointTool>(this);
    moveArcTool = std::make_unique<MoveArcTool>(this);
    lineSegmentTool = std::make_unique<LineSegmentTool>(this);
    createSphereTool = std::make_unique<CreateShape3DTool>(ShapeType::Sphere, this);
    createCubeTool = std::make_unique<CreateShape3DTool>(ShapeType::Cube, this);
    createCylinderTool = std::make_unique<CreateShape3DTool>(ShapeType::Cylinder, this);
    createConeTool = std::make_unique<CreateShape3DTool>(ShapeType::Cone, this);
    createTorusTool = std::make_unique<CreateShape3DTool>(ShapeType::Torus, this);

    activateTool(ToolType::Select);
}

ToolManager::~ToolManager() = default;

void ToolManager::activateTool(ToolType type) {
    if (currentTool) currentTool->deactivate();

    currentToolType = type;
    switch (type) {
        case ToolType::Select:             currentTool = selectTool.get(); break;
        case ToolType::MoveShape:          currentTool = moveShapeTool.get(); break;
        case ToolType::RotateShape:        currentTool = rotateShapeTool.get(); break;
        case ToolType::MoveLineEndpoint:   currentTool = moveLineEndpointTool.get(); break;
        case ToolType::CreateCircularArc:  currentTool = circularArcTool.get(); break;
        case ToolType::MoveArcEndpoint:    currentTool = moveArcEndpointTool.get(); break;
        case ToolType::MoveArc:            currentTool = moveArcTool.get(); break;
        case ToolType::CreateLineSegment:  currentTool = lineSegmentTool.get(); break;
        case ToolType::CreateSphere:       currentTool = createSphereTool.get(); break;
        case ToolType::CreateCube:         currentTool = createCubeTool.get(); break;
        case ToolType::CreateCylinder:     currentTool = createCylinderTool.get(); break;
        case ToolType::CreateCone:         currentTool = createConeTool.get(); break;
        case ToolType::CreateTorus:        currentTool = createTorusTool.get(); break;
    }

    if (currentTool) currentTool->activate();
    g_store.setActiveToolType(type);
}

void ToolManager::deactivateCurrentTool() {
    activateTool(ToolType::Select);
}

void ToolManager::handleMouseDown(QPointF pos, Qt::KeyboardModifiers mods) {
    if (currentTool) currentTool->onMouseDown(pos, mods, canvas);
}

void ToolManager::handleMouseMove(QPointF pos, Qt::KeyboardModifiers mods) {
    if (currentTool) currentTool->onMouseMove(pos, mods, canvas);
}

void ToolManager::handleMouseUp(QPointF pos, Qt::KeyboardModifiers mods) {
    if (currentTool) currentTool->onMouseUp(pos, mods, canvas);
}

void ToolManager::handleWheel(int delta) {
    if (currentTool) currentTool->onWheel(delta, canvas);
}

void ToolManager::handleKeyDown(QKeyEvent* event) {
    if (currentTool) currentTool->onKeyDown(event, canvas);
}
