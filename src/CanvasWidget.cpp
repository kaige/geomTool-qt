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

bool CanvasWidget::projectLine(const Vec3& a, const Vec3& b, QPointF& sa, QPointF& sb) {
    // Near-plane line clipping: when the camera is tilted, grid or shape
    // endpoints behind the camera produce garbage projected coordinates,
    // causing lines to "radiate" from a vanishing point. We clip the segment
    // against the camera's near plane (depth >= small epsilon) so only the
    // visible portion is drawn.

    Vec3 camPos = camera.getPosition();
    Vec3 fwd = camera.getForward();

    float dA = (a - camPos).dot(fwd);
    float dB = (b - camPos).dot(fwd);
    const float nearEps = 0.1f;

    // Both behind — invisible
    if (dA < nearEps && dB < nearEps) return false;

    Vec3 ca = a, cb = b;

    // Clip endpoint a if behind
    if (dA < nearEps) {
        float t = (nearEps - dA) / (dB - dA);
        ca = a + (b - a) * t;
    }
    // Clip endpoint b if behind
    if (dB < nearEps) {
        float t = (nearEps - dB) / (dA - dB);
        cb = b + (a - b) * t;
    }

    float sax, say, sbx, sby;
    if (!camera.project(ca, width(), height(), sax, say)) return false;
    if (!camera.project(cb, width(), height(), sbx, sby)) return false;

    sa = QPointF(sax, say);
    sb = QPointF(sbx, sby);
    return true;
}

Vec3 CanvasWidget::screenToWorld(float sx, float sy) {
    return camera.screenToWorld(sx, sy, width(), height());
}

Vec3 CanvasWidget::screenToWorldOnWorkPlane(float sx, float sy) {
    // Work plane = the y=0 XZ plane (the frontal "page" the default
    // camera faces). Normal +Y points into the scene.
    return camera.screenToWorldOnPlane(sx, sy, width(), height(), {0, 1, 0}, {0, 0, 0});
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

    // During drag operations, repaint the canvas for smooth visual feedback.
    // Drag tools update vertex/shape data directly (without notifyChange),
    // so we must trigger a lightweight canvas-only repaint here — NOT a full
    // UI refresh (which would rebuild the shape list sidebar on every move
    // and cause severe lag). The full notifyChange() fires once when the drag
    // ends and the tool switches back to Select.
    if (isDraggingObject || isDraggingEndpoint || isRotatingObject) {
        update();
    }
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
    // Subtle grid on the Y=0 XZ work plane — square extent in both directions
    float gridExtent = 20.0f;
    int numLines = 20;

    QColor gridColor(240, 240, 240);
    QColor axisColor(220, 220, 220);

    for (int i = -numLines; i <= numLines; i++) {
        float x = (float)i;
        // Vertical line (along Z): clip against near plane so endpoints
        // behind the camera don't produce radiating artifacts.
        QPointF s1, s2;
        if (projectLine({x, 0, -gridExtent}, {x, 0, gridExtent}, s1, s2))
            renderer.addLine(s1, s2, gridColor, 0.5f, false);

        // Horizontal line (along X)
        if (projectLine({-gridExtent, 0, x}, {gridExtent, 0, x}, s1, s2))
            renderer.addLine(s1, s2, gridColor, 0.5f, false);
    }

    // Axes lines on the work plane (X horizontal, Z vertical on screen)
    QPointF origin, xAxis;
    if (projectLine({0, 0, 0}, {gridExtent, 0, 0}, origin, xAxis))
        renderer.addLine(origin, xAxis, axisColor, 1.0f, false);

    QPointF origin2, zAxis;
    if (projectLine({0, 0, 0}, {0, 0, gridExtent}, origin2, zAxis))
        renderer.addLine(origin2, zAxis, axisColor, 1.0f, false);
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
            cv->position.x + circ->radius, 0, cv->position.z
        });
        for (int i = 1; i <= segs; i++) {
            float a = 2 * M_PI * i / segs;
            Vec3 p = {
                cv->position.x + circ->radius * std::cos(a),
                0,
                cv->position.z + circ->radius * std::sin(a)
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

        // --- Cylinder / Cone rendering (pose-blended) -----------------
        // One unified algorithm for every pose. Each cap circle is drawn as
        // the image of the unit circle under a 2×2 screen matrix E — any
        // linear image of a circle is an exact ellipse:
        //   * E_strict — the true 斜二测 projection of the circle (linear
        //     part of Camera::project: frame axes + oblique shear). Exact
        //     for every pose; at the default view a level circle comes out
        //     with semi-axes 1.068r/0.331r but the major axis tilted ~7°.
        //   * E_textbook — the classroom 直观图 convention for level
        //     circles: horizontal major axis, semi-axes 1.06r / 0.35r.
        // They are blended by t = tAxis·tCam (smoothstep of axis
        // verticality × camera frontalness, zero when tilted >15°, orbited
        // >18°, or non-uniformly scaled), damped per frame
        // (shape->poseBlend, ~200 ms) so pose jumps MORPH instead of
        // snapping. Silhouettes / cone generators are solved in CLOSED
        // FORM against the blended ellipse, so they stay exactly tangent
        // at every t; per-segment dash rules blend the strict adjacency
        // test with the textbook "far half of the drawn ellipse" split.
        if (shape->type == ShapeType::Cylinder || shape->type == ShapeType::Cone) {
            const bool isCone = (shape->type == ShapeType::Cone);
            const Vec3 fwd = forward;
            const Vec3 ff  = camera.getFrameForward();
            const Vec3 uW = modelMat.transformDir({1, 0, 0});
            const Vec3 vW = modelMat.transformDir({0, 1, 0});
            const Vec3 aW = modelMat.transformDir({0, 0, 1});
            const float sU = uW.length(), sV = vW.length();
            if (sU < 1e-6f || sV < 1e-6f || aW.length() < 1e-6f) return;
            const Vec3 uN = uW * (1.0f / sU);
            const Vec3 vN = vW * (1.0f / sV);
            const Vec3 aN = aW.normalized();
            const float ppu = height() / camera.frustumSize;

            // Linear part of the strict projection, as SCREEN-pixel deltas
            // (y down — hence the negated up component).
            const float S = Camera::OBLIQUE_SHEAR;
            auto J = [&](const Vec3& w) {
                const float zs = -w.dot(ff);
                return QPointF((w.dot(camera.getRight()) + S * zs) * ppu,
                              -(w.dot(camera.getUp())    + S * zs) * ppu);
            };

            // Cap centers (px). For the cone, cTop IS the apex point.
            const QPointF cBot = worldToScreen(modelMat.transformPoint({0, 0, -1}));
            const QPointF cTop = worldToScreen(modelMat.transformPoint({0, 0,  1}));

            // ---- pose blend factor ------------------------------------
            auto smoothstep = [](float e0, float e1, float x) {
                float u = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
                return u * u * (3.0f - 2.0f * u);
            };
            const float tAxis = smoothstep(0.966f, 1.0f, std::abs(aN.z))    // ≤15° tilt
                              * (1.0f - smoothstep(0.01f, 0.05f,            // uniform scale
                                     std::abs(sU - sV) / sU));
            const float tCam  = smoothstep(0.951f, 1.0f, ff.y);             // ≤18° orbit
            const float tTgt  = tAxis * tCam;
            if (shape->poseBlend < 0.0f)        shape->poseBlend = tTgt;    // snap, 1st frame
            else shape->poseBlend += (tTgt - shape->poseBlend) * 0.25f;     // frame damping
            const float t = shape->poseBlend;
            if (std::abs(shape->poseBlend - tTgt) > 0.01f) update();        // settle morph

            // Blended generator columns: screen image of (cos a, sin a).
            // J already converts to px, so the strict columns are J(û)·sU
            // (NOT ·rPx — that would double-apply the px scale); the
            // textbook columns are the semi-axes 1.06r / 0.35r in px.
            const float rPx = sU * ppu;
            const QPointF b1 = J(uN) * (sU * (1.0f - t)) + QPointF(1.06f * rPx * t, 0.0f);
            const QPointF b2 = J(vN) * (sV * (1.0f - t)) + QPointF(0.0f, -0.35f * rPx * t);
            auto P = [&](const QPointF& c, float a) {
                return QPointF(c.x() + b1.x() * std::cos(a) + b2.x() * std::sin(a),
                               c.y() + b1.y() * std::cos(a) + b2.y() * std::sin(a));
            };

            // ---- strict facing tests (continuous in pose) --------------
            const float A = fwd.dot(uN), B = fwd.dot(vN), C = fwd.dot(aN);
            // Cone side outward normal ∝ (h cos a, h sin a, r) with h=2, r=1;
            // cylinder side normal = (cos a, sin a, 0). Visible when f·n < 0.
            auto sideVisible = [&](float a) {
                const float m = A * std::cos(a) + B * std::sin(a);
                return isCone ? (2.0f * m + C < 0.0f) : (m < 0.0f);
            };
            const bool topFar = (C > 0.0f);   // +axis cap faces away
            const bool botFar = (C < 0.0f);   // −axis cap faces away

            // ---- cap rims ----------------------------------------------
            constexpr int eseg = 64;
            auto drawRim = [&](const QPointF& c, bool capFar) {
                QPointF prev = P(c, 0.0f);
                float dashOff = 0.0f;
                for (int i = 1; i <= eseg; ++i) {
                    const float a2 = 2.0f * float(M_PI) * i / eseg;
                    const float am = a2 - float(M_PI) / eseg;
                    QPointF cur = P(c, a2);
                    float score = (1.0f - t) * ((capFar && !sideVisible(am)) ? 1.0f : 0.0f)
                                +        t  * ((capFar && std::sin(am) > 0.0f) ? 1.0f : 0.0f);
                    bool hid = score > 0.5f;
                    renderer.addLine(prev, cur, hid ? hiddenColor : drawColor,
                                     hwWire, hid, dashOff);
                    float dx = cur.x() - prev.x(), dy = cur.y() - prev.y();
                    dashOff += std::sqrt(dx * dx + dy * dy);
                    prev = cur;
                }
            };
            drawRim(cBot, botFar);

            if (isCone) {
                // Tangent generators apex → blended base ellipse, closed
                // form: with z = E⁻¹(apex − c), the tangent parameters are
                // a = atan2(z.y, z.x) ± acos(1/|z|)  (apex outside ⇒ |z|>1).
                const float det = b1.x() * b2.y() - b1.y() * b2.x();
                if (std::abs(det) > 1e-6f * rPx * rPx) {
                    const QPointF v = cTop - cBot;
                    const QPointF z(( b2.y() * v.x() - b2.x() * v.y()) / det,
                                    (-b1.y() * v.x() + b1.x() * v.y()) / det);
                    const float zz = std::sqrt(z.x() * z.x() + z.y() * z.y());
                    if (zz > 1.0001f) {
                        const float del = std::atan2(z.y(), z.x());
                        const float w   = std::acos(1.0f / zz);
                        for (float a : { del - w, del + w })
                            renderer.addLine(cTop, P(cBot, a), drawColor, hwWire, false);
                    }
                }
                return;
            }

            drawRim(cTop, topFar);

            // ---- cylinder side silhouettes ------------------------------
            // Lines parallel to the on-screen axis d, tangent to the blended
            // ellipse: −sin a·cross(b1,d) + cos a·cross(b2,d) = 0.
            const QPointF d(cTop.x() - cBot.x(), cTop.y() - cBot.y());
            const float c1 = b1.x() * d.y() - b1.y() * d.x();
            const float c2 = b2.x() * d.y() - b2.y() * d.x();
            if (c1 * c1 + c2 * c2 > 1e-12f * rPx * rPx) {
                const float a0 = std::atan2(c2, c1);
                for (float a : { a0, a0 + float(M_PI) })
                    renderer.addLine(P(cBot, a), P(cTop, a), drawColor, hwWire, false);
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
        //  * Equator: the great circle in the local XY plane (local z=0),
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

            // Equator: local XY-plane great circle (transformed by modelMat
            // so rotation/scale apply). Outward normal = radial direction.
            auto equPt = [&](float a) {
                return modelMat.transformPoint({std::cos(a), std::sin(a), 0.0f});
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

        // --- Torus rendering ----------------------------------------
        // 4 path-ring arcs (every 90° around the tube) and the silhouette
        // (mesh edges where a front-facing quad meets a back-facing quad)
        // are drawn solid only where actually visible. Both are occlusion-
        // tested against the torus body, so hidden parts — e.g. the far side
        // of the hole outline behind the near tube — are omitted.
        // Visibility uses an occlusion test, not surface-normal facing: the
        // torus is non-convex, so an inward-facing patch of the inner ring
        // can be visible through the hole while a front-facing patch is
        // hidden behind the near side. A surface point Q is hidden iff it
        // is the far surface along the view ray — stepping a little toward
        // the camera then lands inside the tube.
        if (shape->type == ShapeType::Torus) {
            const float R = 1.0f, r = 0.4f;
            constexpr int segMajor = 48;  // segments per major-ring arc

            auto localPt = [&](float u, float v) {
                return modelMat.transformPoint({(R + r * std::cos(v)) * std::cos(u),
                                                 (R + r * std::cos(v)) * std::sin(u),
                                                 r * std::sin(v)});
            };

            // Torus solid in world space, for the visibility test.
            const float scale = modelMat.transformDir({1.0f, 0.0f, 0.0f}).length();
            const Vec3 axisN = modelMat.transformDir({0.0f, 0.0f, 1.0f}).normalized();
            const float wR = R * scale, wr = r * scale;
            const float boundR = wR + wr;   // torus bounding-sphere radius

            // The camera is orthographic, so every view ray is parallel to
            // `forward` — NOT a perspective ray through the camera position.
            // A surface point Q is hidden iff the torus occludes itself along
            // that parallel ray: P(t) = Q + t*forward (t<0 toward the camera)
            // reaches the tube solid before Q. (Local near/far facing is not
            // enough: a near-facing patch on the far side of the ring is
            // hidden behind the near side; and a camera-position ray would be
            // the wrong ray for off-axis points such as the side cross-
            // sections.) Bounded march over the chord inside the bounding
            // sphere; the tube is at most ~2*wr across so a 0.5*wr step
            // catches any real occluder, with early exit on the first hit.
            auto isOccluded = [&](const Vec3& Q) {
                Vec3 relQ = Q - center;
                float rq = relQ.dot(forward);
                float disc = rq * rq - relQ.dot(relQ) + boundR * boundR;
                if (disc <= 0.0f) return false;          // Q outside bounding sphere
                float tNear = -rq - std::sqrt(disc);     // < 0: camera-side entry
                const float step = 0.5f * wr;
                const float wr2 = wr * wr;
                for (float t = tNear; t < -0.5f * wr; t += step) {
                    Vec3 rel = (Q + forward * t) - center;
                    float along = rel.dot(axisN);
                    float radial = (rel - axisN * along).length();
                    if ((radial - wR) * (radial - wR) + along * along < wr2)
                        return true;                     // occluder before Q
                }
                return false;
            };

            // --- 4 path-ring arcs: draw only the visible segments (solid) ---
            auto drawVisibleCircle = [&](int segs, auto ptAt) {
                QPointF prev = worldToScreen(ptAt(0.0f));
                for (int i = 0; i < segs; ++i) {
                    float midp = 2.0f * M_PI * (i + 0.5f) / segs;
                    QPointF cur = worldToScreen(ptAt(2.0f * M_PI * (i + 1) / segs));
                    if (!isOccluded(ptAt(midp)))
                        renderer.addLine(prev, cur, drawColor, hwWire, false);
                    prev = cur;
                }
            };
            for (int k = 0; k < 4; ++k) {
                float v = 2.0f * M_PI * k / 4;
                drawVisibleCircle(segMajor, [&](float u) { return localPt(u, v); });
            }

            // --- Silhouette: grid edges on the front/back facing boundary ---
            constexpr int Nu = 64, Nv = 32;
            std::vector<Vec3> grid(size_t(Nu) * Nv);
            std::vector<unsigned char> front(size_t(Nu) * Nv);
            for (int i = 0; i < Nu; ++i)
                for (int j = 0; j < Nv; ++j)
                    grid[size_t(i) * Nv + j] =
                        localPt(2.0f * M_PI * i / Nu, 2.0f * M_PI * j / Nv);
            auto G = [&](int i, int j) -> const Vec3& {
                return grid[size_t((i + Nu) % Nu) * Nv + size_t((j + Nv) % Nv)];
            };
            for (int i = 0; i < Nu; ++i)
                for (int j = 0; j < Nv; ++j) {
                    Vec3 n = (G(i + 1, j) - G(i, j)).cross(G(i, j + 1) - G(i, j));
                    front[size_t(i) * Nv + j] = (n.dot(forward) < 0.0f) ? 1 : 0;
                }
            auto F = [&](int i, int j) -> bool {
                return front[size_t((i + Nu) % Nu) * Nv + size_t((j + Nv) % Nv)] != 0;
            };
            for (int i = 0; i < Nu; ++i)
                for (int j = 0; j < Nv; ++j) {
                    if (F(i, j) != F(i, j - 1)) {
                        Vec3 mid = (G(i, j) + G(i + 1, j)) * 0.5f;
                        if (!isOccluded(mid))
                            renderer.addLine(worldToScreen(G(i, j)), worldToScreen(G(i + 1, j)),
                                             drawColor, hwWire, false);
                    }
                    if (F(i, j) != F(i - 1, j)) {
                        Vec3 mid = (G(i, j) + G(i, j + 1)) * 0.5f;
                        if (!isOccluded(mid))
                            renderer.addLine(worldToScreen(G(i, j)), worldToScreen(G(i, j + 1)),
                                             drawColor, hwWire, false);
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
                // (`forward` is camera.getForward() — under the oblique
                // (斜二测) camera this is the projector direction, so an
                // axis-aligned cube shows front/top/right solid and dashes
                // exactly the 3 edges at the far-bottom-left corner.)
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
    // Match geomTool (Three.js EndpointMarkers.ts) style exactly:
    // Each endpoint = filled circle + thin colored ring border.
    // The fill occludes the line/arc near the endpoint (depthTest=false in Three.js,
    // equivalent to drawing after the GL layer in QPainter).
    //
    // Colors (from EndpointMarkers.ts):
    //   Line start/end  : white fill (0xffffff) + orange ring (0xff6b35)
    //   Arc center      : green fill (0x00ff00) + dark-green ring (0x00aa00)
    //   Arc start/end   : white fill (0xffffff) + orange ring (0xff6b35)
    //
    // Size: 0.0075 * frustumSize in world units → ~constant on-screen size
    //   (matches calculateCircleRadius / 2 after updateAllEndpointsScale).

    // Convert world-space radius to screen pixels.
    // React: circleR = 0.0075 * frustumSize (world).
    // Screen: px = world / (frustumSize/2) * (height/2) = world * height / frustumSize
    // So: screenR = 0.0075 * frustumSize * height / frustumSize = 0.0075 * height
    float markerR = 0.0075f * (float)height();
    markerR = std::max(markerR, 5.0f);  // minimum legible size
    float ringWidth = markerR * 0.12f;   // thin ring (~10% of radius, matching RING_INNER_RATIO=0.9)

    const QColor fillColor(255, 255, 255, 242);      // white, 0.95 opacity
    const QColor ringColor(255, 107, 53);             // orange #ff6b35
    const QColor centerFill(0, 255, 0, 242);          // green, 0.95 opacity
    const QColor centerRing(0, 170, 0);               // dark green #00aa00

    if (shape->type == ShapeType::LineSegment) {
        Vec3 startPos, endPos;
        getSelectedLineEndpoints(startPos, endPos);
        QPointF s1 = worldToScreen(startPos);
        QPointF s2 = worldToScreen(endPos);

        // Filled white circle + orange ring border
        painter.setBrush(fillColor);
        painter.setPen(QPen(ringColor, ringWidth));
        painter.drawEllipse(s1, markerR, markerR);
        painter.drawEllipse(s2, markerR, markerR);

    } else if (shape->type == ShapeType::CircularArc) {
        Vec3 center, start, end;
        getSelectedArcPoints(center, start, end);
        QPointF sc = worldToScreen(center);
        QPointF ss = worldToScreen(start);
        QPointF se = worldToScreen(end);

        // Center — green fill + dark green ring
        painter.setBrush(centerFill);
        painter.setPen(QPen(centerRing, ringWidth));
        painter.drawEllipse(sc, markerR, markerR);

        // Start/End — white fill + orange ring
        painter.setBrush(fillColor);
        painter.setPen(QPen(ringColor, ringWidth));
        painter.drawEllipse(ss, markerR, markerR);
        painter.drawEllipse(se, markerR, markerR);
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
    Vec3 frameFwd = camera.getFrameForward();
    Vec3 right = camera.getRight();
    Vec3 up = camera.getUp();

    auto projectAxis = [&](const Vec3& axisDir, const QColor& color, const QString& label) {
        // Screen direction under the FULL oblique map (frame projection
        // plus the 斜二测 shear), so the depth axis (Y under the default
        // frontal view) shows as the textbook 45° up-right half-length
        // arrow instead of collapsing onto the gizmo center.
        float zs = -axisDir.dot(frameFwd);
        float xProj = axisDir.dot(right) + Camera::OBLIQUE_SHEAR * zs;
        float yProj = axisDir.dot(up)    + Camera::OBLIQUE_SHEAR * zs;

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
