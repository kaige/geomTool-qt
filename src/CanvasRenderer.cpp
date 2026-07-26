#include "CanvasRenderer.h"
#include <QOpenGLShader>
#include <QVector2D>
#include <cmath>

namespace {
// ----------------------------------------------------------------
// Line vertex shader.
// Each segment is expanded into a screen-space quad (two
// triangles) perpendicular to the segment direction, giving a
// constant pixel thickness regardless of zoom or platform.
// ----------------------------------------------------------------
const char* kLineVS = R"(
#version 330 core
layout(location = 0) in vec2 aA;          // segment start (screen px)
layout(location = 1) in vec2 aB;          // segment end   (screen px)
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aT;         // 0..1 along segment
layout(location = 4) in float aSide;      // -1 / +1 perpendicular
layout(location = 5) in float aHalfWidth; // px
layout(location = 6) in float aSegLen;    // px length
layout(location = 7) in float aDashed;    // 0 or 1

uniform vec2 uViewport;

out vec4 vColor;
out float vT;
out float vSegLen;
out float vDashed;

void main() {
    vec2 d = aB - aA;
    float L = length(d);
    // Unit perpendicular to the segment direction.
    vec2 nrm = (L > 0.0001) ? vec2(-d.y, d.x) / L : vec2(0.0, 1.0);
    vec2 scr = mix(aA, aB, aT) + nrm * (aSide * aHalfWidth);
    // Screen px (y-down) -> NDC (y-up), matching QPainter/QOpenGLWidget.
    vec2 ndc = vec2(2.0 * scr.x / uViewport.x - 1.0,
                    1.0 - 2.0 * scr.y / uViewport.y);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vColor = aColor;
    vT = aT;
    vSegLen = aSegLen;
    vDashed = aDashed;
}
)";

const char* kLineFS = R"(
#version 330 core
in vec4 vColor;
in float vT;
in float vSegLen;
in float vDashed;

uniform float uDashLen; // half-period of the dash pattern (px)

out vec4 frag;

void main() {
    if (vDashed > 0.5) {
        float dist = vT * vSegLen;
        float m = mod(dist, uDashLen * 2.0);
        if (m > uDashLen) discard;
    }
    frag = vColor;
}
)";

// Corner (t, side) pairs for the two triangles of one quad:
//   (0,-1),(0,+1),(1,+1)   and   (0,-1),(1,+1),(1,-1)
const float kCornerT[6]    = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f };
const float kCornerSide[6] = { -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f };

const float kDashHalfPeriod = 8.0f; // px
} // namespace

CanvasRenderer::~CanvasRenderer() {
    // The GL context is torn down together with the widget, so there
    // is no current context here in which to glDelete*. The VAO/VBO
    // are released with the context; this is acceptable for the single
    // long-lived canvas widget.
}

bool CanvasRenderer::initialize() {
    initializeOpenGLFunctions();

    if (!m_lineProg.addShaderFromSourceCode(QOpenGLShader::Vertex,   kLineVS)) return false;
    if (!m_lineProg.addShaderFromSourceCode(QOpenGLShader::Fragment, kLineFS)) return false;
    if (!m_lineProg.link()) return false;

    glGenVertexArrays(1, &m_lineVAO);
    glGenBuffers(1, &m_lineVBO);

    glBindVertexArray(m_lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO);
    setupLineAttributes();
    glBindVertexArray(0);

    m_ok = true;
    return true;
}

void CanvasRenderer::setupLineAttributes() {
    const GLsizei stride = sizeof(LineVert);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(LineVert, ax)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(LineVert, bx)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(LineVert, r)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(LineVert, t)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(LineVert, side)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(LineVert, halfWidth)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(LineVert, segLen)));
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(LineVert, dashed)));
}

void CanvasRenderer::beginFrame(int viewportW, int viewportH) {
    m_vpW = viewportW > 0 ? viewportW : 1;
    m_vpH = viewportH > 0 ? viewportH : 1;
    m_lineVerts.clear();
}

void CanvasRenderer::addLine(const QPointF& a, const QPointF& b,
                             const QColor& color, float halfWidthPx, bool dashed) {
    const float ax = float(a.x()), ay = float(a.y());
    const float bx = float(b.x()), by = float(b.y());
    const float dx = bx - ax, dy = by - ay;
    const float segLen = std::sqrt(dx * dx + dy * dy);

    if (m_lineVerts.empty()) m_lineVerts.reserve(2048);

    LineVert v;
    v.ax = ax; v.ay = ay; v.bx = bx; v.by = by;
    v.r = color.redF(); v.g = color.greenF();
    v.b = color.blueF(); v.a = color.alphaF();
    v.halfWidth = halfWidthPx;
    v.segLen = segLen;
    v.dashed = dashed ? 1.0f : 0.0f;

    for (int i = 0; i < 6; ++i) {
        v.t = kCornerT[i];
        v.side = kCornerSide[i];
        m_lineVerts.push_back(v);
    }
}

void CanvasRenderer::endFrame() {
    if (!m_ok) return;

    m_lineProg.bind();
    m_lineProg.setUniformValue("uViewport", QVector2D(float(m_vpW), float(m_vpH)));
    m_lineProg.setUniformValue("uDashLen", kDashHalfPeriod);

    glBindVertexArray(m_lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO);

    if (!m_lineVerts.empty()) {
        glBufferData(GL_ARRAY_BUFFER,
                     GLsizeiptr(m_lineVerts.size() * sizeof(LineVert)),
                     m_lineVerts.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, GLsizei(m_lineVerts.size()));
    }

    glBindVertexArray(0);
    m_lineProg.release();
}
