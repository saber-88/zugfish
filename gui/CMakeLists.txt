cmake_minimum_required(VERSION 3.21)
project(zugfish-qt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qt >= 6.4. (Arch: qt6-base qt6-declarative qt6-svg)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Svg)
qt_standard_project_setup()

qt_add_executable(zugfish-qt
    src/main.cpp
    src/AppController.cpp
    src/PlyModel.cpp
    src/OnlineGamesModel.cpp
    src/backend/DemoBackend.cpp
)

# --- QML module: every UI file lives here -----------------------------------
set(ZQ_QML_FILES
    Main.qml Theme.qml icons.js Icon.qml ClassBadge.qml IconButton.qml AppButton.qml
    Chip.qml Card.qml SegmentedTabs.qml AppTextField.qml Toggle.qml ThemedScrollBar.qml
    Toast.qml TopBar.qml WelcomeView.qml PlayerStrip.qml ChessBoard.qml EvalBar.qml
    EvalGraph.qml BoardControls.qml AccuracyRing.qml ReviewPanel.qml MovesPanel.qml
    DetailsPanel.qml ImportDialog.qml SettingsDialog.qml)

# Sources live in qml/, but inside the module they sit at its root, next to the generated
# qmldir. That is what makes the Theme singleton visible to every component.
set(ZQ_QML_PATHS "")
foreach(f IN LISTS ZQ_QML_FILES)
    set_source_files_properties(qml/${f} PROPERTIES QT_RESOURCE_ALIAS ${f})
    list(APPEND ZQ_QML_PATHS qml/${f})
endforeach()
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# Qt 6.4's ahead-of-time QML compiler is experimental and can hang on larger files.
if(Qt6_VERSION VERSION_LESS 6.5)
    set(ZQ_QML_CACHE_OPTION NO_CACHEGEN)
endif()

qt_add_qml_module(zugfish-qt
    URI Zugfish
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    ${ZQ_QML_CACHE_OPTION}
    QML_FILES ${ZQ_QML_PATHS}
)

# --- Non-QML resources (referenced as qrc:/assets/..., :/data/...) ------------
qt_add_resources(zugfish-qt assets
    PREFIX /
    FILES
        assets/fonts/IBMPlexSans-Regular.ttf
        assets/fonts/IBMPlexSans-Medium.ttf
        assets/fonts/IBMPlexSans-SemiBold.ttf
        assets/fonts/IBMPlexSans-Bold.ttf
        assets/fonts/IBMPlexMono-Medium.ttf
        assets/pieces/wK.svg assets/pieces/wQ.svg assets/pieces/wR.svg
        assets/pieces/wB.svg assets/pieces/wN.svg assets/pieces/wP.svg
        assets/pieces/bK.svg assets/pieces/bQ.svg assets/pieces/bR.svg
        assets/pieces/bB.svg assets/pieces/bN.svg assets/pieces/bP.svg
        data/demo_games.json
        data/samples.json
)

target_link_libraries(zugfish-qt PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Svg)
target_compile_options(zugfish-qt PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>)

# --- Optional: use your real zugfish core instead of the demo data -----------
#   cmake -B build -DZUGFISH_CORE_DIR=/path/to/zugfish
# Or automatically when built from root CMakeLists.txt
if(TARGET zugfish_core)
    target_sources(zugfish-qt PRIVATE src/backend/ZugfishBackend.cpp)
    target_compile_definitions(zugfish-qt PRIVATE ZUGFISH_WITH_CORE)
    target_link_libraries(zugfish-qt PRIVATE zugfish_core)
else()
    set(ZUGFISH_CORE_DIR "" CACHE PATH "Path to your zugfish core checkout (enables the real backend)")
    if(ZUGFISH_CORE_DIR)
        file(GLOB ZUGFISH_CORE_SOURCES CONFIGURE_DEPENDS "${ZUGFISH_CORE_DIR}/core/*.cpp")
        target_sources(zugfish-qt PRIVATE src/backend/ZugfishBackend.cpp ${ZUGFISH_CORE_SOURCES})
        target_include_directories(zugfish-qt PRIVATE
            "${ZUGFISH_CORE_DIR}/core" "${ZUGFISH_CORE_DIR}/third_party")
        target_compile_definitions(zugfish-qt PRIVATE ZUGFISH_WITH_CORE)
        find_package(CURL REQUIRED)
        find_package(nlohmann_json REQUIRED)
        target_link_libraries(zugfish-qt PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS zugfish-qt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
