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
    auto snapResult = cv->snapManager->findSnapPoint(pos, cv);
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
    auto snapResult = cv->snapManager->findSnapPoint(pos, cv);
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
    float startAngle = std::atan2(start.z - center.z, start.x - center.x);
    float endAngle = std::atan2(end.z - center.z, end.x - center.x);

    // Determine direction using cross product in the XZ work plane
    Vec3 v1 = {start.x - center.x, 0, start.z - center.z};
    Vec3 v2 = {arc.x - center.x, 0, arc.z - center.z};
    float cross = v1.x * v2.z - v1.z * v2.x;
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
            0,
            center.z + radius * std::sin(angle)
        });
    }
    return points;
}

Vec3 CircularArcTool::calculateArcCenter(Vec3 start, Vec3 end, Vec3 arc) {
    // 2D calculation in the y=0 XZ work plane (planar pair: x, z)
    float ax = start.x, az = start.z;
    float bx = arc.x, bz = arc.z;
    float cx = end.x, cz = end.z;

    float midABx = (ax + bx) / 2;
    float midABz = (az + bz) / 2;
    float midBCx = (bx + cx) / 2;
    float midBCz = (bz + cz) / 2;

    float centerX, centerZ;

    if (std::abs(bx - ax) < 1e-10f && std::abs(cx - bx) < 1e-10f) {
        centerX = (ax + cx) / 2;
        centerZ = (midABz + midBCz) / 2;
    } else if (std::abs(bx - ax) < 1e-10f) {
        centerX = ax;
        float perpSlopeBC = -(cx - bx) / (cz - bz);
        centerZ = midBCz + perpSlopeBC * (centerX - midBCx);
    } else if (std::abs(cx - bx) < 1e-10f) {
        centerX = cx;
        float perpSlopeAB = -(bx - ax) / (bz - az);
        centerZ = midABz + perpSlopeAB * (centerX - midABx);
    } else {
        float slopeAB = (bz - az) / (bx - ax);
        float slopeBC = (cz - bz) / (cx - bx);
        if (std::abs(slopeAB - slopeBC) < 1e-10f) {
            float radius = std::sqrt((cx-ax)*(cx-ax) + (cz-az)*(cz-az)) / 2;
            float angle = std::atan2(cz - az, cx - ax);
            centerX = (ax + cx) / 2 - radius * std::sin(angle);
            centerZ = (az + cz) / 2 + radius * std::cos(angle);
        } else {
            float perpSlopeAB = -(bx - ax) / (bz - az);
            float perpSlopeBC = -(cx - bx) / (cz - bz);
            centerX = (perpSlopeAB * midABx - perpSlopeBC * midBCx + midBCz - midABz) / (perpSlopeAB - perpSlopeBC);
            centerZ = perpSlopeAB * (centerX - midABx) + midABz;
        }
    }

    // Fallback: circumcircle
    float r1 = std::sqrt((ax-centerX)*(ax-centerX) + (az-centerZ)*(az-centerZ));
    float r2 = std::sqrt((bx-centerX)*(bx-centerX) + (bz-centerZ)*(bz-centerZ));
    float r3 = std::sqrt((cx-centerX)*(cx-centerX) + (cz-centerZ)*(cz-centerZ));
    float maxR = std::max({r1, r2, r3});
    float minR = std::min({r1, r2, r3});
    if (maxR - minR > maxR * 0.5f) {
        float d = 2 * (ax * (bz - cz) + bx * (cz - az) + cx * (az - bz));
        if (std::abs(d) > 1e-10f) {
            centerX = ((ax*ax+az*az)*(bz-cz) + (bx*bx+bz*bz)*(cz-az) + (cx*cx+cz*cz)*(az-bz)) / d;
            centerZ = ((ax*ax+az*az)*(cx-bx) + (bx*bx+bz*bz)*(ax-cx) + (cx*cx+cz*cz)*(bx-ax)) / d;
        }
    }

    return {centerX, 0, centerZ};
}
