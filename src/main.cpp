#include <QApplication>
#include <QSurfaceFormat>
#include <cstring>
#include "MainWindow.h"
#include "CanvasWidget.h"
#include "GeometryStore.h"

int main(int argc, char* argv[]) {
    // Request an OpenGL 3.3 Core context with MSAA before any widget is
    // created. This profile is the cross-platform sweet spot: desktop
    // Windows/Linux use a native 3.3+ driver and macOS resolves it to a
    // 4.1 core context (its highest supported core profile).
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4); // 4x MSAA for smooth line edges
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setApplicationName("GeomTool");
    app.setOrganizationName("kaige");

    MainWindow window;
    window.show();

    // --demo: seed a sample scene so the app can be launched headlessly for
    // visual verification (e.g. hidden-line rendering). No effect on normal use.
    bool demo = false;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--demo") == 0) demo = true;
    if (demo) {
        window.setWindowTitle("GeomTool v1.0 -- DEMO");
        g_store.addShape3D(ShapeType::Cube);
        if (!g_store.shapes.empty())
            g_store.selectShape(g_store.shapes.back()->id); // selected so Delete can be exercised
        if (g_canvas) {
            g_canvas->camera.azimuth = 0.6f;
            g_canvas->camera.elevation = 0.4f;
            g_canvas->camera.frustumSize = 5.0f; // larger cube for easier face clicking
            g_canvas->update();
        }
    }

    return app.exec();
}
