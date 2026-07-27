/**
 * @file main.cpp
 * @brief Entry point for QuantiloomGUI application
 *
 * Initializes Qt6 application with Vulkan support and launches main window.
 *
 * @author blitzcolo
 */

#include "MainWindow.hpp"
#include "SdkGuard.hpp"
#include "i18n/LanguageManager.hpp"
#include "ui/theme/ThemeManager.hpp"
#include "ui/chrome/DialogChrome.hpp"

#include <QApplication>
#include <QVulkanInstance>
#include <QLoggingCategory>
#include <QMessageBox>

#include <core/Log.hpp>  // libQuantiloom logging

int main(int argc, char* argv[]) {
    // Set log message format with timestamp [HH:mm:ss.zzz]
    qSetMessagePattern("[%{time HH:mm:ss.zzz}] %{message}");

    // Initialize libQuantiloom logging (console only, no log file)
    quantiloom::Log::Init(nullptr, quantiloom::Log::Level::Debug);

    // Enable high DPI scaling
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);

    // Set application metadata
    app.setApplicationName("Quantiloom");
    app.setApplicationVersion("0.1.2");
    app.setOrganizationName("blitzcolo");
    app.setOrganizationDomain("github.com/blitzcolo");

    // Load translations. From here on the language can also be changed at
    // runtime through Edit ▸ Preferences; LanguageManager swaps the translator
    // and Qt notifies every widget, so nothing here is startup-only.
    LanguageManager::instance().applyStoredPreference();

    // Same shape, and for the same reason: applied before the window exists so
    // the first paint is already themed rather than flashing the default style.
    // With no stored preference this picks by environment -- Classic on a
    // Windows older than 10, Blender Dark under a dark system scheme, Windows
    // 11 otherwise. Changeable at runtime from View ▸ Theme.
    ThemeManager::instance().applyStoredPreference();

    // Dialogs draw their own caption too, from here on. After the theme, so
    // the first one to open is already in its colours.
    DialogChrome::install();

    // Verify this binary and the Quantiloom library it is about to load came
    // from the same SDK install. Placed after the translators so the message is
    // localised, and before any SDK call so a mismatch cannot act first.
    // Reported through libQuantiloom's logger rather than qCritical/qWarning:
    // Qt's own categories do not reach stderr in this build, so a qWarning here
    // would be invisible on a non-interactive run.
    const SdkCheckResult sdkCheck = checkSdkBinaries();
    if (sdkCheck.mismatch) {
        QL_LOG_CRITICAL("SDK mismatch: {}", sdkCheck.message.toStdString());
        QMessageBox::critical(nullptr,
                              QObject::tr("Quantiloom SDK mismatch"),
                              sdkCheck.message);
        quantiloom::Log::Shutdown();
        return 1;
    }
    if (sdkCheck.stale) {
        QL_LOG_WARN("SDK stale: {}", sdkCheck.message.toStdString());
    }

    // Create Vulkan instance for Qt
    QVulkanInstance vulkanInstance;

    // Set Vulkan API version (1.3 required for ray tracing)
    vulkanInstance.setApiVersion(QVersionNumber(1, 3, 0));

    // Enable validation layers in debug builds
#ifdef QT_DEBUG
    vulkanInstance.setLayers({"VK_LAYER_KHRONOS_validation"});
    QLoggingCategory::setFilterRules("qt.vulkan=true");
#endif

    // Required extensions for ray tracing
    vulkanInstance.setExtensions({
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    });

    if (!vulkanInstance.create()) {
        qFatal("Failed to create Vulkan instance: %d", vulkanInstance.errorCode());
        return 1;
    }

    // Create and show main window
    MainWindow mainWindow(&vulkanInstance);
    mainWindow.show();

    // A path on the command line opens like File ▸ Open. The window edits a
    // document now, and a document-shaped application is expected to accept
    // one this way.
    const QStringList arguments = QApplication::arguments();
    if (arguments.size() > 1) {
        mainWindow.openFromCommandLine(arguments.at(1));
    }

    int result = app.exec();

    // Cleanup libQuantiloom logging
    quantiloom::Log::Shutdown();

    return result;
}
