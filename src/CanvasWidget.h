#pragma once
#include <QOpenGLWidget>
#include <QtGlobal>  // for Q_OS_WASM define

// Cross-platform OpenGL function set.
#ifdef Q_OS_WASM
#include <QOpenGLFunctions>
using GLFunctions = QOpenGLFunctions;
#else
#include <QOpenGLFunctions_3_3_Core>
using GLFunctions = QOpenGLFunctions_3_3_Core;
#endif
#include <QPointF>
#include "Camera.h"
#include "Types.h"
#include "Math3D.h"
#include "I18n.h"
#include "GeometryFactory.h"
#include "CanvasRenderer.h"
#include "tools/BaseTool.h"

class ToolManager;
class SnapManager;

// ============================================================
// CanvasWidget - OpenGL-backed canvas that renders all geometry.
//
// Geometry (grid, shapes, tool preview lines) is drawn through a
// programmable OpenGL 3.3 Core pipeline (see CanvasRenderer).
// Small screen-space overlays (selection handles, snap markers,
// the axis gizmo with its text labels) are composited on top with
// QPainter, which QOpenGLWidget supports natively.
//
// Projection to screen pixels stays on the CPU (Camera::project),
// so GL geometry and QPainter overlays are always pixel-aligned.
// ============================================================
class CanvasWidget : public QOpenGLWidget, protected GLFunctions {
    Q_OBJECT
public:
    explicit CanvasWidget(QWidget* parent = nullptr);
    ~CanvasWidget();

    Camera camera;
    std::unique_ptr<ToolManager> toolManager;
    std::unique_ptr<SnapManager> snapManager;

    // Mouse state
    bool mouseDown = false;
    QPointF mousePos;
    QPointF startMousePos;
    bool isRotatingCamera = false;
    float cameraRotationStartAzimuth = 0;
    float cameraRotationStartElevation = 0;

    // Selection drag state
    bool isDraggingObject = false;
    bool isRotatingObject = false;
    char rotationAxis = 0; // 'x', 'y', 'z'
    Vec3 dragStartWorldPos;
    Vec3 dragStartObjectPos;
    Vec3 dragStartObjectRotation;

    // Endpoint drag state
    bool isDraggingEndpoint = false;

    // Temp geometry for tools (lines, arcs, dotted lines)
    struct TempLine {
        std::vector<Vec3> points;
        QColor color;
        bool dashed = false;
    };
    std::vector<TempLine> tempLines;

    // Snap marker state
    bool snapVisible = false;
    Vec3 snapPosition;
    int snapType = 0; // 0=endpoint, 1=center, 2=midpoint, 3=vertex(feature point)

    void clearTempLines() { tempLines.clear(); update(); }

    // Screen <-> World conversion helpers
    Vec3 screenToWorld(float sx, float sy);
    Vec3 screenToWorldOnWorkPlane(float sx, float sy);
    Vec3 screenToWorldOnPlane(float sx, float sy, const Vec3& normal, const Vec3& point);

    // Hit testing
    std::string hitTestShape(float sx, float sy);
    bool hitTestLineEndpoint(float sx, float sy, std::string& outLineId, bool& outIsStart);
    bool hitTestArcPoint(float sx, float sy, std::string& outArcId, int& outPointType);
    // pointType: 0=center, 1=start, 2=end

    // Get endpoints for a selected line/arc shape (for rendering markers)
    void getSelectedLineEndpoints(Vec3& start, Vec3& end);
    void getSelectedArcPoints(Vec3& center, Vec3& start, Vec3& end);

    // Convert shape to screen points for rendering
    QPointF worldToScreen(const Vec3& world);

    // Project a world-space line segment to screen coordinates, clipping
    // against the camera's near plane. Returns false if the entire segment
    // is behind the camera. Handles partial-visibility by clamping the
    // off-screen endpoint to the near-plane intersection.
    bool projectLine(const Vec3& a, const Vec3& b, QPointF& sa, QPointF& sb);

    // Set cursor
    void setCanvasCursor(Qt::CursorShape shape);

    // Trigger repaint
    void refresh();

protected:
    // QOpenGLWidget rendering hooks
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    CanvasRenderer renderer;

    // GL-layer geometry (projected to screen px, drawn via OpenGL)
    void drawGrid();
    void drawShapes();
    void drawShape(BaseShape* shape);
    void drawTempLines();

    // QPainter overlays (drawn on top of the GL layer)
    void drawEndpoints(QPainter& painter, BaseShape* shape);
    void drawSnapMarker(QPainter& painter);
    void drawAxes(QPainter& painter);
};

// Global canvas pointer for toolbar access
extern CanvasWidget* g_canvas;
