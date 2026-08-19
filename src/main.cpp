#include <QApplication>
#include <QSurfaceFormat>
#include <QFontDatabase>
#include <QFont>
#include <QTimer>
#include <QDebug>
#include <cstdio>
#include <cstring>
#include "MainWindow.h"
#include "CanvasWidget.h"
#include "GeometryStore.h"

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

    // --shot <path>: after the first renders, dump the canvas framebuffer
    // (QOpenGLWidget::grabFramebuffer — includes the QPainter overlays) to
    // <path> and quit. Headless visual verification that does not depend on
    // system screen capture (which needs TCC screen-recording and a live
    // display — unreliable on this headless Mac mini).
    const char* shotPath = nullptr;
    for (int i = 1; i < argc - 1; ++i)
        if (std::strcmp(argv[i], "--shot") == 0) shotPath = argv[i + 1];
    if (shotPath && g_canvas) {
        QTimer::singleShot(800, [shotPath]() {
            QImage fb = g_canvas->grabFramebuffer();
            bool ok = fb.save(QString::fromLocal8Bit(shotPath));
            qDebug("shot %s: %s", shotPath, ok ? "saved" : "FAILED");
            QApplication::quit();
        });
    }

    return app.exec();
}
