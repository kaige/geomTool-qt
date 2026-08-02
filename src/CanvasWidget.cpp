#include "CanvasWidget.h"
#include "GeometryStore.h"
#include "tools/ToolManager.h"
#include "SnapManager.h"
#include <QPainter>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <cmath>
#include <algorithm>

namespace {
// 2D point-in-triangle test, winding-independent (a triangle is "filled"
// regardless of vertex order). Used for face-based hit testing.
bool pointInTriangle(float px, float py,
                     float ax, float ay, float bx, float by, float cx, float cy) {
    auto s = [](float ppx, float ppy, float qax, float qay, float qbx, float qby) {
        return (qax - ppx) * (qby - ppy) - (qbx - ppx) * (qay - ppy);
    };
    float d1 = s(px, py, ax, ay, bx, by);
    float d2 = s(px, py, bx, by, cx, cy);
    float d3 = s(px, py, cx, cy, ax, ay);
    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);
}
} // namespace

CanvasWidget* g_canvas = nullptr;

CanvasWidget::CanvasWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(400, 300);

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

// ============================================================
// OpenGL setup
// ============================================================

void CanvasWidget::initializeGL() {
    initializeOpenGLFunctions();

    // We rely on draw order (painter's algorithm), not depth.
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!renderer.initialize()) {
        // Shader compile/link failure is reported through Qt's logging
        // category; the canvas will render blank until resolved.
    }
}

void CanvasWidget::resizeGL(int w, int h) {
    camera.aspect = (float)w / (float)std::max(1, h);
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

            // Face hit-test: a click anywhere on a face (not just near an
            // edge) also selects the shape. Faces are not rendered, so the
            // wireframe and hidden dashed edges stay fully visible.
            auto faces = GeometryFactory::create3DFaces(shape->type);
            for (size_t j = 0; j + 2 < faces.size(); j += 3) {
                Vec3 w0 = modelMat.transformPoint(faces[j]);
                Vec3 w1 = modelMat.transformPoint(faces[j+1]);
                Vec3 w2 = modelMat.transformPoint(faces[j+2]);
                float ax, ay, bx, by, cx, cy;
                if (!camera.project(w0, width(), height(), ax, ay)) continue;
                if (!camera.project(w1, width(), height(), bx, by)) continue;
                if (!camera.project(w2, width(), height(), cx, cy)) continue;
                if (pointInTriangle(sx, sy, ax, ay, bx, by, cx, cy))
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
    // event->modifiers() omits Alt on Windows (WM_SYS* mouse messages), so
    // merge with the live keyboard state to reliably detect Shift/Ctrl/Alt.
    Qt::KeyboardModifiers mods = event->modifiers() | QGuiApplication::queryKeyboardModifiers();
    toolManager->handleMouseDown(event->position(), mods);
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    Qt::KeyboardModifiers mods = event->modifiers() | QGuiApplication::queryKeyboardModifiers();
    toolManager->handleMouseMove(event->position(), mods);
    mousePos = event->position();
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    mouseDown = false;
    Qt::KeyboardModifiers mods = event->modifiers() | QGuiApplication::queryKeyboardModifiers();
    toolManager->handleMouseUp(event->position(), mods);
}

void CanvasWidget::wheelEvent(QWheelEvent* event) {
    int delta = event->angleDelta().y();
    camera.zoom(delta > 0 ? -1 : 1);
    update();
}

void CanvasWidget::keyPressEvent(QKeyEvent* event) {
    // Delete the selected shape. Handled here (not in a tool) so it works
    // regardless of which tool is currently active.
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (!g_store.selectedShapeId.empty()) {
            g_store.removeShape(g_store.selectedShapeId);
            return;
        }
    }
    toolManager->handleKeyDown(event);
}

// ============================================================
// Rendering
//
// paintGL draws two layers:
//   1. GL layer    : grid + shapes + tool preview lines (OpenGL)
//   2. Overlay     : selection handles + snap marker + axis gizmo (QPainter)
// QPainter::beginNativePainting() / endNativePainting() bracket the raw
// GL calls so both engines share the framebuffer safely.
// ============================================================

void CanvasWidget::paintGL() {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // --- GL layer -------------------------------------------------
    painter.beginNativePainting();

    // Viewport must cover the full physical framebuffer; on high-DPI
    // displays width()/height() are logical and the FBO is physical.
    const qreal dpr = devicePixelRatioF();
    glViewport(0, 0, int(width() * dpr), int(height() * dpr));

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    camera.aspect = (float)width() / std::max(1, height());

    renderer.beginFrame(width(), height());
    drawGrid();
    drawShapes();
    drawTempLines();
    renderer.endFrame();

    painter.endNativePainting();

    // --- QPainter overlay ----------------------------------------
    if (!g_store.selectedShapeId.empty()) {
        BaseShape* sel = g_store.getSelectedShape();
        if (sel) drawEndpoints(painter, sel);
    }

    drawSnapMarker(painter);
    drawAxes(painter);
}

void CanvasWidget::drawGrid() {
    // Subtle grid on the Z=0 plane — square extent in both directions
    float gridExtent = 20.0f;
    int numLines = 20;

    QColor gridColor(240, 240, 240);
    QColor axisColor(220, 220, 220);

    for (int i = -numLines; i <= numLines; i++) {
        float x = (float)i;
        QPointF s1 = worldToScreen({x, -gridExtent, 0});
        QPointF s2 = worldToScreen({x, gridExtent, 0});
        renderer.addLine(s1, s2, gridColor, 0.5f, false);

        QPointF s3 = worldToScreen({-gridExtent, x, 0});
        QPointF s4 = worldToScreen({gridExtent, x, 0});
        renderer.addLine(s3, s4, gridColor, 0.5f, false);
    }

    // Axes lines on Z=0 plane
    QPointF origin = worldToScreen({0, 0, 0});
    QPointF xAxis  = worldToScreen({gridExtent, 0, 0});
    QPointF yAxis  = worldToScreen({0, gridExtent, 0});
    renderer.addLine(origin, xAxis, axisColor, 1.0f, false);
    renderer.addLine(origin, yAxis, axisColor, 1.0f, false);
}

void CanvasWidget::drawShapes() {
    for (auto& shape : g_store.shapes) {
        if (!shape->visible) continue;
        drawShape(shape.get());
    }
}

void CanvasWidget::drawShape(BaseShape* shape) {
    bool isSelected = (shape->id == g_store.selectedShapeId);
    float r, g, b;
    parseColor(shape->color, r, g, b);
    QColor shapeColor((int)(r*255), (int)(g*255), (int)(b*255));
    QColor drawColor = isSelected ? QColor(255, 107, 53) : shapeColor;

    // QPainter line width 2 -> half width 1px; unselected 3D wireframe 1.5 -> 0.75px
    const float hwLine = 1.0f;
    const float hwWire = isSelected ? 1.0f : 0.75f;

    Mat4 modelMat = Mat4::modelMatrix(shape->position, shape->rotation, shape->scale);

    if (shape->type == ShapeType::LineSegment) {
        auto* ls = static_cast<LineSegmentShape*>(shape);
        auto pts = GeometryFactory::createLineSegmentPoints(ls);
        if (pts.size() < 2) return;
        renderer.addLine(worldToScreen(pts[0]), worldToScreen(pts[1]), drawColor, hwLine, false);

    } else if (shape->type == ShapeType::CircularArc) {
        auto* arc = static_cast<CircularArcShape*>(shape);
        auto pts = GeometryFactory::createArcPoints(arc);
        for (size_t i = 0; i + 1 < pts.size(); i++) {
            renderer.addLine(worldToScreen(pts[i]), worldToScreen(pts[i+1]), drawColor, hwLine, false);
        }

    } else if (shape->type == ShapeType::Circle) {
        auto* circ = static_cast<CircleShape*>(shape);
        Vertex* cv = g_store.getVertexById(circ->centerVertexId);
        if (!cv) return;
        int segs = 64;
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
            renderer.addLine(prev, cur, drawColor, hwLine, false);
            prev = cur;
        }

    } else if (shape->type == ShapeType::Rectangle || shape->type == ShapeType::Triangle || shape->type == ShapeType::Polygon) {
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
            for (size_t i = 0; i < screenPts.size(); i++) {
                renderer.addLine(screenPts[i], screenPts[(i+1) % screenPts.size()], drawColor, hwLine, false);
            }
        }

    } else {
        // 3D shape - wireframe with hidden-line styling.
        // For the cube (flat faces) we use proper back-face culling: an edge
        // is hidden iff both faces meeting at it are back-facing, which yields
        // exactly the 3 hidden edges of an axonometric cube. For the centered
        // convex quadrics (sphere/cylinder/cone/torus) the edge midpoint being
        // behind the object center is the correct hidden test.
        auto edges = GeometryFactory::create3DEdges(shape->type);
        const Vec3 center = shape->position;
        const Vec3 forward = camera.getForward();
        const QColor hiddenColor((drawColor.red()   + 180) / 2,
                                 (drawColor.green() + 180) / 2,
                                 (drawColor.blue()  + 180) / 2);

        // Per-edge pair of adjacent outward face normals (local space) for the
        // cube, in createBoxEdges() order.
        static const Vec3 cubeFaceN[12][2] = {
            {{0,-1,0},{0,0,-1}}, {{0,0,-1},{1,0,0}}, {{0,0,-1},{0,1,0}}, {{0,0,-1},{-1,0,0}},
            {{0,0,1},{0,-1,0}},  {{0,0,1},{1,0,0}},  {{0,0,1},{0,1,0}},  {{0,0,1},{-1,0,0}},
            {{-1,0,0},{0,-1,0}}, {{1,0,0},{0,-1,0}}, {{1,0,0},{0,1,0}},  {{-1,0,0},{0,1,0}},
        };

        // --- Cylinder rendering -------------------------------------
        // Two different rules by region:
        //  * Side (vertical): silhouette rule — only the two outline lines
        //    where side visibility flips are drawn (solid).
        //  * Top/bottom rims: cube-style — every segment is drawn; dashed
        //    only when BOTH adjacent faces (cap + side quad) are back-facing,
        //    solid as soon as either face is visible.
        if (shape->type == ShapeType::Cylinder) {
            constexpr int seg = 16;
            const float rTop = 1.0f, rBot = 1.0f, h = 2.0f;
            const float halfH = h * 0.5f;
            const Vec3 fwd = forward;

            // Cylinder local axes in world space (rotation only; uniform
            // scale leaves directions valid).
            const float A = modelMat.transformDir({1, 0, 0}).dot(fwd); // local X
            const float B = modelMat.transformDir({0, 0, 1}).dot(fwd); // local Z
            const float C = modelMat.transformDir({0, 1, 0}).dot(fwd); // local Y (axis)

            // Side face with outward normal n(a) = cos(a)*X + sin(a)*Z is
            // front-facing (visible) iff n·fwd < 0.
            auto sideVisible = [&](float a) {
                return std::cos(a) * A + std::sin(a) * B < 0.0f;
            };
            const bool topVis = (C < 0.0f);  // top cap, outward normal +Y
            const bool botVis = (C > 0.0f);  // bottom cap, outward normal -Y

            auto localPt = [&](float a, float y, float r) {
                return modelMat.transformPoint({r * std::cos(a), y, r * std::sin(a)});
            };

            // Top & bottom rim segments: cube-style. A segment is hidden
            // (dashed) iff both its adjacent faces — the cap and the side
            // quad — are back-facing; otherwise solid. The dash phase is
            // continuous around each rim (as on the sphere equator) so a
            // small cylinder's rim does not blur into a solid ring.
            QPointF prevTop = worldToScreen(localPt(0.0f,  halfH, rTop));
            QPointF prevBot = worldToScreen(localPt(0.0f, -halfH, rBot));
            float dashOffTop = 0.0f, dashOffBot = 0.0f;
            for (int i = 0; i < seg; ++i) {
                float a2 = 2.0f * M_PI * (i + 1) / seg;
                bool sv = sideVisible(2.0f * M_PI * (i + 0.5f) / seg);

                QPointF curTop = worldToScreen(localPt(a2,  halfH, rTop));
                bool topHidden = !topVis && !sv;
                renderer.addLine(prevTop, curTop,
                                 topHidden ? hiddenColor : drawColor, hwWire,
                                 topHidden, dashOffTop);
                float dtx = curTop.x() - prevTop.x(), dty = curTop.y() - prevTop.y();
                dashOffTop += std::sqrt(dtx * dtx + dty * dty);
                prevTop = curTop;

                QPointF curBot = worldToScreen(localPt(a2, -halfH, rBot));
                bool botHidden = !botVis && !sv;
                renderer.addLine(prevBot, curBot,
                                 botHidden ? hiddenColor : drawColor, hwWire,
                                 botHidden, dashOffBot);
                float dbx = curBot.x() - prevBot.x(), dby = curBot.y() - prevBot.y();
                dashOffBot += std::sqrt(dbx * dbx + dby * dby);
                prevBot = curBot;
            }

            // Two vertical silhouette edges where side visibility flips:
            //   cos(a)*A + sin(a)*B = 0  =>  a = atan2(B, A) ± π/2.
            // Skipped when looking straight down the axis (no side silhouette).
            if (A * A + B * B > 1e-6f) {
                float phi = std::atan2(B, A);
                float sil[2] = { phi + float(M_PI_2), phi - float(M_PI_2) };
                for (float a : sil) {
                    Vec3 p1 = localPt(a, -halfH, rBot), p2 = localPt(a, halfH, rTop);
                    renderer.addLine(worldToScreen(p1), worldToScreen(p2), drawColor, hwWire, false);
                }
            }
            return;
        }

        // --- Sphere rendering ---------------------------------------
        // Two elements, mirroring the cylinder's two rules:
        //  * Silhouette (outline): the great circle in the plane through
        //    the center perpendicular to the view direction — the sphere's
        //    apparent boundary. This is the sphere analogue of the
        //    cylinder's side silhouette: the locus where the surface
        //    normal is perpendicular to the view, i.e. where adjacent
        //    surface patches flip front/back visibility (the "edge whose
        //    adjacent faces disagree" rule). It is entirely front-facing,
        //    so every segment is drawn solid.
        //  * Equator: the great circle in the local XZ plane (local y=0),
        //    cube/cylinder-rim style — a segment is dashed when it lies on
        //    the back hemisphere (outward normal points away from the
        //    camera) and solid on the front hemisphere.
        if (shape->type == ShapeType::Sphere) {
            constexpr int seg = 64;
            const Vec3 fwd = forward;
            // World radius: uniform scale applied to the base unit sphere.
            const float worldR = modelMat.transformDir({1.0f, 0.0f, 0.0f}).length();

            // Silhouette circle: right/up span the plane normal to `fwd`.
            const Vec3 right = camera.getRight();
            const Vec3 up = camera.getUp();
            QPointF silPrev = worldToScreen(center + right * worldR);
            for (int i = 1; i <= seg; ++i) {
                float a = 2.0f * M_PI * i / seg;
                QPointF silCur = worldToScreen(center
                    + right * (std::cos(a) * worldR)
                    + up    * (std::sin(a) * worldR));
                renderer.addLine(silPrev, silCur, drawColor, hwWire, false);
                silPrev = silCur;
            }

            // Equator: local XZ-plane great circle (transformed by modelMat
            // so rotation/scale apply). Outward normal = radial direction.
            auto equPt = [&](float a) {
                return modelMat.transformPoint({std::cos(a), 0.0f, std::sin(a)});
            };
            // Advance the dash phase along the screen-space ring (cumulative
            // screen length) so the hidden back arc reads as one continuous
            // dashed curve — matching the cube's single-segment hidden edges
            // — instead of restarting the dash at every segment, which blurs
            // a small sphere's equator into a solid line.
            QPointF prevRing = worldToScreen(equPt(0.0f));
            float dashOff = 0.0f;
            for (int i = 0; i < seg; ++i) {
                float a1 = 2.0f * M_PI * i / seg;
                float a2 = 2.0f * M_PI * (i + 1) / seg;
                QPointF curRing = worldToScreen(equPt(a2));
                Vec3 n = (equPt((a1 + a2) * 0.5f) - center).normalized();
                bool hidden = n.dot(fwd) > 0.0f;  // back hemisphere
                renderer.addLine(prevRing, curRing,
                                 hidden ? hiddenColor : drawColor, hwWire,
                                 hidden, dashOff);
                float dx = curRing.x() - prevRing.x();
                float dy = curRing.y() - prevRing.y();
                dashOff += std::sqrt(dx * dx + dy * dy);
                prevRing = curRing;
            }
            return;
        }

        // --- Cone rendering -----------------------------------------
        // Mirrors the cylinder/sphere rules:
        //  * Side silhouette: the two generators (apex -> base) where the
        //    side surface flips front/back visibility — drawn solid. This
        //    is the cone analogue of the cylinder's side silhouette lines
        //    and the sphere's outline circle.
        //  * Base rim: cube/cylinder-rim style — a segment is dashed when
        //    BOTH adjacent faces (base cap + side quad) are back-facing,
        //    solid as soon as either is visible. Dash phase is continuous
        //    around the rim (as on the sphere equator) so a small cone's
        //    base ring does not blur into a solid line.
        if (shape->type == ShapeType::Cone) {
            constexpr int seg = 48;
            const float r = 1.0f, h = 2.0f;
            const float halfH = h * 0.5f;
            const Vec3 fwd = forward;

            // Cone local axes projected onto the view direction (rotation
            // only; uniform scale leaves directions valid).
            const float A = modelMat.transformDir({1, 0, 0}).dot(fwd); // local X
            const float B = modelMat.transformDir({0, 0, 1}).dot(fwd); // local Z
            const float C = modelMat.transformDir({0, 1, 0}).dot(fwd); // local Y (axis)

            // The side outward normal at generator angle a is proportional
            // to (h cos a, r, h sin a); its dot with fwd is
            // h (cos a A + sin a B) + r C. Front-facing (visible) when < 0.
            auto sideVisible = [&](float a) {
                return h * (std::cos(a) * A + std::sin(a) * B) + r * C < 0.0f;
            };
            const bool baseVis = (C > 0.0f);  // base cap, outward normal -Y

            auto localPt = [&](float a, float y, float rad) {
                return modelMat.transformPoint({rad * std::cos(a), y, rad * std::sin(a)});
            };

            // Base rim: cylinder-rim rule with a continuous dash phase.
            QPointF prevRing = worldToScreen(localPt(0.0f, -halfH, r));
            float dashOff = 0.0f;
            for (int i = 0; i < seg; ++i) {
                float a2 = 2.0f * M_PI * (i + 1) / seg;
                QPointF curRing = worldToScreen(localPt(a2, -halfH, r));
                bool sv = sideVisible(2.0f * M_PI * (i + 0.5f) / seg);
                bool hidden = !baseVis && !sv;
                renderer.addLine(prevRing, curRing,
                                 hidden ? hiddenColor : drawColor, hwWire,
                                 hidden, dashOff);
                float dx = curRing.x() - prevRing.x();
                float dy = curRing.y() - prevRing.y();
                dashOff += std::sqrt(dx * dx + dy * dy);
                prevRing = curRing;
            }

            // Two side silhouette generators where the side flips visibility:
            //   h (cos a A + sin a B) + r C = 0  =>  cos a A + sin a B = -r C / h.
            // With R = sqrt(A²+B²), phi = atan2(B,A): a = phi ± acos((-rC/h)/R).
            // No generator when looking along the axis (R≈0) or when the side
            // is uniformly front/back (|ratio|>1); the base ring is then the
            // silhouette, which the rim loop already draws.
            const float R = std::sqrt(A * A + B * B);
            if (R > 1e-6f) {
                float ratio = (-r * C / h) / R;
                if (std::fabs(ratio) <= 1.0f) {
                    float phi = std::atan2(B, A);
                    float dlt = std::acos(ratio);
                    float sil[2] = { phi + dlt, phi - dlt };
                    Vec3 apex = localPt(0.0f, halfH, 0.0f);
                    for (float a : sil) {
                        Vec3 base = localPt(a, -halfH, r);
                        renderer.addLine(worldToScreen(apex), worldToScreen(base),
                                         drawColor, hwWire, false);
                    }
                }
            }
            return;
        }

        for (size_t j = 0; j < edges.size(); j += 2) {
            Vec3 p1 = modelMat.transformPoint(edges[j]);
            Vec3 p2 = modelMat.transformPoint(edges[j+1]);

            bool hidden;
            if (shape->type == ShapeType::Cube) {
                size_t e = j / 2;
                // Transform face normals into world space (rotation; uniform scale keeps direction).
                Vec3 n0 = modelMat.transformDir(cubeFaceN[e][0]);
                Vec3 n1 = modelMat.transformDir(cubeFaceN[e][1]);
                // Back-facing: outward normal points along the view direction.
                hidden = (n0.dot(forward) > 0.0f) && (n1.dot(forward) > 0.0f);
            } else {
                Vec3 mid = (p1 + p2) * 0.5f;
                hidden = (mid - center).dot(forward) > 0.0f;
            }

            renderer.addLine(worldToScreen(p1), worldToScreen(p2),
                             hidden ? hiddenColor : drawColor, hwWire, hidden);
        }
    }
}

void CanvasWidget::drawTempLines() {
    for (auto& tl : tempLines) {
        if (tl.points.size() < 2) continue;
        for (size_t i = 0; i + 1 < tl.points.size(); i++) {
            renderer.addLine(worldToScreen(tl.points[i]), worldToScreen(tl.points[i+1]),
                             tl.color, 1.0f, tl.dashed);
        }
    }
}

// ============================================================
// QPainter overlays
// ============================================================

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

void CanvasWidget::drawAxes(QPainter& painter) {
    // Small axes indicator in bottom-left corner
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
    Vec3 forward = camera.getForward();
    Vec3 right = camera.getRight();
    Vec3 up = camera.getUp();

    auto projectAxis = [&](const Vec3& axisDir, const QColor& color, const QString& label) {
        float xProj = axisDir.dot(right);
        float yProj = axisDir.dot(up);
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
