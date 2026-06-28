#include "CanvasWidget.h"
#include "GeometryStore.h"
#include "tools/ToolManager.h"
#include "SnapManager.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <cmath>
#include <algorithm>

CanvasWidget* g_canvas = nullptr;

CanvasWidget::CanvasWidget(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(400, 300);
    setAttribute(Qt::WA_OpaquePaintEvent);

    snapManager = std::make_unique<SnapManager>();
    toolManager = std::make_unique<ToolManager>(this);
    g_canvas = this;

    g_store.onChange = [this]() { update(); };
}

CanvasWidget::~CanvasWidget() {
    g_canvas = nullptr;
}

void CanvasWidget::refresh() { update(); }

void CanvasWidget::setCanvasCursor(Qt::CursorShape shape) {
    setCursor(shape);
}

void CanvasWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    camera.aspect = (float)width() / (float)height();
}

QPointF CanvasWidget::worldToScreen(const Vec3& world) {
    float sx, sy;
    camera.project(world, width(), height(), sx, sy);
    return QPointF(sx, sy);
}

Vec3 CanvasWidget::screenToWorld(float sx, float sy) {
    return camera.screenToWorld(sx, sy, width(), height());
}

Vec3 CanvasWidget::screenToWorldOnZ0Plane(float sx, float sy) {
    return camera.screenToWorldOnPlane(sx, sy, width(), height(), {0, 0, 1}, {0, 0, 0});
}

Vec3 CanvasWidget::screenToWorldOnPlane(float sx, float sy, const Vec3& normal, const Vec3& point) {
    return camera.screenToWorldOnPlane(sx, sy, width(), height(), normal, point);
}

// ============================================================
// Hit Testing
// ============================================================

std::string CanvasWidget::hitTestShape(float sx, float sy) {
    float threshold = 8.0f; // pixel threshold

    // Check shapes in reverse order (top to bottom)
    for (int i = (int)g_store.shapes.size() - 1; i >= 0; i--) {
        auto& shape = g_store.shapes[i];
        if (!shape->visible) continue;

        // Get world-space points for this shape
        std::vector<Vec3> worldPoints;
        std::vector<Vec3> edgePoints;

        Mat4 modelMat = Mat4::modelMatrix(shape->position, shape->rotation, shape->scale);

        if (shape->type == ShapeType::LineSegment) {
            auto* ls = static_cast<LineSegmentShape*>(shape.get());
            Vertex* sv = g_store.getVertexById(ls->startVertexId);
            Vertex* ev = g_store.getVertexById(ls->endVertexId);
            if (sv && ev) {
                worldPoints.push_back(sv->position);
                worldPoints.push_back(ev->position);
            }
        } else if (shape->type == ShapeType::CircularArc) {
            auto* arc = static_cast<CircularArcShape*>(shape.get());
            worldPoints = GeometryFactory::createArcPoints(arc);
        } else {
            // 3D shape - get edge points and transform
            auto edges = GeometryFactory::create3DEdges(shape->type);
            for (auto& e : edges)
                edgePoints.push_back(modelMat.transformPoint(e));

            // For hit testing, check distance to each edge line
            for (size_t j = 0; j < edgePoints.size(); j += 2) {
                float s1x, s1y, s2x, s2y;
                if (!camera.project(edgePoints[j], width(), height(), s1x, s1y)) continue;
                if (!camera.project(edgePoints[j+1], width(), height(), s2x, s2y)) continue;

                // Distance from point to line segment
                float dx = s2x - s1x, dy = s2y - s1y;
                float len2 = dx*dx + dy*dy;
                if (len2 < 0.01f) continue;
                float t = clampf(((sx - s1x) * dx + (sy - s1y) * dy) / len2, 0, 1);
                float px = s1x + t * dx, py = s1y + t * dy;
                float dist = std::sqrt((sx - px)*(sx - px) + (sy - py)*(sy - py));
                if (dist < threshold)
                    return shape->id;
            }
            continue;
        }

        // For line/arc shapes, check distance to polyline
        for (size_t j = 0; j + 1 < worldPoints.size(); j++) {
            float s1x, s1y, s2x, s2y;
            if (!camera.project(worldPoints[j], width(), height(), s1x, s1y)) continue;
            if (!camera.project(worldPoints[j+1], width(), height(), s2x, s2y)) continue;

            float dx = s2x - s1x, dy = s2y - s1y;
            float len2 = dx*dx + dy*dy;
            if (len2 < 0.01f) continue;
            float t = clampf(((sx - s1x) * dx + (sy - s1y) * dy) / len2, 0, 1);
            float px = s1x + t * dx, py = s1y + t * dy;
            float dist = std::sqrt((sx - px)*(sx - px) + (sy - py)*(sy - py));
            if (dist < threshold)
                return shape->id;
        }
    }
    return "";
}

bool CanvasWidget::hitTestLineEndpoint(float sx, float sy, std::string& outLineId, bool& outIsStart) {
    if (g_store.selectedShapeId.empty()) return false;
    BaseShape* sel = g_store.getSelectedShape();
    if (!sel || sel->type != ShapeType::LineSegment) return false;
    auto* ls = static_cast<LineSegmentShape*>(sel);

    Vertex* sv = g_store.getVertexById(ls->startVertexId);
    Vertex* ev = g_store.getVertexById(ls->endVertexId);
    if (!sv || !ev) return false;

    float threshold = 10.0f;
    float s1x, s1y, s2x, s2y;
    if (camera.project(sv->position, width(), height(), s1x, s1y)) {
        if (std::sqrt((sx-s1x)*(sx-s1x) + (sy-s1y)*(sy-s1y)) < threshold) {
            outLineId = ls->id;
            outIsStart = true;
            return true;
        }
    }
    if (camera.project(ev->position, width(), height(), s2x, s2y)) {
        if (std::sqrt((sx-s2x)*(sx-s2x) + (sy-s2y)*(sy-s2y)) < threshold) {
            outLineId = ls->id;
            outIsStart = false;
            return true;
        }
    }
    return false;
}

bool CanvasWidget::hitTestArcPoint(float sx, float sy, std::string& outArcId, int& outPointType) {
    if (g_store.selectedShapeId.empty()) return false;
    BaseShape* sel = g_store.getSelectedShape();
    if (!sel || sel->type != ShapeType::CircularArc) return false;
    auto* arc = static_cast<CircularArcShape*>(sel);

    Vertex* cv = g_store.getVertexById(arc->centerVertexId);
    Vertex* sv = g_store.getVertexById(arc->startVertexId);
    Vertex* ev = g_store.getVertexById(arc->endVertexId);
    if (!cv || !sv || !ev) return false;

    float threshold = 10.0f;
    struct { Vec3 pos; int type; } pts[] = {{cv->position, 0}, {sv->position, 1}, {ev->position, 2}};
    for (auto& p : pts) {
        float px, py;
        if (camera.project(p.pos, width(), height(), px, py)) {
            if (std::sqrt((sx-px)*(sx-px) + (sy-py)*(sy-py)) < threshold) {
                outArcId = arc->id;
                outPointType = p.type;
                return true;
            }
        }
    }
    return false;
}

void CanvasWidget::getSelectedLineEndpoints(Vec3& start, Vec3& end) {
    BaseShape* sel = g_store.getSelectedShape();
    if (!sel || sel->type != ShapeType::LineSegment) return;
    auto* ls = static_cast<LineSegmentShape*>(sel);
    Vertex* sv = g_store.getVertexById(ls->startVertexId);
    Vertex* ev = g_store.getVertexById(ls->endVertexId);
    if (sv) start = sv->position;
    if (ev) end = ev->position;
}

void CanvasWidget::getSelectedArcPoints(Vec3& center, Vec3& start, Vec3& end) {
    BaseShape* sel = g_store.getSelectedShape();
    if (!sel || sel->type != ShapeType::CircularArc) return;
    auto* arc = static_cast<CircularArcShape*>(sel);
    Vertex* cv = g_store.getVertexById(arc->centerVertexId);
    Vertex* sv = g_store.getVertexById(arc->startVertexId);
    Vertex* ev = g_store.getVertexById(arc->endVertexId);
    if (cv) center = cv->position;
    if (sv) start = sv->position;
    if (ev) end = ev->position;
}

// ============================================================
// Event Handlers
// ============================================================

void CanvasWidget::mousePressEvent(QMouseEvent* event) {
    setFocus();
    mouseDown = true;
    mousePos = event->position();
    startMousePos = event->position();
    toolManager->handleMouseDown(event->position(), event->modifiers());
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    toolManager->handleMouseMove(event->position(), event->modifiers());
    mousePos = event->position();
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    mouseDown = false;
    toolManager->handleMouseUp(event->position(), event->modifiers());
}

void CanvasWidget::wheelEvent(QWheelEvent* event) {
    int delta = event->angleDelta().y();
    camera.zoom(delta > 0 ? -1 : 1);
    update();
}

void CanvasWidget::keyPressEvent(QKeyEvent* event) {
    toolManager->handleKeyDown(event);
}

// ============================================================
// Rendering
// ============================================================

void CanvasWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    painter.fillRect(rect(), Qt::white);

    // Update aspect
    camera.aspect = (float)width() / std::max(1, height());

    // Draw grid
    drawGrid(painter);

    // Draw shapes
    drawShapes(painter);

    // Draw endpoints for selected shape
    if (!g_store.selectedShapeId.empty()) {
        BaseShape* sel = g_store.getSelectedShape();
        if (sel) drawEndpoints(painter, sel);
    }

    // Draw temp lines (from tools)
    drawTempLines(painter);

    // Draw snap marker
    drawSnapMarker(painter);

    // Draw axes overlay
    drawAxes(painter);
}

void CanvasWidget::drawGrid(QPainter& painter) {
    // Draw a subtle grid on the Z=0 plane
    float gridSize = 10.0f;
    int numLines = 20;

    painter.setPen(QPen(QColor(240, 240, 240), 1));
    for (int i = -numLines; i <= numLines; i++) {
        float x = (float)i;
        Vec3 p1 = {x, -gridSize, 0};
        Vec3 p2 = {x, gridSize, 0};
        QPointF s1 = worldToScreen(p1);
        QPointF s2 = worldToScreen(p2);
        painter.drawLine(s1, s2);

        Vec3 p3 = {-gridSize, x, 0};
        Vec3 p4 = {gridSize, x, 0};
        QPointF s3 = worldToScreen(p3);
        QPointF s4 = worldToScreen(p4);
        painter.drawLine(s3, s4);
    }

    // Draw axes lines on Z=0 plane
    painter.setPen(QPen(QColor(220, 220, 220), 2));
    QPointF origin = worldToScreen({0, 0, 0});
    QPointF xAxis = worldToScreen({gridSize, 0, 0});
    QPointF yAxis = worldToScreen({0, gridSize, 0});
    painter.drawLine(origin, xAxis);
    painter.drawLine(origin, yAxis);
}

void CanvasWidget::drawShapes(QPainter& painter) {
    for (auto& shape : g_store.shapes) {
        if (!shape->visible) continue;
        drawShape(painter, shape.get());
    }
}

void CanvasWidget::drawShape(QPainter& painter, BaseShape* shape) {
    bool isSelected = (shape->id == g_store.selectedShapeId);
    float r, g, b;
    parseColor(shape->color, r, g, b);
    QColor shapeColor((int)(r*255), (int)(g*255), (int)(b*255));
    QColor drawColor = isSelected ? QColor(255, 107, 53) : shapeColor;

    Mat4 modelMat = Mat4::modelMatrix(shape->position, shape->rotation, shape->scale);

    if (shape->type == ShapeType::LineSegment) {
        auto* ls = static_cast<LineSegmentShape*>(shape);
        auto pts = GeometryFactory::createLineSegmentPoints(ls);
        if (pts.size() < 2) return;
        QPointF s1 = worldToScreen(pts[0]);
        QPointF s2 = worldToScreen(pts[1]);
        painter.setPen(QPen(drawColor, 2));
        painter.drawLine(s1, s2);

    } else if (shape->type == ShapeType::CircularArc) {
        auto* arc = static_cast<CircularArcShape*>(shape);
        auto pts = GeometryFactory::createArcPoints(arc);
        painter.setPen(QPen(drawColor, 2));
        for (size_t i = 0; i + 1 < pts.size(); i++) {
            QPointF s1 = worldToScreen(pts[i]);
            QPointF s2 = worldToScreen(pts[i+1]);
            painter.drawLine(s1, s2);
        }

    } else if (shape->type == ShapeType::Circle) {
        auto* circ = static_cast<CircleShape*>(shape);
        Vertex* cv = g_store.getVertexById(circ->centerVertexId);
        if (!cv) return;
        int segs = 64;
        painter.setPen(QPen(drawColor, 2));
        QPointF prev = worldToScreen({
            cv->position.x + circ->radius, cv->position.y, 0
        });
        for (int i = 1; i <= segs; i++) {
            float a = 2 * M_PI * i / segs;
            Vec3 p = {
                cv->position.x + circ->radius * std::cos(a),
                cv->position.y + circ->radius * std::sin(a),
                0
            };
            QPointF cur = worldToScreen(p);
            painter.drawLine(prev, cur);
            prev = cur;
        }

    } else if (shape->type == ShapeType::Rectangle || shape->type == ShapeType::Triangle || shape->type == ShapeType::Polygon) {
        // All three share vertexIds via PolygonShape layout (RectangleShape/TriangleShape inherit from BaseShape with their own vertexIds)
        // Use reinterpret since they all have vertexIds at compatible layout
        std::vector<std::string> vertexIds;
        if (shape->type == ShapeType::Rectangle)
            vertexIds = static_cast<RectangleShape*>(shape)->vertexIds;
        else if (shape->type == ShapeType::Triangle)
            vertexIds = static_cast<TriangleShape*>(shape)->vertexIds;
        else
            vertexIds = static_cast<PolygonShape*>(shape)->vertexIds;

        std::vector<QPointF> screenPts;
        for (auto& vid : vertexIds) {
            Vertex* v = g_store.getVertexById(vid);
            if (v) screenPts.push_back(worldToScreen(v->position));
        }
        if (screenPts.size() >= 2) {
            painter.setPen(QPen(drawColor, 2));
            for (size_t i = 0; i < screenPts.size(); i++) {
                painter.drawLine(screenPts[i], screenPts[(i+1) % screenPts.size()]);
            }
        }

    } else {
        // 3D shape - draw wireframe edges
        auto edges = GeometryFactory::create3DEdges(shape->type);
        painter.setPen(QPen(drawColor, isSelected ? 2 : 1.5));

        for (size_t j = 0; j < edges.size(); j += 2) {
            Vec3 p1 = modelMat.transformPoint(edges[j]);
            Vec3 p2 = modelMat.transformPoint(edges[j+1]);
            QPointF s1 = worldToScreen(p1);
            QPointF s2 = worldToScreen(p2);
            painter.drawLine(s1, s2);
        }
    }
}

void CanvasWidget::drawEndpoints(QPainter& painter, BaseShape* shape) {
    float markerSize = 8.0f;

    if (shape->type == ShapeType::LineSegment) {
        Vec3 startPos, endPos;
        getSelectedLineEndpoints(startPos, endPos);
        QPointF s1 = worldToScreen(startPos);
        QPointF s2 = worldToScreen(endPos);

        painter.setBrush(QColor(255, 107, 53));
        painter.setPen(QPen(Qt::white, 1));
        painter.drawEllipse(s1, markerSize, markerSize);
        painter.drawEllipse(s2, markerSize, markerSize);

    } else if (shape->type == ShapeType::CircularArc) {
        Vec3 center, start, end;
        getSelectedArcPoints(center, start, end);
        QPointF sc = worldToScreen(center);
        QPointF ss = worldToScreen(start);
        QPointF se = worldToScreen(end);

        // Center - ring (different shape)
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 107, 53), 2));
        painter.drawEllipse(sc, markerSize + 2, markerSize + 2);

        // Start/End - filled circles
        painter.setBrush(QColor(255, 107, 53));
        painter.setPen(QPen(Qt::white, 1));
        painter.drawEllipse(ss, markerSize, markerSize);
        painter.drawEllipse(se, markerSize, markerSize);
    }
}

void CanvasWidget::drawSnapMarker(QPainter& painter) {
    if (!snapVisible) return;
    QPointF pos = worldToScreen(snapPosition);

    painter.setPen(QPen(QColor(0, 200, 0), 2));
    float s = 6.0f;
    painter.drawLine(pos.x() - s, pos.y(), pos.x() + s, pos.y());
    painter.drawLine(pos.x(), pos.y() - s, pos.x(), pos.y() + s);

    if (snapType == 1) {
        // Center - circle
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(pos, s + 2, s + 2);
    } else if (snapType == 0) {
        // Endpoint - square
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(pos.x() - s, pos.y() - s, s*2, s*2);
    } else if (snapType == 2) {
        // Midpoint - diamond
        painter.setBrush(Qt::NoBrush);
        QPolygonF diamond;
        diamond << QPointF(pos.x(), pos.y() - s)
                << QPointF(pos.x() + s, pos.y())
                << QPointF(pos.x(), pos.y() + s)
                << QPointF(pos.x() - s, pos.y());
        painter.drawPolygon(diamond);
    }
}

void CanvasWidget::drawTempLines(QPainter& painter) {
    for (auto& tl : tempLines) {
        if (tl.points.size() < 2) continue;
        if (tl.dashed) {
            painter.setPen(QPen(tl.color, 2, Qt::DashLine));
        } else {
            painter.setPen(QPen(tl.color, 2));
        }
        for (size_t i = 0; i + 1 < tl.points.size(); i++) {
            QPointF s1 = worldToScreen(tl.points[i]);
            QPointF s2 = worldToScreen(tl.points[i+1]);
            painter.drawLine(s1, s2);
        }
    }
}

void CanvasWidget::drawAxes(QPainter& painter) {
    // Draw small axes indicator in bottom-left corner
    int size = 80;
    int margin = 10;
    int cx = margin + size / 2;
    int cy = height() - margin - size / 2;
    float axisLen = size / 2 - 5;

    // Background circle
    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.setBrush(QColor(250, 250, 250, 200));
    painter.drawEllipse(QPointF(cx, cy), size/2.0, size/2.0);

    // Get camera direction to project axes
    Vec3 camPos = camera.getPosition();
    Vec3 forward = camera.getForward();
    Vec3 right = camera.getRight();
    Vec3 up = camera.getUp();

    // Project axis directions
    auto projectAxis = [&](const Vec3& axisDir, const QColor& color, const QString& label) {
        float xProj = axisDir.dot(right);
        float yProj = axisDir.dot(up);
        // Depth for sorting (not used for visual but conceptually)
        float depth = axisDir.dot(forward);

        float px = cx + xProj * axisLen;
        float py = cy - yProj * axisLen;

        painter.setPen(QPen(color, 2));
        painter.drawLine(cx, cy, (int)px, (int)py);
        painter.setPen(color);
        painter.drawText((int)px + 2, (int)py - 2, label);
    };

    projectAxis({1, 0, 0}, QColor(255, 0, 0), "X");
    projectAxis({0, 1, 0}, QColor(0, 200, 0), "Y");
    projectAxis({0, 0, 1}, QColor(0, 0, 255), "Z");
}
