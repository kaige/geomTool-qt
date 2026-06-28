#pragma once
#include "tools/BaseTool.h"
#include "Math3D.h"

class ToolManager;

class CircularArcTool : public BaseTool {
public:
    enum class Step { PickStart, PickEnd, PickArcPoint };
    Step step = Step::PickStart;
    Vec3 startPoint, endPoint, arcPoint;
    bool hasStart = false, hasEnd = false;

    ToolManager* tm;

    CircularArcTool(ToolManager* t)
        : BaseTool("Circular Arc", ToolType::CreateCircularArc), tm(t) {}

    void activate() override;
    void deactivate() override;

    void onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override;
    void onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* canvas) override {}
    void onKeyDown(QKeyEvent* event, CanvasWidget* canvas) override;

private:
    std::vector<Vec3> generateArcPoints(Vec3 start, Vec3 end, Vec3 arc);
    Vec3 calculateArcCenter(Vec3 start, Vec3 end, Vec3 arc);
};
