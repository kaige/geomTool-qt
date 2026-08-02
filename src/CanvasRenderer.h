#pragma once

#include <QtGlobal>  // for Q_OS_WASM define

// Cross-platform OpenGL function set.
// Desktop uses Core 3.3; WebAssembly uses GLES 3.0 (WebGL 2).
#ifdef Q_OS_WASM
#include <QOpenGLFunctions>
using GLFunctions = QOpenGLFunctions;
#else
#include <QOpenGLFunctions_3_3_Core>
using GLFunctions = QOpenGLFunctions_3_3_Core;
#endif
#include <QOpenGLShaderProgram>
#include <QPointF>
#include <QColor>
#include <vector>
#include <cstddef>

// ============================================================
// CanvasRenderer - OpenGL renderer for the canvas geometry.
//
// Renders every line segment on the canvas (grid, shapes,
// tool preview lines) as screen-space triangle quads so line
// width is honored on every platform - including macOS, where
// the core-profile context clamps aliased line width to 1px.
//
// Projection to screen pixels is performed on the CPU (via
// Camera::project) and the result is fed to the GPU, which
// keeps the GL geometry pixel-aligned with the QPainter
// overlays (selection handles, snap markers, axis gizmo).
//
// Targets OpenGL 3.3 Core / GLES 3.0 for full cross-platform support:
//   - desktop Windows / Linux : native 3.3+ driver
//   - macOS                   : resolves to a 4.1 core context
//   - WebAssembly (browser)   : WebGL 2 via GLES 3.0 emulation
// ============================================================
class CanvasRenderer : protected GLFunctions {
public:
    CanvasRenderer() = default;
    ~CanvasRenderer();

    // Compile shaders + create VAO/VBO. Call once in initializeGL().
    // Returns false if shader compilation / linking failed.
    bool initialize();

    // Begin a new frame. viewportW/H are the widget size in pixels.
    // Clears any pending line batches.
    void beginFrame(int viewportW, int viewportH);

    // Queue a thick line segment between two screen-space points.
    // halfWidthPx is half the desired line thickness in pixels.
    // dashOff (px) advances the dash phase — pass a cumulative screen
    // length so consecutive segments of one dashed curve (e.g. a sphere's
    // equator) share a continuous dash pattern instead of restarting each
    // segment, which makes small shapes blur into a solid line.
    void addLine(const QPointF& a, const QPointF& b,
                 const QColor& color, float halfWidthPx, bool dashed,
                 float dashOff = 0.0f);

    // Upload + draw all queued geometry for this frame.
    void endFrame();

private:
    // Interleaved vertex for one corner of a line quad.
    struct LineVert {
        float ax, ay;        // segment start (screen px)
        float bx, by;        // segment end   (screen px)
        float r, g, b, a;    // color
        float t;             // 0..1 position along segment
        float side;          // -1 / +1 perpendicular offset
        float halfWidth;     // px
        float segLen;        // px length (for dash distance)
        float dashed;        // 0 or 1
        float dashOff;       // px phase offset along the dash pattern
    };

    QOpenGLShaderProgram m_lineProg;
    GLuint m_lineVAO = 0;
    GLuint m_lineVBO = 0;
    std::vector<LineVert> m_lineVerts;
    int m_vpW = 1;
    int m_vpH = 1;
    bool m_ok = false;

    void setupLineAttributes();
};
