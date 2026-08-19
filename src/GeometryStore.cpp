#include "GeometryStore.h"

GeometryStore g_store;

// ============================================================
// Arc calculations (ported from GeometryStore.ts)
// ============================================================

static Vec3 calculateArcCenter(const Vec3& start, const Vec3& end, const Vec3& arc) {
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

    // Fallback: circumcircle formula
    float r1 = std::sqrt((ax-centerX)*(ax-centerX) + (az-centerZ)*(az-centerZ));
    float r2 = std::sqrt((bx-centerX)*(bx-centerX) + (bz-centerZ)*(bz-centerZ));
    float r3 = std::sqrt((cx-centerX)*(cx-centerX) + (cz-centerZ)*(cz-centerZ));

    if (std::max({r1, r2, r3}) - std::min({r1, r2, r3}) > std::max({r1, r2, r3}) * 0.5f) {
        float d = 2 * (ax * (bz - cz) + bx * (cz - az) + cx * (az - bz));
        if (std::abs(d) > 1e-10f) {
            centerX = ((ax*ax+az*az)*(bz-cz) + (bx*bx+bz*bz)*(cz-az) + (cx*cx+cz*cz)*(az-bz)) / d;
            centerZ = ((ax*ax+az*az)*(cx-bx) + (bx*bx+bz*bz)*(ax-cx) + (cx*cx+cz*cz)*(bx-ax)) / d;
        }
    }

    return {centerX, 0, centerZ};
}

void GeometryStore::addCircularArc(const Vec3& start, const Vec3& end, const Vec3& arcPoint) {
    Vec3 center = calculateArcCenter(start, end, arcPoint);

    // Determine direction (clockwise/counter-clockwise) in the XZ plane
    Vec3 v1 = {start.x - center.x, 0, start.z - center.z};
    Vec3 v2 = {arcPoint.x - center.x, 0, arcPoint.z - center.z};
    float cross = v1.x * v2.z - v1.z * v2.x;
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
        (otherV->position.z - centerV->position.z) * (otherV->position.z - centerV->position.z)
    );

    updateVertex(vertexId, position);

    Vec3 chordVec = {otherV->position.x - position.x, 0, otherV->position.z - position.z};
    float chordLength = chordVec.length();

    if (chordLength > 0.001f) {
        Vec3 midpoint = {
            (position.x + otherV->position.x) / 2,
            0,
            (position.z + otherV->position.z) / 2
        };
        Vec3 perpVec = {-chordVec.x / chordLength, 0, chordVec.z / chordLength};
        float halfChord = chordLength / 2;
        float centerDistance = 0;
        if (originalRadius > halfChord)
            centerDistance = std::sqrt(originalRadius * originalRadius - halfChord * halfChord);
        else
            centerDistance = halfChord * 0.1f;

        Vec3 midToCenter = {centerV->position.x - midpoint.x, 0, centerV->position.z - midpoint.z};
        float dotProduct = midToCenter.dot(perpVec);
        float side = dotProduct >= 0 ? 1.0f : -1.0f;

        Vec3 newCenter = {
            midpoint.x + perpVec.x * centerDistance * side,
            0,
            midpoint.z + perpVec.z * centerDistance * side
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
        (otherV->position.z - centerV->position.z) * (otherV->position.z - centerV->position.z)
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
