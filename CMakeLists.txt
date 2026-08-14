# ============================================================================
# QuantiloomGUI - Qt6 GUI for Quantiloom Spectral Renderer
# ============================================================================

cmake_minimum_required(VERSION 3.21)

project(QuantiloomQt
    VERSION 0.1.10
    DESCRIPTION "Qt6 GUI for libQuantiloom"
    LANGUAGES CXX
)

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
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Gui
    Charts
    OpenGL
    OpenGLWidgets
    LinguistTools
)

message(STATUS "Qt6 version: ${Qt6_VERSION}")
message(STATUS "Qt6 path: ${Qt6_DIR}")

# ============================================================================
# Find Vulkan
# ============================================================================
find_package(Vulkan REQUIRED)
message(STATUS "Vulkan SDK: ${Vulkan_VERSION}")

# ============================================================================
# libQuantiloom SDK Integration
# ============================================================================
# Uses prebuilt libQuantiloom from SDK directory.
# SDK path auto-detected based on platform (Windows/Linux).
# ============================================================================

set(QUANTILOOM_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../Quantiloom-SDK"
    CACHE PATH "Path to Quantiloom SDK root directory")

# Auto-detect platform-specific SDK path
if(WIN32)
    set(QUANTILOOM_SDK_DIR "${QUANTILOOM_SDK_ROOT}/windows_amd64")
elseif(UNIX AND NOT APPLE)
    set(QUANTILOOM_SDK_DIR "${QUANTILOOM_SDK_ROOT}/linux_amd64")
else()
    message(FATAL_ERROR "Unsupported platform. Only Windows and Linux are supported.")
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

# Find prebuilt Quantiloom (also finds Vulkan and glm as dependencies)
find_package(Quantiloom REQUIRED)
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
set(QUANTILOOM_SDK_WATCHED
    "${QUANTILOOM_SDK_DIR}/lib/cmake/Quantiloom/QuantiloomConfig.cmake"
    "${QUANTILOOM_SDK_DIR}/lib/cmake/Quantiloom/QuantiloomTargets.cmake"
    "${QUANTILOOM_SDK_DIR}/lib/cmake/Quantiloom/QuantiloomTargets-release.cmake"
)
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
    # MainWindow's MCP tool catalogue -- its own translation unit because it is
    # long, not because it is a separate class.
    src/McpTools.cpp
    # Shared UI infrastructure
    src/ui/PanelBase.cpp
    src/ui/PanelBase.hpp
    src/ui/ModeCatalog.cpp
    src/ui/ModeCatalog.hpp
    src/ui/UiStyle.cpp
    src/ui/UiStyle.hpp
    src/ui/CollapsibleGroupBox.cpp
    src/ui/CollapsibleGroupBox.hpp
    src/ui/SpectrumPlotWidget.cpp
    src/ui/SpectrumPlotWidget.hpp
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
    src/vulkan/QuantiloomVulkanWindow.cpp
    src/vulkan/QuantiloomVulkanWindow.hpp
    src/vulkan/QuantiloomVulkanRenderer.cpp
    src/vulkan/QuantiloomVulkanRenderer.hpp
    src/vulkan/CameraMatrices.cpp
    src/vulkan/CameraMatrices.hpp
    src/vulkan/OverlayRenderer.cpp
    src/vulkan/OverlayRenderer.hpp
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
    src/panels/DisplayEnhancementPanel.cpp
    src/panels/DisplayEnhancementPanel.hpp
    src/panels/SpectralMaterialGenPanel.cpp
    src/panels/SpectralMaterialGenPanel.hpp
    src/panels/PropertiesPanel.cpp
    src/panels/PropertiesPanel.hpp
    src/panels/CameraPanel.cpp
    src/panels/CameraPanel.hpp
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
    src/dialogs/PreferencesDialog.cpp
    src/dialogs/PreferencesDialog.hpp
    src/dialogs/HelpDialog.cpp
    src/dialogs/HelpDialog.hpp
)

# ============================================================================
# Overlay shaders (viewport grid / gizmo)
# ============================================================================
# GLSL compiled to SPIR-V at build time and embedded as C arrays, so the
# executable carries them and nothing is loaded from disk at runtime.
find_program(GLSLANG_VALIDATOR glslangValidator
    PATHS "C:/VulkanSDK/1.4.350.0/Bin" "C:/VulkanSDK/1.4.328.1/Bin"
          "$ENV{VULKAN_SDK}/Bin"
)
if(NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found; install the Vulkan SDK")
endif()

set(OVERLAY_SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders/overlay)
set(OVERLAY_SHADER_GEN_DIR ${QUANTILOOM_GENERATED_DIR}/overlay_shaders)
file(MAKE_DIRECTORY ${OVERLAY_SHADER_GEN_DIR})

set(OVERLAY_SHADER_HEADERS "")
# compile_overlay_shader(<source name> <C array name> <output header name>)
function(compile_overlay_shader SRC_NAME VAR_NAME OUT_NAME)
    set(OUT_HEADER ${OVERLAY_SHADER_GEN_DIR}/${OUT_NAME})
    add_custom_command(
        OUTPUT ${OUT_HEADER}
        COMMAND ${GLSLANG_VALIDATOR} -V --vn ${VAR_NAME}
                -o ${OUT_HEADER} ${OVERLAY_SHADER_SRC_DIR}/${SRC_NAME}
        DEPENDS ${OVERLAY_SHADER_SRC_DIR}/${SRC_NAME}
        COMMENT "Compiling overlay shader ${SRC_NAME}"
        VERBATIM
    )
    set(OVERLAY_SHADER_HEADERS ${OVERLAY_SHADER_HEADERS} ${OUT_HEADER} PARENT_SCOPE)
endfunction()

compile_overlay_shader(grid.vert kGridVertSpv grid_vert.h)
compile_overlay_shader(grid.frag kGridFragSpv grid_frag.h)
compile_overlay_shader(gizmo.vert kGizmoVertSpv gizmo_vert.h)
compile_overlay_shader(gizmo.frag kGizmoFragSpv gizmo_frag.h)

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
# Note: Remove WIN32 for debugging to see console output
# add_executable(QuantiloomQt WIN32
add_executable(QuantiloomQt
    ${GUI_SOURCES}
    ${QM_FILES}
    ${RESOURCE_FILES}
    ${OVERLAY_SHADER_HEADERS}
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
    Qt6::Charts
    Vulkan::Vulkan
    ${QUANTILOOM_LIBRARIES}
    Quantiloom::libSpectraForge
)

# ============================================================================
# Compiler Flags
# ============================================================================
if(MSVC)
    # UTF-8 source/execution charset
    target_compile_options(QuantiloomQt PRIVATE /utf-8 /W4)
    # Disable specific warnings
    target_compile_options(QuantiloomQt PRIVATE /wd4251)
else()
    target_compile_options(QuantiloomQt PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ============================================================================
# Windows-specific: Deploy DLLs
# ============================================================================
if(WIN32)
    # Copy Qt DLLs to output directory (for running from build dir)
    add_custom_command(TARGET QuantiloomQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Qt6::Core>
            $<TARGET_FILE:Qt6::Widgets>
            $<TARGET_FILE:Qt6::Gui>
            $<TARGET_FILE:Qt6::Charts>
            $<TARGET_FILE:Qt6::OpenGL>
            $<TARGET_FILE:Qt6::OpenGLWidgets>
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

    # Copy shader files from SDK (required at runtime)
    file(GLOB QUANTILOOM_SHADERS "${QUANTILOOM_SDK_DIR}/bin/*.spv")
    if(QUANTILOOM_SHADERS)
        add_custom_command(TARGET QuantiloomQt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QUANTILOOM_SHADERS}
                $<TARGET_FILE_DIR:QuantiloomQt>
            COMMENT "Copying shader files from SDK..."
        )
    endif()

    # Copy NN atmosphere model pack from SDK (required at runtime when the
    # atmosphere is enabled; copy_directory_if_different skips unchanged files)
    if(EXISTS "${QUANTILOOM_SDK_DIR}/assets/atmos_models")
        add_custom_command(TARGET QuantiloomQt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                "${QUANTILOOM_SDK_DIR}/assets/atmos_models"
                "$<TARGET_FILE_DIR:QuantiloomQt>/assets/atmos_models"
            COMMENT "Copying atmosphere model pack from SDK..."
        )
    endif()

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
# Summary
# ============================================================================
message(STATUS "========================================")
message(STATUS "  QuantiloomQt Configuration")
message(STATUS "========================================")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  Qt6:          ${Qt6_VERSION}")
message(STATUS "  Vulkan:       ${Vulkan_VERSION}")
message(STATUS "  Quantiloom:   SDK (${QUANTILOOM_SDK_DIR})")
message(STATUS "========================================")
