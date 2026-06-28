#include "tools/CircularArcTool.h"
#include "tools/ToolManager.h"
#include "CanvasWidget.h"
#include "GeometryStore.h"
#include "SnapManager.h"
#include <cmath>

void CircularArcTool::activate() {
    BaseTool::activate();
    step = Step::PickStart;
    hasStart = false;
    hasEnd = false;
}

void CircularArcTool::deactivate() {
    BaseTool::deactivate();
    step = Step::PickStart;
    hasStart = false;
    hasEnd = false;
}

void CircularArcTool::onMouseDown(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    Vec3 worldPos = cv->screenToWorldOnZ0Plane(pos.x(), pos.y());
    auto snapResult = cv->snapManager->findSnapPoint(worldPos);
    Vec3 snappedPos = snapResult.snappedPosition;

    cv->snapVisible = snapResult.snapped;
    cv->snapPosition = snappedPos;
    cv->snapType = snapResult.snapType;

    if (step == Step::PickStart) {
        startPoint = snappedPos;
        hasStart = true;
        step = Step::PickEnd;
    } else if (step == Step::PickEnd) {
        endPoint = snappedPos;
        hasEnd = true;
        step = Step::PickArcPoint;
    } else if (step == Step::PickArcPoint) {
        g_store.addCircularArc(startPoint, endPoint, snappedPos);
        // Reset for next arc
        cv->clearTempLines();
        cv->snapVisible = false;
        step = Step::PickStart;
        hasStart = false;
        hasEnd = false;
    }
}

void CircularArcTool::onMouseMove(QPointF pos, Qt::KeyboardModifiers mods, CanvasWidget* cv) {
    Vec3 worldPos = cv->screenToWorldOnZ0Plane(pos.x(), pos.y());
    auto snapResult = cv->snapManager->findSnapPoint(worldPos);
    Vec3 snappedPos = snapResult.snappedPosition;

    cv->snapVisible = snapResult.snapped;
    cv->snapPosition = snappedPos;
    cv->snapType = snapResult.snapType;

    cv->clearTempLines();

    if (step == Step::PickEnd && hasStart) {
        // Show line from start to mouse
        CanvasWidget::TempLine tl;
        tl.points = {startPoint, snappedPos};
        tl.color = QColor(255, 107, 53);
        tl.dashed = true;
        cv->tempLines.push_back(tl);
    } else if (step == Step::PickArcPoint && hasStart && hasEnd) {
        // Show line from start to end
        CanvasWidget::TempLine tl;
        tl.points = {startPoint, endPoint};
        tl.color = QColor(255, 107, 53);
        tl.dashed = true;
        cv->tempLines.push_back(tl);

        // Show temp arc
        auto arcPts = generateArcPoints(startPoint, endPoint, snappedPos);
        CanvasWidget::TempLine tl2;
        tl2.points = arcPts;
        tl2.color = QColor(255, 107, 53);
        cv->tempLines.push_back(tl2);

        // Dotted line from end to mouse
        CanvasWidget::TempLine tl3;
        tl3.points = {endPoint, snappedPos};
        tl3.color = QColor(150, 150, 150);
        tl3.dashed = true;
        cv->tempLines.push_back(tl3);
    }

    cv->update();
}

void CircularArcTool::onKeyDown(QKeyEvent* event, CanvasWidget* cv) {
    if (event->key() == Qt::Key_Escape) {
        cv->clearTempLines();
        cv->snapVisible = false;
        deactivate();
        tm->activateTool(ToolType::Select);
    }
}

std::vector<Vec3> CircularArcTool::generateArcPoints(Vec3 start, Vec3 end, Vec3 arc) {
    std::vector<Vec3> points;
    Vec3 center = calculateArcCenter(start, end, arc);
    float radius = start.distanceTo(center);
    float startAngle = std::atan2(start.y - center.y, start.x - center.x);
    float endAngle = std::atan2(end.y - center.y, end.x - center.x);

    // Determine direction using cross product
    Vec3 v1 = {start.x - center.x, start.y - center.y, 0};
    Vec3 v2 = {arc.x - center.x, arc.y - center.y, 0};
    float cross = v1.x * v2.y - v1.y * v2.x;
    bool clockwise = cross < 0;

    float angleDiff = endAngle - startAngle;
    if (clockwise) {
        if (angleDiff > 0) angleDiff -= 2 * M_PI;
    } else {
        if (angleDiff < 0) angleDiff += 2 * M_PI;
    }

    int numPoints = 32;
    float angleStep = angleDiff / numPoints;
    for (int i = 0; i <= numPoints; i++) {
        float angle = startAngle + angleStep * i;
        points.push_back({
            center.x + radius * std::cos(angle),
            center.y + radius * std::sin(angle),
            0
        });
    }
    return points;
}

Vec3 CircularArcTool::calculateArcCenter(Vec3 start, Vec3 end, Vec3 arc) {
    float ax = start.x, ay = start.y;
    float bx = arc.x, by = arc.y;
    float cx = end.x, cy = end.y;

    float midABx = (ax + bx) / 2;
    float midABy = (ay + by) / 2;
    float midBCx = (bx + cx) / 2;
    float midBCy = (by + cy) / 2;

    float centerX, centerY;

    if (std::abs(bx - ax) < 1e-10f && std::abs(cx - bx) < 1e-10f) {
        centerX = (ax + cx) / 2;
        centerY = (midABy + midBCy) / 2;
    } else if (std::abs(bx - ax) < 1e-10f) {
        centerX = ax;
        float perpSlopeBC = -(cx - bx) / (cy - by);
        centerY = midBCy + perpSlopeBC * (centerX - midBCx);
    } else if (std::abs(cx - bx) < 1e-10f) {
        centerX = cx;
        float perpSlopeAB = -(bx - ax) / (by - ay);
        centerY = midABy + perpSlopeAB * (centerX - midABx);
    } else {
        float slopeAB = (by - ay) / (bx - ax);
        float slopeBC = (cy - by) / (cx - bx);
        if (std::abs(slopeAB - slopeBC) < 1e-10f) {
            float radius = std::sqrt((cx-ax)*(cx-ax) + (cy-ay)*(cy-ay)) / 2;
            float angle = std::atan2(cy - ay, cx - ax);
            centerX = (ax + cx) / 2 - radius * std::sin(angle);
            centerY = (ay + cy) / 2 + radius * std::cos(angle);
        } else {
            float perpSlopeAB = -(bx - ax) / (by - ay);
            float perpSlopeBC = -(cx - bx) / (cy - by);
            centerX = (perpSlopeAB * midABx - perpSlopeBC * midBCx + midBCy - midABy) / (perpSlopeAB - perpSlopeBC);
            centerY = perpSlopeAB * (centerX - midABx) + midABy;
        }
    }

    // Fallback: circumcircle
    float r1 = std::sqrt((ax-centerX)*(ax-centerX) + (ay-centerY)*(ay-centerY));
    float r2 = std::sqrt((bx-centerX)*(bx-centerX) + (by-centerY)*(by-centerY));
    float r3 = std::sqrt((cx-centerX)*(cx-centerX) + (cy-centerY)*(cy-centerY));
    float maxR = std::max({r1, r2, r3});
    float minR = std::min({r1, r2, r3});
    if (maxR - minR > maxR * 0.5f) {
        float d = 2 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
        if (std::abs(d) > 1e-10f) {
            centerX = ((ax*ax+ay*ay)*(by-cy) + (bx*bx+by*by)*(cy-ay) + (cx*cx+cy*cy)*(ay-by)) / d;
            centerY = ((ax*ax+ay*ay)*(cx-bx) + (bx*bx+by*by)*(ax-cx) + (cx*cx+cy*cy)*(bx-ax)) / d;
        }
    }

    return {centerX, centerY, 0};
}
