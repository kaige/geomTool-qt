#pragma once
#include "tools/BaseTool.h"
#include "Math3D.h"

class ToolManager;

class CreateShape3DTool : public BaseTool {
public:
    ShapeType shapeType;
    qint64 mouseDownTime = 0;
    static const int CLICK_THRESHOLD = 300; // ms

    CreateShape3DTool(ShapeType st, ToolManager* t)
        : BaseTool("Create3D", ToolType::CreateSphere), shapeType(st) {
        switch (st) {
            case ShapeType::Sphere:   toolType = ToolType::CreateSphere; break;
            case ShapeType::Cube:     toolType = ToolType::CreateCube; break;
            case ShapeType::Cylinder: toolType = ToolType::CreateCylinder; break;
            case ShapeType::Cone:     toolType = ToolType::CreateCone; break;
            case ShapeType::Torus:    toolType = ToolType::CreateTorus; break;
            default: break;
        }
    }

    void onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onKeyDown(QKeyEvent* event, CanvasWidget* canvas) override;
};
