#pragma once
#include <QWidget>
#include <QPixmap>
#include <QPointF>
#include "Camera.h"
#include "Types.h"
#include "Math3D.h"
#include "I18n.h"
#include "GeometryFactory.h"
#include "tools/BaseTool.h"

class ToolManager;
class SnapManager;

// ============================================================
// CanvasWidget - Custom QWidget that renders all geometry
// Uses QPainter for 2D rendering of 3D-projected geometry
// ============================================================
class CanvasWidget : public QWidget {
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
    int snapType = 0; // 0=endpoint, 1=center, 2=midpoint

    void clearTempLines() { tempLines.clear(); update(); }

    // Screen <-> World conversion helpers
    Vec3 screenToWorld(float sx, float sy);
    Vec3 screenToWorldOnZ0Plane(float sx, float sy);
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

    // Set cursor
    void setCanvasCursor(Qt::CursorShape shape);

    // Trigger repaint
    void refresh();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void drawAxes(QPainter& painter);
    void drawGrid(QPainter& painter);
    void drawShapes(QPainter& painter);
    void drawShape(QPainter& painter, BaseShape* shape);
    void drawEndpoints(QPainter& painter, BaseShape* shape);
    void drawSnapMarker(QPainter& painter);
    void drawTempLines(QPainter& painter);
};

// Global canvas pointer for toolbar access
extern CanvasWidget* g_canvas;
