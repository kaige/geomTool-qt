#include <QApplication>
#include <QSurfaceFormat>
#include <QFontDatabase>
#include <QFont>
#include <QTimer>
#include <QDebug>
#include <QMouseEvent>
#include <QPointF>
#include <cstdio>
#include <cstring>
#include "MainWindow.h"
#include "CanvasWidget.h"
#include "GeometryStore.h"
#include "tools/ToolManager.h"

int main(int argc, char* argv[]) {
    // Request an OpenGL context with MSAA before any widget is created.
    // Desktop: 3.3 Core profile (resolves to 4.1 on macOS).
    // WASM: GLES defaults (WebGL 2 emulation, no version/profile override needed).
    QSurfaceFormat fmt;
#ifndef Q_OS_WASM
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
#endif
    fmt.setSamples(4); // 4x MSAA for smooth line edges
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setApplicationName("GeomTool");
    app.setOrganizationName("kaige");

    // Load embedded font (needed for WASM where no system fonts exist)
    int fontId = QFontDatabase::addApplicationFont(":/fonts/HiraginoSansGB-Subset.ttf");
    if (fontId != -1) {
        QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty()) {
            QFont appFont(families.first(), 10);
            app.setFont(appFont);
        }
    }

    MainWindow window;
    window.show();

    // --demo: seed a sample scene so the app can be launched headlessly for
    // visual verification (e.g. hidden-line rendering). No effect on normal use.
    bool demo = false;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--demo") == 0) demo = true;
    if (demo) {
        window.setWindowTitle("GeomTool v1.0 -- DEMO");
        g_store.addShape3D(ShapeType::Torus);
        if (g_canvas) {
            g_canvas->camera.frustumSize = 5.0f; // frame the shape; angles stay at the default frontal XZ view
            g_canvas->update();
        }
    }

    // --rot rx,ry,rz: apply an initial rotation to the most recent shape
    // (headless test hook for verifying non-default poses with --shot).
    for (int i = 1; i < argc - 3; ++i) {
        if (std::strcmp(argv[i], "--rot") == 0 && !g_store.shapes.empty()) {
            float rx, ry, rz;
            if (std::sscanf(argv[i + 1], "%f", &rx) == 1 &&
                std::sscanf(argv[i + 2], "%f", &ry) == 1 &&
                std::sscanf(argv[i + 3], "%f", &rz) == 1)
                g_store.shapes.back()->rotation = {rx, ry, rz};
        }
    }

    // --snapdemo [target dx dy]: headless snap verification scene — a cube
    // and a cone with the line tool active, plus a synthetic hover offset
    // (dx,dy) pixels from a 3D feature point. target 0 = the cube's
    // front-bottom-left corner, 1 = the cone apex. Combined with --shot
    // this verifies that the snap marker sticks to solid feature points.
    // Extra flag --snapdrag: after the hover, drag the mouse from the cube
    // corner to the cone apex (press → move → release), creating a line —
    // verifies that drawn geometry actually lands on the feature points.
    bool dragDemo = false;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--snapdrag") == 0) dragDemo = true;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--snapdemo") != 0) continue;
        int target = 0; float hdx = 10.0f, hdy = 10.0f;
        if (i + 1 < argc) std::sscanf(argv[i + 1], "%d", &target);
        if (i + 2 < argc) std::sscanf(argv[i + 2], "%f", &hdx);
        if (i + 3 < argc) std::sscanf(argv[i + 3], "%f", &hdy);
        g_store.addShape3D(ShapeType::Cube, {-1.6f, 0.5f, 0});
        g_store.addShape3D(ShapeType::Cone,  { 1.6f, 0.5f, 0});
        if (g_canvas) {
            g_canvas->toolManager->activateTool(ToolType::CreateLineSegment);
            QTimer::singleShot(400, [target, hdx, hdy, dragDemo]() {
                auto hover = [](const QPointF& p) {
                    QMouseEvent e(QEvent::MouseMove, p,
                                  g_canvas->mapToGlobal(p.toPoint()),
                                  Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                    QApplication::sendEvent(g_canvas, &e);
                    g_canvas->update();
                };
                auto click = [](QEvent::Type t, const QPointF& p) {
                    Qt::MouseButton btn = (t == QEvent::MouseButtonPress ||
                                           t == QEvent::MouseButtonRelease)
                                        ? Qt::LeftButton : Qt::NoButton;
                    Qt::MouseButtons buttons = (t == QEvent::MouseButtonPress)
                                        ? Qt::LeftButton : Qt::NoButton;
                    QMouseEvent e(t, p, g_canvas->mapToGlobal(p.toPoint()),
                                  btn, buttons, Qt::NoModifier);
                    QApplication::sendEvent(g_canvas, &e);
                    g_canvas->update();
                };
                auto featureScreen = [](int which) {
                    Vec3 local = (which == 1) ? Vec3{0, 0, 1}             // cone apex
                                              : Vec3{-0.5f, -0.5f, -0.5f}; // cube corner
                    BaseShape* s = (which == 1) ? g_store.shapes.back().get()
                                                : g_store.shapes.front().get();
                    return g_canvas->worldToScreen(s->position + local);
                };

                if (!dragDemo) {
                    hover(featureScreen(target) + QPointF(hdx, hdy));
                    return;
                }
                QPointF a = featureScreen(0) + QPointF(hdx, hdy);
                QPointF b = featureScreen(1) + QPointF(-8, 8);
                hover(a);
                QTimer::singleShot(300, [a, b, click, hover]() {
                    click(QEvent::MouseButtonPress, a);
                    QTimer::singleShot(250, [a, b, click, hover]() {
                        hover(b);
                        click(QEvent::MouseMove, b);
                        QTimer::singleShot(250, [b, click]() {
                            click(QEvent::MouseButtonRelease, b);
                        });
                    });
                });
            });
        }
        break;
    }

    // --shot <path>: after the first renders, dump the canvas framebuffer
    // (QOpenGLWidget::grabFramebuffer — includes the QPainter overlays) to
    // <path> and quit. Headless visual verification that does not depend on
    // system screen capture (which needs TCC screen-recording and a live
    // display — unreliable on this headless Mac mini).
    // --shotdelay <ms>: override the default 800 ms wait (use with --snapdemo
    // --snapdrag, whose synthetic event chain needs a longer window on slow
    // software-GL renders).
    const char* shotPath = nullptr;
    int shotDelay = 800;
    for (int i = 1; i < argc - 1; ++i)
        if (std::strcmp(argv[i], "--shot") == 0) shotPath = argv[i + 1];
    for (int i = 1; i < argc - 1; ++i)
        if (std::strcmp(argv[i], "--shotdelay") == 0) std::sscanf(argv[i + 1], "%d", &shotDelay);
    if (shotPath && g_canvas) {
        QTimer::singleShot(shotDelay, [shotPath]() {
            QImage fb = g_canvas->grabFramebuffer();
            bool ok = fb.save(QString::fromLocal8Bit(shotPath));
            qDebug("shot %s: %s", shotPath, ok ? "saved" : "FAILED");
            QApplication::quit();
        });
    }

    return app.exec();
}
