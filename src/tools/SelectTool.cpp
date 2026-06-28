#include "tools/SelectTool.h"
#include "tools/ToolManager.h"
#include "tools/MoveShapeTool.h"
#include "tools/RotateShapeTool.h"
#include "tools/MoveLineEndpointTool.h"
#include "tools/MoveArcEndpointTool.h"
#include "tools/MoveArcTool.h"
#include "CanvasWidget.h"
#include "GeometryStore.h"
#include <cmath>

void SelectTool::onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    float sx = pos.x(), sy = pos.y();

    // Check for line endpoint click
    std::string lineId;
    bool isStart;
    if (cv->hitTestLineEndpoint(sx, sy, lineId, isStart)) {
        auto* tool = tm->moveLineEndpointTool.get();
        tool->setEndpointInfo(lineId, isStart);
        tm->activateTool(ToolType::MoveLineEndpoint);
        tool->onMouseDown(pos, mods, cv);
        return;
    }

    // Check for arc point click
    std::string arcId;
    int pointType;
    if (cv->hitTestArcPoint(sx, sy, arcId, pointType)) {
        if (pointType == 0) {
            // Center - change radius
            tm->activateTool(ToolType::MoveArc);
            tm->moveArcTool->onMouseDown(pos, mods, cv);
        } else {
            // Start/End endpoint
            auto* tool = tm->moveArcEndpointTool.get();
            tool->setEndpointInfo(arcId, pointType == 1);
            tm->activateTool(ToolType::MoveArcEndpoint);
            tool->onMouseDown(pos, mods, cv);
        }
        return;
    }

    // Check if clicking on selected shape (for move/rotate)
    std::string clickedId = cv->hitTestShape(sx, sy);
    if (!g_store.selectedShapeId.empty() && clickedId == g_store.selectedShapeId) {
        char axis = 0;
        if (mods & Qt::AltModifier) axis = 'x';
        else if (mods & Qt::ShiftModifier) axis = 'y';
        else if (mods & Qt::ControlModifier) axis = 'z';

        if (axis) {
            auto* tool = tm->rotateShapeTool.get();
            tool->setRotationAxis(axis);
            tm->activateTool(ToolType::RotateShape);
            tool->onMouseDown(pos, mods, cv);
        } else {
            tm->activateTool(ToolType::MoveShape);
            tm->moveShapeTool->onMouseDown(pos, mods, cv);
        }
        return;
    }

    // If clicked on another shape, select it
    if (!clickedId.empty()) {
        g_store.selectShape(clickedId);
        return;
    }

    // Camera operations
    if (mods & Qt::ControlModifier) {
        // Rotate camera
        cv->isRotatingCamera = true;
        cv->cameraRotationStartAzimuth = cv->camera.azimuth;
        cv->cameraRotationStartElevation = cv->camera.elevation;
        cv->setCanvasCursor(Qt::ClosedHandCursor);
    }
}

void SelectTool::onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    if (!cv->mouseDown) {
        // Hover cursor feedback
        float sx = pos.x(), sy = pos.y();

        std::string lid;
        bool isS;
        if (cv->hitTestLineEndpoint(sx, sy, lid, isS)) {
            cv->setCanvasCursor(Qt::CrossCursor);
            return;
        }
        std::string aId;
        int pt;
        if (cv->hitTestArcPoint(sx, sy, aId, pt)) {
            cv->setCanvasCursor(Qt::CrossCursor);
            return;
        }
        std::string hovered = cv->hitTestShape(sx, sy);
        if (!hovered.empty() && hovered == g_store.selectedShapeId)
            cv->setCanvasCursor(Qt::OpenHandCursor);
        else
            cv->setCanvasCursor(Qt::ArrowCursor);
        return;
    }

    // Camera rotation
    if (cv->isRotatingCamera) {
        float dx = (pos.x() - cv->startMousePos.x()) * 0.005f;
        float dy = (pos.y() - cv->startMousePos.y()) * 0.005f;
        cv->camera.azimuth = cv->cameraRotationStartAzimuth - dx;
        cv->camera.elevation = clampf(cv->cameraRotationStartElevation + dy,
                                       -M_PI_2 + 0.1f, M_PI_2 - 0.1f);
        cv->update();
        return;
    }

    // Camera pan (when no object is being dragged and no active interaction tool)
    if (!cv->isDraggingObject && !cv->isDraggingEndpoint) {
        float dx = (pos.x() - cv->mousePos.x()) / cv->width();
        float dy = (pos.y() - cv->mousePos.y()) / cv->height();
        cv->camera.pan(dx, -dy);
        cv->update();
    }
}

void SelectTool::onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    float dx = std::abs(pos.x() - cv->startMousePos.x());
    float dy = std::abs(pos.y() - cv->startMousePos.y());

    if (dx < 5 && dy < 5 && !cv->isRotatingCamera) {
        handleClick(pos.x(), pos.y());
    }

    cv->isRotatingCamera = false;
    cv->setCanvasCursor(Qt::ArrowCursor);
}

void SelectTool::onWheel(int delta, CanvasWidget* cv) {
    cv->camera.zoom(delta > 0 ? -1 : 1);
    cv->update();
}

void SelectTool::onKeyDown(QKeyEvent* event, CanvasWidget* cv) {
    if (event->key() == Qt::Key_Escape) {
        cv->camera.reset();
        cv->update();
    }
}

void SelectTool::handleClick(float sx, float sy) {
    std::string clickedId = canvas->hitTestShape(sx, sy);
    if (!clickedId.empty())
        g_store.selectShape(clickedId);
    else
        g_store.selectShapeNull();
}
