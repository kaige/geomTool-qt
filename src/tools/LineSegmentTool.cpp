#include "tools/LineSegmentTool.h"
#include "tools/ToolManager.h"
#include "CanvasWidget.h"
#include "GeometryStore.h"
#include <QDateTime>
#include <cmath>

const float LineSegmentTool::DEFAULT_LINE_LENGTH = 2.0f;

void LineSegmentTool::onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    mouseDownTime = QDateTime::currentMSecsSinceEpoch();
    mouseDownPos = pos;
    cv->setCanvasCursor(Qt::CrossCursor);

    auto snapResult = cv->snapManager->findSnapPoint(pos, cv);
    startPoint = snapResult.snappedPosition;
    hasStart = true;
    isDragging = false;
}

void LineSegmentTool::onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    auto snapResult = cv->snapManager->findSnapPoint(pos, cv);

    // Update snap marker
    cv->snapVisible = snapResult.snapped;
    cv->snapPosition = snapResult.snappedPosition;
    cv->snapType = snapResult.snapType;
    cv->update();

    if (!cv->mouseDown || !hasStart) return;

    // Check if dragged enough
    float dx = pos.x() - mouseDownPos.x();
    float dy = pos.y() - mouseDownPos.y();
    if (std::sqrt(dx*dx + dy*dy) > MOVE_THRESHOLD)
        isDragging = true;

    if (isDragging) {
        // Show rubber band line
        cv->clearTempLines();
        CanvasWidget::TempLine tl;
        tl.points = {startPoint, snapResult.snappedPosition};
        tl.color = QColor(255, 107, 53);
        cv->tempLines.push_back(tl);
        cv->update();
    }
}

void LineSegmentTool::onMouseUp(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    if (!hasStart) return;

    auto snapResult = cv->snapManager->findSnapPoint(pos, cv);
    Vec3 snappedPos = snapResult.snappedPosition;

    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - mouseDownTime;

    if (!isDragging && elapsed < CLICK_THRESHOLD) {
        // Quick click - default length line centered at click
        float half = DEFAULT_LINE_LENGTH / 2;
        g_store.addLineSegment(
            {startPoint.x - half, startPoint.y, startPoint.z},
            {startPoint.x + half, startPoint.y, startPoint.z}
        );
    } else if (isDragging) {
        // Drag - line from start to end
        g_store.addLineSegment(startPoint, snappedPos);
    } else {
        float half = DEFAULT_LINE_LENGTH / 2;
        g_store.addLineSegment(
            {startPoint.x - half, startPoint.y, startPoint.z},
            {startPoint.x + half, startPoint.y, startPoint.z}
        );
    }

    cv->clearTempLines();
    cv->snapVisible = false;
    hasStart = false;
    isDragging = false;
}

void LineSegmentTool::onKeyDown(QKeyEvent* event, CanvasWidget* cv) {
    if (event->key() == Qt::Key_Escape) {
        cv->clearTempLines();
        cv->snapVisible = false;
        hasStart = false;
        isDragging = false;
        tm->activateTool(ToolType::Select);
    }
}
