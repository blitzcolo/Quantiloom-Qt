/**
 * @file main.cpp
 * @brief Entry point for QuantiloomGUI application
 *
 * Initializes Qt6 application with Vulkan support and launches main window.
 *
 * @author wtflmao
 */

#include "MainWindow.hpp"

#include <QApplication>
#include <QVulkanInstance>
#include <QLoggingCategory>
#include <QTranslator>
#include <QLocale>
#include <QSettings>

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
    app.setApplicationVersion("0.0.4");
    app.setOrganizationName("wtflmao");
    app.setOrganizationDomain("github.com/wtflmao");

    // Load translations
    QTranslator translator;

    // Check for saved language preference first
    QSettings settings;
    QString savedLocale = settings.value("language", "").toString();

    bool translationLoaded = false;

    if (!savedLocale.isEmpty()) {
        // Use saved language preference
        const QString baseName = "quantiloom_" + savedLocale;
        if (translator.load(baseName, app.applicationDirPath())) {
            app.installTranslator(&translator);
            qDebug() << "Loaded translation:" << baseName << "(user preference)";
            translationLoaded = true;
        }
    }

    if (!translationLoaded) {
        // Fall back to system locale
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString& locale : uiLanguages) {
            const QString baseName = "quantiloom_" + QLocale(locale).name();
            if (translator.load(baseName, app.applicationDirPath())) {
                app.installTranslator(&translator);
                qDebug() << "Loaded translation:" << baseName << "(system locale)";
                break;
            }
        }
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

    int result = app.exec();

    // Cleanup libQuantiloom logging
    quantiloom::Log::Shutdown();

    return result;
}
