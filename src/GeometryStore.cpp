#include "GeometryStore.h"

GeometryStore g_store;

// ============================================================
// Arc calculations (ported from GeometryStore.ts)
// ============================================================

static Vec3 calculateArcCenter(const Vec3& start, const Vec3& end, const Vec3& arc) {
    // 2D calculation (ignore z)
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

    // Fallback: circumcircle formula
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

void GeometryStore::addCircularArc(const Vec3& start, const Vec3& end, const Vec3& arcPoint) {
    Vec3 center = calculateArcCenter(start, end, arcPoint);

    // Determine direction (clockwise/counter-clockwise)
    Vec3 v1 = {start.x - center.x, start.y - center.y, 0};
    Vec3 v2 = {arcPoint.x - center.x, arcPoint.y - center.y, 0};
    float cross = v1.x * v2.y - v1.y * v2.x;
    bool clockwise = cross < 0;

    std::string centerId = addVertex(center);
    std::string startId = addVertex(start);
    std::string endId = addVertex(end);

    auto shape = std::make_unique<CircularArcShape>();
    shape->type = ShapeType::CircularArc;
    shape->centerVertexId = centerId;
    shape->startVertexId = startId;
    shape->endVertexId = endId;
    shape->clockwise = clockwise;
    shape->id = std::to_string(nextId++);
    shapes.push_back(std::move(shape));
    notifyChange();
}

void GeometryStore::updateArcEndpoint(const std::string& arcId, bool isStart, const Vec3& position) {
    auto* s = getShapeById(arcId);
    if (!s || s->type != ShapeType::CircularArc) return;
    auto& arc = static_cast<CircularArcShape&>(*s);
    std::string vertexId = isStart ? arc.startVertexId : arc.endVertexId;
    std::string otherVertexId = isStart ? arc.endVertexId : arc.startVertexId;

    Vertex* otherV = getVertexById(otherVertexId);
    Vertex* centerV = getVertexById(arc.centerVertexId);
    if (!otherV || !centerV) return;

    float originalRadius = std::sqrt(
        (otherV->position.x - centerV->position.x) * (otherV->position.x - centerV->position.x) +
        (otherV->position.y - centerV->position.y) * (otherV->position.y - centerV->position.y)
    );

    updateVertex(vertexId, position);

    Vec3 chordVec = {otherV->position.x - position.x, otherV->position.y - position.y, 0};
    float chordLength = chordVec.length();

    if (chordLength > 0.001f) {
        Vec3 midpoint = {
            (position.x + otherV->position.x) / 2,
            (position.y + otherV->position.y) / 2,
            (position.z + otherV->position.z) / 2
        };
        Vec3 perpVec = {-chordVec.x / chordLength, chordVec.y / chordLength, 0};
        float halfChord = chordLength / 2;
        float centerDistance = 0;
        if (originalRadius > halfChord)
            centerDistance = std::sqrt(originalRadius * originalRadius - halfChord * halfChord);
        else
            centerDistance = halfChord * 0.1f;

        Vec3 midToCenter = {centerV->position.x - midpoint.x, centerV->position.y - midpoint.y, 0};
        float dotProduct = midToCenter.dot(perpVec);
        float side = dotProduct >= 0 ? 1.0f : -1.0f;

        Vec3 newCenter = {
            midpoint.x + perpVec.x * centerDistance * side,
            midpoint.y + perpVec.y * centerDistance * side,
            midpoint.z
        };
        updateVertex(arc.centerVertexId, newCenter);
    }
}

void GeometryStore::slideArcEndpointOnCircle(const std::string& arcId, bool isStart, const Vec3& position) {
    auto* s = getShapeById(arcId);
    if (!s || s->type != ShapeType::CircularArc) return;
    auto& arc = static_cast<CircularArcShape&>(*s);
    std::string vertexId = isStart ? arc.startVertexId : arc.endVertexId;
    std::string otherVertexId = isStart ? arc.endVertexId : arc.startVertexId;

    Vertex* centerV = getVertexById(arc.centerVertexId);
    Vertex* otherV = getVertexById(otherVertexId);
    if (!centerV || !otherV) return;

    float radius = std::sqrt(
        (otherV->position.x - centerV->position.x) * (otherV->position.x - centerV->position.x) +
        (otherV->position.y - centerV->position.y) * (otherV->position.y - centerV->position.y)
    );

    Vec3 dir = {
        position.x - centerV->position.x,
        position.y - centerV->position.y,
        position.z - centerV->position.z
    };
    float len = dir.length();
    if (len > 0.001f) {
        Vec3 newPos = {
            centerV->position.x + (dir.x / len) * radius,
            centerV->position.y + (dir.y / len) * radius,
            centerV->position.z + (dir.z / len) * radius
        };
        updateVertex(vertexId, newPos);
    }
}

void GeometryStore::updateArcRadius(const std::string& arcId, float scale) {
    auto* s = getShapeById(arcId);
    if (!s || s->type != ShapeType::CircularArc) return;
    auto& arc = static_cast<CircularArcShape&>(*s);
    Vertex* centerV = getVertexById(arc.centerVertexId);
    Vertex* startV = getVertexById(arc.startVertexId);
    Vertex* endV = getVertexById(arc.endVertexId);
    if (!centerV || !startV || !endV) return;

    Vec3 newStart = {
        centerV->position.x + (startV->position.x - centerV->position.x) * scale,
        centerV->position.y + (startV->position.y - centerV->position.y) * scale,
        centerV->position.z + (startV->position.z - centerV->position.z) * scale
    };
    Vec3 newEnd = {
        centerV->position.x + (endV->position.x - centerV->position.x) * scale,
        centerV->position.y + (endV->position.y - centerV->position.y) * scale,
        centerV->position.z + (endV->position.z - centerV->position.z) * scale
    };
    updateVertex(arc.startVertexId, newStart);
    updateVertex(arc.endVertexId, newEnd);
}
