cmake_minimum_required(VERSION 3.16)
project(geomTool-qt VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(WASM_BUILD "Building for WebAssembly" OFF)

# Qt module list — SVG is optional (only needed for icon rendering on desktop;
# WASM builds may not always have the Svg module readily available).
if(WASM_BUILD)
    find_package(Qt6 COMPONENTS Widgets OpenGL OpenGLWidgets REQUIRED)
else()
    find_package(Qt6 COMPONENTS Widgets OpenGL OpenGLWidgets Svg REQUIRED)
endif()

add_executable(geomTool
    src/main.cpp
    src/MainWindow.cpp
    src/CanvasWidget.cpp
    src/CanvasRenderer.cpp
    src/GeometryStore.cpp
    src/Math3D.cpp
    src/SnapManager.cpp
    src/I18n.cpp
    src/ToolbarWidget.cpp
    src/ShapeListWidget.cpp
    src/StatusBarWidget.cpp
    src/tools/ToolManager.cpp
    src/tools/SelectTool.cpp
    src/tools/MoveShapeTool.cpp
    src/tools/RotateShapeTool.cpp
    src/tools/LineSegmentTool.cpp
    src/tools/CircularArcTool.cpp
    src/tools/CreateShape3DTool.cpp
    src/tools/MoveLineEndpointTool.cpp
    src/tools/MoveArcEndpointTool.cpp
    src/tools/MoveArcTool.cpp
    resources/icons.qrc
)

target_link_libraries(geomTool PRIVATE
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)
if(NOT WASM_BUILD)
    target_link_libraries(geomTool PRIVATE Qt6::Svg)
endif()

target_include_directories(geomTool PRIVATE src src/tools)

# WASM-specific settings
if(WASM_BUILD OR EMSCRIPTEN)
    # Qt's WASM platform plugin uses embind for JS<->C++ interop,
    # and WebGL2 for OpenGL rendering. ALLOW_MEMORY_GROWTH is needed
    # because Qt allocates a lot of heap memory.
    set_target_properties(geomTool PROPERTIES
        LINK_FLAGS "\
            -s USE_WEBGL2=1 \
            -s WASM=1 \
            -s ALLOW_MEMORY_GROWTH=1 \
            -s LLD_REPORT_UNDEFINED \
            --bind \
        "
    )
endif()

# Qt6::OpenGL links the platform OpenGL library on every OS
# (OpenGL32 on Windows, libGL on Linux, the OpenGL framework on macOS),
# so no per-platform GL linking is needed here.

if(APPLE)
    set_target_properties(geomTool PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.kaige.geomTool"
    )
endif()

install(TARGETS geomTool DESTINATION bin)
