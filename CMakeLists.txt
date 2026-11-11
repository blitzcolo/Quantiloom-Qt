# ============================================================================
# QuantiloomGUI - Qt6 GUI for Quantiloom Spectral Renderer
# ============================================================================

cmake_minimum_required(VERSION 3.21)

project(QuantiloomQt
    VERSION 0.9.1
    DESCRIPTION "Qt6 GUI for libQuantiloom"
    LANGUAGES CXX
)

if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
endif()

# ============================================================================
# C++20 Standard
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt6 specific settings
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Export compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Build Type
# ============================================================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ============================================================================
# Find Qt6
# ============================================================================
# GuiPrivate is the QRhi API (<rhi/qrhi.h>) the viewport presents through, and
# ShaderTools is qsb, which bakes its shaders. OpenGL and OpenGLWidgets are
# deliberately not linked -- the viewport renders through QRhi on Vulkan --
# but Qt6Charts loads Qt6OpenGLWidgets and Qt6OpenGL at run time, so the
# Windows deploy step below still copies those two DLLs beside the exe.
#
# QRhi is a limited-compatibility API: it may change between Qt minor
# releases, which is what Qt's private-module warning says on every configure.
# The answer is the pin below, so the warning is acknowledged here once.
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Gui
    GuiPrivate
    Charts
    ShaderTools
    LinguistTools
)

# The viewport imports its own GPU device into QRhi and records SDK work
# between beginExternal() and endExternal(); written against 6.10, widened to
# 6.12 after checking src/viewport/AGENTS.md against the 6.12.0 headers
# (QtGui.framework/Headers/6.12.0/QtGui/rhi) -- present and unchanged there:
#   Vulkan: QRhiVulkanNativeHandles {physDev, dev, gfxQueueFamilyIdx,
#     gfxQueueIdx}, QRhiVulkanCommandBufferNativeHandles::commandBuffer,
#     QRhiVulkanInitParams::preferredExtensionsForImportedDevice() and
#     preferredInstanceExtensions().
#   Metal: QRhiMetalNativeHandles {dev, cmdQueue} (host-created device and
#     queue imported through QRhi::create) and
#     QRhiMetalCommandBufferNativeHandles {commandBuffer, encoder}.
#   Common: QRhiCommandBuffer::beginExternal()/endExternal()/nativeHandles(),
#     QRhiTexture::NativeTexture {quint64 object; int layout}, setNativeLayout(),
#     QRhiTexture::UsedWithLoadStore, QRhi::clipSpaceCorrMatrix(),
#     QRhi::FramesInFlight, QSurface::MetalSurface.
# Not re-read: qrhivulkan.cpp / qrhimetal.mm (6.12 ships headers only), so the
# barrier behaviour in src/viewport/AGENTS.md is carried over from 6.10.
# Check a different minor the same way before widening this.
if(Qt6_VERSION VERSION_LESS 6.10 OR Qt6_VERSION VERSION_GREATER_EQUAL 6.13)
    message(FATAL_ERROR
        "Qt ${Qt6_VERSION} found; the viewport's QRhi code is pinned to Qt 6.10 - 6.12.")
endif()

message(STATUS "Qt6 version: ${Qt6_VERSION}")
message(STATUS "Qt6 path: ${Qt6_DIR}")

# ============================================================================
# Find Vulkan
# ============================================================================
# The viewport presents through QRhi; on Apple the backend is Metal and there
# is no Vulkan SDK to find, so require it everywhere except Apple.
if(NOT APPLE)
    find_package(Vulkan REQUIRED)
    message(STATUS "Vulkan SDK: ${Vulkan_VERSION}")
endif()

# ============================================================================
# libQuantiloom SDK Integration
# ============================================================================
# Uses prebuilt libQuantiloom from SDK directory.
# SDK path auto-detected based on platform (Windows/Linux/macOS).
# ============================================================================

set(QUANTILOOM_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../Quantiloom-SDK"
    CACHE PATH "Path to Quantiloom SDK root directory")

# Auto-detect platform-specific SDK path
if(WIN32)
    set(QUANTILOOM_SDK_DIR "${QUANTILOOM_SDK_ROOT}/windows_amd64")
elseif(APPLE)
    set(QUANTILOOM_SDK_DIR "${QUANTILOOM_SDK_ROOT}/macos_arm64")
elseif(UNIX)
    set(QUANTILOOM_SDK_DIR "${QUANTILOOM_SDK_ROOT}/linux_amd64")
else()
    message(FATAL_ERROR "Unsupported platform. Windows, Linux and macOS are supported.")
endif()

if(NOT EXISTS "${QUANTILOOM_SDK_DIR}/lib/cmake/Quantiloom/QuantiloomConfig.cmake")
    message(FATAL_ERROR
        "Quantiloom SDK not found at: ${QUANTILOOM_SDK_DIR}\n"
        "Expected: ${QUANTILOOM_SDK_DIR}/lib/cmake/Quantiloom/QuantiloomConfig.cmake\n"
        "Please set QUANTILOOM_SDK_ROOT to the correct SDK location."
    )
endif()

# Add SDK to CMAKE_PREFIX_PATH for find_package
list(APPEND CMAKE_PREFIX_PATH "${QUANTILOOM_SDK_DIR}")

message(STATUS "Using Quantiloom SDK: ${QUANTILOOM_SDK_DIR}")

# Find prebuilt Quantiloom. VulkanInterop is the component that carries the
# Vulkan-typed builders and device helpers (and finds Vulkan for its headers);
# the base package is backend-neutral.
# The SDK and Studio ship together: SdkGuard checks the exact binary at start-up,
# and the minimum here is the release this tree was last built and reviewed against.
# macOS is Metal-only: MetalInterop replaces VulkanInterop.
if(APPLE)
    find_package(Quantiloom 0.10.0 REQUIRED COMPONENTS MetalInterop)
else()
    find_package(Quantiloom 0.10.0 REQUIRED COMPONENTS VulkanInterop)
endif()
message(STATUS "Found Quantiloom: ${Quantiloom_VERSION}")

# Use namespaced target from find_package
set(QUANTILOOM_LIBRARIES Quantiloom::libQuantiloom)

# ============================================================================
# SDK freshness guard
# ============================================================================
# Quantiloom-dev's build_wsl.sh reinstalls the SDK wholesale (rm -rf, then
# cmake --install), so every SDK file gets a new timestamp whenever the core is
# rebuilt. Two things have to follow from that, and neither did on its own:
#
#   1. This build tree must reconfigure and recompile. CMAKE_CONFIGURE_DEPENDS
#      below forces it -- the package files are rewritten by every install.
#   2. A binary linked against the old SDK must never *run* against the new
#      one. SensorParams gained a field and GenericSensor gained two members
#      while staying source-compatible, so such a pairing is undefined
#      behaviour that produces no error of any kind. We record the library's
#      hash here and check it at startup; see src/SdkGuard.cpp.
#
# Note that a POST_BUILD copy of the DLL is not enough by itself: POST_BUILD
# only runs when QuantiloomQt relinks, and a changed DLL with an unchanged
# import library does not trigger a relink.

get_target_property(QUANTILOOM_SDK_DLL Quantiloom::libQuantiloom IMPORTED_LOCATION_RELEASE)
if(NOT QUANTILOOM_SDK_DLL)
    get_target_property(QUANTILOOM_SDK_DLL Quantiloom::libQuantiloom IMPORTED_LOCATION)
endif()
get_target_property(QUANTILOOM_SDK_IMPLIB Quantiloom::libQuantiloom IMPORTED_IMPLIB_RELEASE)

set(QUANTILOOM_SDK_DLL_NAME "")
set(QUANTILOOM_SDK_DLL_SHA256 "")
if(QUANTILOOM_SDK_DLL AND EXISTS "${QUANTILOOM_SDK_DLL}")
    get_filename_component(QUANTILOOM_SDK_DLL_NAME "${QUANTILOOM_SDK_DLL}" NAME)
    file(SHA256 "${QUANTILOOM_SDK_DLL}" QUANTILOOM_SDK_DLL_SHA256)
    message(STATUS "SDK library stamp: ${QUANTILOOM_SDK_DLL_NAME} ${QUANTILOOM_SDK_DLL_SHA256}")
else()
    message(WARNING
        "Could not read the SDK's Quantiloom library; the startup mismatch "
        "guard will be inert for this build.")
endif()

# Reconfigure whenever the SDK is reinstalled. The package files are enough on
# their own -- the install rewrites all of them -- but the binaries are listed
# too so that a hand-copied DLL is also caught.
# Ninja and Makefiles already track every file find_package read, and listing
# them again is a "defined as an output multiple times" error under Ninja.
set(QUANTILOOM_SDK_WATCHED "")
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    list(APPEND QUANTILOOM_SDK_WATCHED
        "${QUANTILOOM_SDK_DIR}/lib/cmake/Quantiloom/QuantiloomConfig.cmake"
        "${QUANTILOOM_SDK_DIR}/lib/cmake/Quantiloom/QuantiloomTargets.cmake"
        "${QUANTILOOM_SDK_DIR}/lib/cmake/Quantiloom/QuantiloomTargets-release.cmake"
    )
endif()
foreach(_sdk_binary "${QUANTILOOM_SDK_DLL}" "${QUANTILOOM_SDK_IMPLIB}")
    if(_sdk_binary AND EXISTS "${_sdk_binary}")
        list(APPEND QUANTILOOM_SDK_WATCHED "${_sdk_binary}")
    endif()
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${QUANTILOOM_SDK_WATCHED})

# ============================================================================
# Spectral asset drift check
# ============================================================================
set(QUANTILOOM_DEV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../Quantiloom-dev"
    CACHE PATH "Path to the Quantiloom-dev checkout that bakes assets/spectral")
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckSpectralAssets.cmake)
quantiloom_check_spectral_assets("${CMAKE_CURRENT_SOURCE_DIR}/assets/spectral")

set(QUANTILOOM_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SdkStamp.hpp.in"
    "${QUANTILOOM_GENERATED_DIR}/SdkStamp.hpp"
    @ONLY
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AppVersion.hpp.in"
    "${QUANTILOOM_GENERATED_DIR}/AppVersion.hpp"
    @ONLY
)

# ============================================================================
# Sources
# ============================================================================
set(GUI_SOURCES
    src/main.cpp
    src/SdkGuard.cpp
    src/SdkGuard.hpp
    src/MainWindow.cpp
    src/MainWindow.hpp
    src/ThermalNames.hpp
    src/AssetPaths.cpp
    src/AssetPaths.hpp
    # MainWindow's MCP tool catalogue -- its own translation unit because it is
    # long, not because it is a separate class.
    src/McpTools.cpp
    # Shared UI infrastructure
    src/ui/PanelBase.cpp
    src/ui/PanelBase.hpp
    src/ui/BusyOverlay.cpp
    src/ui/BusyOverlay.hpp
    src/ui/ModeCatalog.cpp
    src/ui/ModeCatalog.hpp
    src/ui/HoverReadbackState.hpp
    src/ui/UiStyle.cpp
    src/ui/UiStyle.hpp
    src/ui/CompactSpinBox.hpp
    src/ui/CollapsibleGroupBox.cpp
    src/ui/CollapsibleGroupBox.hpp
    src/ui/SpectrumPlotWidget.cpp
    src/ui/TrajectoryPlotWidget.cpp
    src/ui/SpectrumPlotWidget.hpp
    src/ui/TrajectoryPlotWidget.hpp
    src/ui/ViewportFrame.cpp
    src/ui/ViewportFrame.hpp
    src/ui/WorkspaceManager.cpp
    src/ui/WorkspaceManager.hpp
    src/ui/chrome/TitleBar.cpp
    src/ui/chrome/TitleBar.hpp
    src/ui/chrome/WindowChrome.cpp
    src/ui/chrome/WindowChrome.hpp
    src/ui/chrome/DialogChrome.cpp
    src/ui/chrome/DialogChrome.hpp
    src/ui/theme/Theme.hpp
    src/ui/theme/ThemeManager.cpp
    src/ui/theme/ThemeManager.hpp
    # Runtime language switching
    src/i18n/LanguageManager.cpp
    src/i18n/LanguageManager.hpp
    src/viewport/ViewportWindow.cpp
    src/viewport/ViewportWindow.hpp
    src/viewport/ViewportRenderer.cpp
    src/viewport/ViewportRenderer.hpp
    src/viewport/GpuBackend.hpp
    src/viewport/SdkFrame.cpp
    src/viewport/SdkFrame.hpp
    src/viewport/PixelReadbackConversion.hpp
    src/viewport/ViewportSampleBatchScheduler.hpp
    src/viewport/CameraMatrices.cpp
    src/viewport/CameraMatrices.hpp
    src/viewport/OverlayRenderer.cpp
    src/viewport/OverlayRenderer.hpp
    # Parameter panels
    src/panels/SceneTreePanel.cpp
    src/panels/SceneTreePanel.hpp
    src/panels/MaterialEditorPanel.cpp
    src/panels/MaterialEditorPanel.hpp
    src/panels/LightingPanel.cpp
    src/panels/LightingPanel.hpp
    src/panels/RenderSettingsPanel.cpp
    src/panels/RenderSettingsPanel.hpp
    src/panels/SpectralLibraryPanel.cpp
    src/panels/SpectralLibraryPanel.hpp
    src/panels/SpectralConfigPanel.cpp
    src/panels/SpectralConfigPanel.hpp
    src/panels/DebugVisualizationPanel.cpp
    src/panels/DebugVisualizationPanel.hpp
    src/panels/AtmosphericPanel.cpp
    src/panels/AtmosphericPanel.hpp
    src/panels/SensorPanel.cpp
    src/panels/SensorPanel.hpp
    src/panels/ThermalPanel.cpp
    src/panels/ThermalPanel.hpp
    src/panels/TimelinePanel.cpp
    src/panels/TimelinePanel.hpp
    src/panels/DisplayEnhancementPanel.cpp
    src/panels/DisplayEnhancementPanel.hpp
    src/panels/SpectralMaterialGenPanel.cpp
    src/panels/SpectralMaterialGenPanel.hpp
    src/panels/PropertiesPanel.cpp
    src/panels/PropertiesPanel.hpp
    src/panels/CameraPanel.cpp
    src/panels/ComparisonPanel.cpp
    src/panels/CameraPanel.hpp
    src/panels/ComparisonPanel.hpp
    # Configuration management
    src/config/ConfigManager.cpp
    src/config/ConfigManager.hpp
    # Editing system
    src/editing/SelectionManager.cpp
    src/editing/SelectionManager.hpp
    src/editing/UndoStack.cpp
    src/editing/UndoStack.hpp
    src/editing/Commands.cpp
    src/editing/Commands.hpp
    src/editing/TransformGizmo.cpp
    src/editing/TransformGizmo.hpp
    src/editing/GizmoModel.cpp
    src/editing/GizmoModel.hpp
    # Dialogs
    src/dialogs/HyperspectralExportDialog.cpp
    src/dialogs/HyperspectralExportDialog.hpp
    src/dialogs/FusionExportDialog.cpp
    src/dialogs/FusionExportDialog.hpp
    # What the three offline export paths share: OfflineRenderer init and the
    # camera-product writer.
    src/dialogs/OfflineExport.cpp
    src/dialogs/OfflineExport.hpp
    src/dialogs/SequenceRenderDialog.cpp
    src/dialogs/SequenceRenderDialog.hpp
    src/dialogs/PreferencesDialog.cpp
    src/dialogs/PreferencesDialog.hpp
    src/dialogs/HelpDialog.cpp
    src/dialogs/HelpDialog.hpp
)

# One GpuBackend per platform (src/viewport/GpuBackend.hpp).
if(APPLE)
    list(APPEND GUI_SOURCES src/viewport/GpuBackendMetal.mm)
    set_source_files_properties(src/viewport/GpuBackendMetal.mm
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc" SKIP_PRECOMPILE_HEADERS ON)
else()
    list(APPEND GUI_SOURCES
        src/viewport/GpuBackendVulkan.cpp
        src/viewport/GpuDevice.cpp
        src/viewport/GpuDevice.hpp
        src/viewport/SdkFrameBridge.cpp
        src/viewport/SdkFrameBridge.hpp
    )
endif()

# ============================================================================
# Translations (i18n)
# ============================================================================
set(TS_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/i18n/translations/quantiloom_zh_CN.ts
    ${CMAKE_CURRENT_SOURCE_DIR}/src/i18n/translations/quantiloom_en.ts
)

# Generate .qm files from .ts files
qt6_add_translation(QM_FILES ${TS_FILES})

# ============================================================================
# Resources
# ============================================================================
set(RESOURCE_FILES
    resources/quantiloom.qrc
)

# The Windows version resource -- what Explorer's Details tab and an installer
# read off the binary. Generated rather than checked in so the version has one
# source, `project(... VERSION)` above; @ONLY because the file is full of
# ${...} that belongs to the resource compiler, not to CMake.
if(WIN32)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/QuantiloomQt.rc.in"
        "${QUANTILOOM_GENERATED_DIR}/QuantiloomQt.rc"
        @ONLY
    )
    list(APPEND RESOURCE_FILES "${QUANTILOOM_GENERATED_DIR}/QuantiloomQt.rc")
endif()

# ============================================================================
# Platform Detection (for libQuantiloom headers)
# ============================================================================
if(WIN32)
    set(QUANTILOOM_PLATFORM_WINDOWS 1)
elseif(UNIX AND NOT APPLE)
    set(QUANTILOOM_PLATFORM_LINUX 1)
elseif(APPLE)
    set(QUANTILOOM_PLATFORM_MACOS 1)
endif()

# ============================================================================
# Executable
# ============================================================================
if(APPLE)
    set(QUANTILOOM_BUNDLE_KEYWORD MACOSX_BUNDLE)
endif()
# Note: Remove WIN32 for debugging to see console output
# add_executable(QuantiloomQt WIN32
add_executable(QuantiloomQt ${QUANTILOOM_BUNDLE_KEYWORD}
    ${GUI_SOURCES}
    ${QM_FILES}
    ${RESOURCE_FILES}
)

# ============================================================================
# Viewport shaders (QRhi)
# ============================================================================
# Baked by qsb into .qsb packages and embedded as resources, named
# :/shaders/<path under shaders/>.qsb -- :/shaders/overlay/composite.frag.qsb.
# SPIR-V for Vulkan is always generated; MSL 1.2 (qsb's default) is kept for
# the Metal track, GLSL and HLSL are not needed. No Vulkan SDK tool is involved:
# qsb ships with Qt.
qt_add_shaders(QuantiloomQt "viewport_shaders"
    PREFIX "/shaders"
    BASE "shaders"
    NOGLSL
    NOHLSL
    FILES
        shaders/overlay/grid.vert
        shaders/overlay/grid.frag
        shaders/overlay/gizmo.vert
        shaders/overlay/gizmo.frag
        shaders/overlay/composite.vert
        shaders/overlay/composite.frag
)

# Platform definition for libQuantiloom headers
if(QUANTILOOM_PLATFORM_WINDOWS)
    target_compile_definitions(QuantiloomQt PRIVATE QUANTILOOM_PLATFORM_WINDOWS)
elseif(QUANTILOOM_PLATFORM_LINUX)
    target_compile_definitions(QuantiloomQt PRIVATE QUANTILOOM_PLATFORM_LINUX)
elseif(QUANTILOOM_PLATFORM_MACOS)
    target_compile_definitions(QuantiloomQt PRIVATE QUANTILOOM_PLATFORM_MACOS)
endif()

# ============================================================================
# Include Directories
# ============================================================================
target_include_directories(QuantiloomQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    # Generated SdkStamp.hpp (see "SDK freshness guard" above)
    ${QUANTILOOM_GENERATED_DIR}
    # libSpectraForge headers (workaround: SDK target missing INTERFACE_INCLUDE_DIRECTORIES)
    ${QUANTILOOM_SDK_DIR}/include/spectraforge
)

# ============================================================================
# Link Libraries
# ============================================================================
target_link_libraries(QuantiloomQt PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Charts
    ${QUANTILOOM_LIBRARIES}
    Quantiloom::libSpectraForge
)
if(APPLE)
    target_link_libraries(QuantiloomQt PRIVATE
        Quantiloom::MetalInterop
        # The SDK's headers include <glm/glm.hpp>; its package provides the target
        # (the Vulkan SDK supplies glm on the other platforms).
        glm::glm
        "-framework Foundation"
        "-framework Metal"
        "-framework QuartzCore"
    )
else()
    target_link_libraries(QuantiloomQt PRIVATE Quantiloom::VulkanInterop)
endif()

# ============================================================================
# Compiler Flags
# ============================================================================
if(MSVC)
    # UTF-8 source/execution charset
    target_compile_options(QuantiloomQt PRIVATE /utf-8 /W4)
    # Compile the sources in parallel; `cmake --build -j` alone only runs whole
    # projects side by side, and this is one project.
    target_compile_options(QuantiloomQt PRIVATE /MP)
    # Disable specific warnings
    target_compile_options(QuantiloomQt PRIVATE /wd4251)
else()
    target_compile_options(QuantiloomQt PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Precompiled header: the Qt widget headers most panels and dialogs include,
# plus the standard library. Stable third-party headers only -- a project
# header here would rebuild every file whenever it is touched -- and no
# Vulkan: QVulkanInstance defines VK_NO_PROTOTYPES before it includes
# vulkan.h, and a forced include would get there first.
target_precompile_headers(QuantiloomQt PRIVATE
    <algorithm> <cmath> <functional> <memory> <utility> <vector>
    <QApplication> <QCheckBox> <QComboBox> <QCoreApplication> <QDebug>
    <QDir> <QDoubleSpinBox> <QEvent> <QFileDialog> <QFileInfo> <QFormLayout>
    <QGroupBox> <QHBoxLayout> <QLabel> <QLineEdit> <QMessageBox>
    <QPushButton> <QSpinBox> <QString> <QTimer> <QVBoxLayout> <QWidget>
)

# ============================================================================
# Windows-specific: Deploy DLLs
# ============================================================================
if(WIN32)
    # Qt6Charts depends on Qt6OpenGLWidgets and Qt6OpenGL at load time (dumpbin
    # shows the imports even though QuantiloomQt.exe itself does not). Studio
    # never links these -- the viewport renders through QRhi on Vulkan -- but a
    # fresh build tree without the copies below refuses to start the exe. These
    # targets exist only so the copy list can name the real DLLs (debug-suffixed
    # in Debug, same as the other entries); they are a runtime dependency of
    # QtCharts, not something Studio links. Kept inside WIN32: macOS and Linux
    # deploy through macdeployqt / rpath and must not gain this requirement.
    find_package(Qt6 REQUIRED COMPONENTS OpenGL OpenGLWidgets)

    # Copy Qt DLLs to output directory (for running from build dir)
    add_custom_command(TARGET QuantiloomQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Qt6::Core>
            $<TARGET_FILE:Qt6::Widgets>
            $<TARGET_FILE:Qt6::Gui>
            $<TARGET_FILE:Qt6::Charts>
            $<TARGET_FILE:Qt6::OpenGLWidgets>
            $<TARGET_FILE:Qt6::OpenGL>
            $<TARGET_FILE_DIR:QuantiloomQt>
        COMMENT "Copying Qt6 DLLs..."
    )

    # Copy Qt platform plugin (qwindows.dll)
    add_custom_command(TARGET QuantiloomQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:QuantiloomQt>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:QuantiloomQt>/platforms/"
        COMMENT "Copying Qt6 platform plugin..."
    )

    # Copy Qt style plugin (qmodernwindowsstyle.dll)
    #
    # This is where the "windows11" and "windowsvista" style keys live -- they
    # are not built into Qt6Widgets, unlike "Fusion" and "Windows". Without the
    # plugin beside the executable, QStyleFactory::create() returns null for
    # both and ThemeManager quietly falls back to Fusion, saying nothing.
    #
    # No theme asks for either key today: both draw through UxTheme with the
    # host Windows' assets, which made the Windows 7 and Windows 11 themes
    # converge, so both are hand-drawn on Fusion now. The plugin is still
    # deployed because a theme is data -- setting styleKey to "windows11" is a
    # one-line change, and this is the silent trap it would fall into.
    if(TARGET Qt6::QModernWindowsStylePlugin)
        add_custom_command(TARGET QuantiloomQt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:QuantiloomQt>/styles"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Qt6::QModernWindowsStylePlugin>"
                "$<TARGET_FILE_DIR:QuantiloomQt>/styles/"
            COMMENT "Copying Qt6 style plugin..."
        )
    else()
        message(WARNING
            "Qt6::QModernWindowsStylePlugin not found — the Windows 11 and "
            "Windows 7 themes will fall back to Fusion.")
    endif()

    # Copy libQuantiloom DLL from SDK
    add_custom_command(TARGET QuantiloomQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Quantiloom::libQuantiloom>
            $<TARGET_FILE_DIR:QuantiloomQt>
        COMMENT "Copying libQuantiloom DLL from SDK..."
    )

    # Copy the SDK's PDBs next to the DLLs. Without them a crash inside the core
    # gives a stack that stops at the DLL boundary. glob rather than
    # $<TARGET_PDB_FILE:>, which is only defined for targets this project builds;
    # the SDK's are imported. Silently absent on an SDK built without them.
    file(GLOB QUANTILOOM_SDK_PDBS "${QUANTILOOM_SDK_DIR}/bin/*.pdb")
    if(QUANTILOOM_SDK_PDBS)
        add_custom_command(TARGET QuantiloomQt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QUANTILOOM_SDK_PDBS}
                $<TARGET_FILE_DIR:QuantiloomQt>
            COMMENT "Copying SDK debug symbols..."
        )
    endif()

    # Copy libSpectraForge DLL from SDK
    add_custom_command(TARGET QuantiloomQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Quantiloom::libSpectraForge>
            $<TARGET_FILE_DIR:QuantiloomQt>
        COMMENT "Copying libSpectraForge DLL from SDK..."
    )

    # ========================================================================
    # Copy OpenUSD Runtime Dependencies from SDK
    # ========================================================================
    file(GLOB USD_DLLS "${QUANTILOOM_SDK_DIR}/bin/usd_*.dll")
    file(GLOB TBB_DLLS "${QUANTILOOM_SDK_DIR}/bin/tbb*.dll")

    if(USD_DLLS)
        add_custom_command(TARGET QuantiloomQt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${USD_DLLS}
                $<TARGET_FILE_DIR:QuantiloomQt>
            COMMENT "Copying OpenUSD DLLs from SDK..."
        )
    endif()

    if(TBB_DLLS)
        add_custom_command(TARGET QuantiloomQt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${TBB_DLLS}
                $<TARGET_FILE_DIR:QuantiloomQt>
            COMMENT "Copying TBB DLLs from SDK..."
        )
    endif()

    # Copy USD plugin directory
    if(EXISTS "${QUANTILOOM_SDK_DIR}/bin/usd")
        add_custom_command(TARGET QuantiloomQt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${QUANTILOOM_SDK_DIR}/bin/usd"
                "$<TARGET_FILE_DIR:QuantiloomQt>/usd"
            COMMENT "Copying USD plugins from SDK..."
        )
    endif()

    # Copy MaterialX DLLs (optional)
    file(GLOB MATERIALX_DLLS "${QUANTILOOM_SDK_DIR}/bin/MaterialX*.dll")
    if(MATERIALX_DLLS)
        add_custom_command(TARGET QuantiloomQt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${MATERIALX_DLLS}
                $<TARGET_FILE_DIR:QuantiloomQt>
            COMMENT "Copying MaterialX DLLs from SDK..."
        )
    endif()
endif()

# ============================================================================
# Copy translation files to output directory (all platforms)
# ============================================================================
add_custom_command(TARGET QuantiloomQt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${QM_FILES}
        $<TARGET_FILE_DIR:QuantiloomQt>
    COMMENT "Copying translation files..."
)

# Qt's own catalogue, for the strings Qt supplies rather than we do: the
# Save/Discard/Cancel buttons of a QMessageBox, the file dialog's furniture,
# the standard shortcuts' names. Without these the application was translated
# and its dialog buttons were not.
get_target_property(QUANTILOOM_QMAKE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(QUANTILOOM_QT_BIN "${QUANTILOOM_QMAKE}" DIRECTORY)
get_filename_component(QUANTILOOM_QT_PREFIX "${QUANTILOOM_QT_BIN}" DIRECTORY)
set(QUANTILOOM_QT_TRANSLATIONS "${QUANTILOOM_QT_PREFIX}/translations")

set(QT_BASE_QM_FILES "")
foreach(_locale en zh_CN)
    set(_qm "${QUANTILOOM_QT_TRANSLATIONS}/qtbase_${_locale}.qm")
    if(EXISTS "${_qm}")
        list(APPEND QT_BASE_QM_FILES "${_qm}")
    endif()
endforeach()

if(QT_BASE_QM_FILES)
    add_custom_command(TARGET QuantiloomQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${QT_BASE_QM_FILES}
            $<TARGET_FILE_DIR:QuantiloomQt>
        COMMENT "Copying Qt's own translation catalogues..."
    )
else()
    message(WARNING
        "Qt translations not found under ${QUANTILOOM_QT_TRANSLATIONS}; "
        "standard dialog buttons will stay in English.")
endif()

# ============================================================================
# Bundled assets
# ============================================================================
# The shell looks up assets/spectral and assets/luts beside the executable
# first (MainWindow::spectralDatabasePath, resolveBundledIlluminant,
# SpectralLibraryPanel::assetsDir), so the copies are deployed beside it.
# A macOS .app keeps data in Contents/Resources (AssetPaths::bundled() looks there).
if(APPLE)
    set(QUANTILOOM_DATA_DIR "$<TARGET_BUNDLE_CONTENT_DIR:QuantiloomQt>/Resources")
else()
    set(QUANTILOOM_DATA_DIR "$<TARGET_FILE_DIR:QuantiloomQt>")
endif()
add_custom_target(QuantiloomQtAssets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/spectral" "${QUANTILOOM_DATA_DIR}/assets/spectral"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/luts" "${QUANTILOOM_DATA_DIR}/assets/luts"
)
install(DIRECTORY assets/spectral assets/luts DESTINATION assets)

add_dependencies(QuantiloomQt QuantiloomQtAssets)

# The SDK's runtime resources (SPIR-V shaders with their SHADERSET stamp, the
# NN atmosphere pack, the BRDF LUT) live under share/quantiloom in the SDK.
# main() pins quantiloom::SetResourceRoot() to the copy beside the executable.
# A target rather than POST_BUILD, so a refreshed SDK is picked up by a build
# that does not relink; copy_directory_if_different keeps the no-op cheap.
add_custom_target(QuantiloomSdkResources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${QUANTILOOM_SDK_DIR}/share/quantiloom"
        "${QUANTILOOM_DATA_DIR}/share/quantiloom"
    COMMENT "Copying the SDK's runtime resources..."
)
add_dependencies(QuantiloomQt QuantiloomSdkResources)

# ============================================================================
# macOS bundle
# ============================================================================
# QuantiloomQt.app/Contents/Frameworks carries the SDK's dylibs exactly as
# installed (cp -RPp, no install_name_tool, no codesign): SdkGuard compares
# their SHA-256 with the stamp recorded above, and either would change it.
# Qt is found through the build's rpath on the Qt prefix; run macdeployqt on
# the bundle to make it self-contained.
if(APPLE)
    file(GLOB QUANTILOOM_SDK_DYLIBS "${QUANTILOOM_SDK_DIR}/lib/*.dylib")
    set_target_properties(QuantiloomQt PROPERTIES
        BUILD_WITH_INSTALL_RPATH ON
        INSTALL_RPATH "@executable_path/../Frameworks;${QUANTILOOM_QT_PREFIX}/lib"
        MACOSX_BUNDLE_BUNDLE_NAME "Quantiloom Studio"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.blitzcolo.quantiloom.studio"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )
    add_custom_target(QuantiloomSdkFrameworks ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_CONTENT_DIR:QuantiloomQt>/Frameworks"
        COMMAND cp -RPp ${QUANTILOOM_SDK_DYLIBS}
            "$<TARGET_BUNDLE_CONTENT_DIR:QuantiloomQt>/Frameworks/"
        COMMENT "Copying the SDK's dylibs into the bundle..."
        VERBATIM
    )
    add_dependencies(QuantiloomQt QuantiloomSdkFrameworks)
    # An SDK built with OpenUSD carries its plugin tree in lib/usd; the SDK's
    # UsdLoader looks for it beside libQuantiloom, which here is Frameworks/.
    if(EXISTS "${QUANTILOOM_SDK_DIR}/lib/usd")
        add_custom_command(TARGET QuantiloomSdkFrameworks POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                "${QUANTILOOM_SDK_DIR}/lib/usd"
                "$<TARGET_BUNDLE_CONTENT_DIR:QuantiloomQt>/Frameworks/usd"
            COMMENT "Copying the SDK's OpenUSD plugins into the bundle..."
            VERBATIM
        )
    endif()
endif()

# ============================================================================
# Summary
# ============================================================================
message(STATUS "========================================")
message(STATUS "  QuantiloomQt Configuration")
message(STATUS "========================================")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  Qt6:          ${Qt6_VERSION}")
if(Vulkan_FOUND)
    message(STATUS "  Vulkan:       ${Vulkan_VERSION}")
else()
    message(STATUS "  Vulkan:       not required (Apple)")
endif()
message(STATUS "  Quantiloom:   SDK (${QUANTILOOM_SDK_DIR})")
message(STATUS "========================================")
