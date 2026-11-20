/**
 * @file main.cpp
 * @brief Entry point for QuantiloomGUI application
 *
 * Initializes the Qt6 application and its GPU backend and launches main window.
 *
 * @author blitzcolo
 */

#include "AppVersion.hpp"  // generated; see cmake/AppVersion.hpp.in
#include "MainWindow.hpp"
#include "SdkGuard.hpp"
#include "i18n/LanguageManager.hpp"
#include "ui/theme/ThemeManager.hpp"
#include "ui/chrome/DialogChrome.hpp"
#include "viewport/GpuBackend.hpp"
#include "AssetPaths.hpp"

#include <QApplication>
#include <QMessageBox>

#include <core/Log.hpp>  // libQuantiloom logging
#include <core/Resources.hpp>

namespace {

/// Shuts libQuantiloom's logging down when main() returns, which is after the
/// window, the viewport and the SDK objects they own have been destroyed:
/// their teardown messages are the half of the log that explains a crash on
/// exit, and a Shutdown() placed before the return would discard them.
struct LogShutdownGuard {
    ~LogShutdownGuard() { quantiloom::Log::Shutdown(); }
};

}  // namespace

int main(int argc, char* argv[]) {
    // Set log message format with timestamp [HH:mm:ss.zzz]
    qSetMessagePattern("[%{time HH:mm:ss.zzz}] %{message}");

    // Initialize libQuantiloom logging (console only, no log file)
    quantiloom::Log::Init(nullptr, quantiloom::Log::Level::Debug);
    LogShutdownGuard logShutdown;

    // Enable high DPI scaling
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);

    // Set application metadata
    app.setApplicationName("Quantiloom");
    // From project(VERSION) via cmake/AppVersion.hpp.in. This is the only place
    // the version is read; anything that needs to show it asks
    // QCoreApplication::applicationVersion() rather than carrying a second copy.
    app.setApplicationVersion(quantiloom_qt::version::kAppVersion);
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
        return 1;
    }
    if (sdkCheck.stale) {
        QL_LOG_WARN("SDK stale: {}", sdkCheck.message.toStdString());
    }

    // The SDK's shaders and assets: its own share/quantiloom, read in place, or
    // a copy bundled beside the executable (QUANTILOOM_BUNDLE_SDK_RESOURCES).
    // Pinned, so no other copy is tried; the SDK checks the shader-set stamp
    // and refuses a stale one.
    quantiloom::SetResourceRoot(assetpaths::sdkResourceRoot().toStdString());

    // The Vulkan backend's QVulkanInstance; the Metal backend has no process-wide state.
    viewport::HostGraphics hostGraphics;
    if (!hostGraphics.create()) {
        return 1;
    }

    // Create and show main window
    MainWindow mainWindow(hostGraphics.vulkanInstance());
    mainWindow.show();

    // A path on the command line opens like File ▸ Open. The window edits a
    // document now, and a document-shaped application is expected to accept
    // one this way.
    //
    // --mcp, or --mcp=PORT, starts the server the Tools menu also starts, so a
    // session an agent is meant to drive comes up already serving instead of
    // waiting for someone to tick a menu entry. The menu remains the way to
    // start and stop it by hand; this only chooses the state the window opens
    // in, and the port it opens on.
    const QStringList arguments = QApplication::arguments();
    for (int i = 1; i < arguments.size(); ++i) {
        const QString& argument = arguments.at(i);
        if (argument == QLatin1String("--mcp")) {
            mainWindow.startMcpServerFromCommandLine(0);
        } else if (argument.startsWith(QLatin1String("--mcp="))) {
            // 0 means "no port given" to the window, so a value that does not
            // parse must not reach it as 0: it would quietly start on the
            // stored port instead of the one typed.
            bool isPort = false;
            const uint port = argument.mid(6).toUInt(&isPort);
            if (!isPort || port < 1 || port > 65535) {
                QL_LOG_ERROR("{}: not a port number from 1 to 65535; the MCP server was not started",
                             argument.toStdString());
                QMessageBox::warning(
                    &mainWindow, QObject::tr("MCP Server"),
                    QObject::tr("%1 is not a port number from 1 to 65535. "
                                "The MCP server was not started.")
                        .arg(argument));
                continue;
            }
            mainWindow.startMcpServerFromCommandLine(static_cast<quint16>(port));
        } else if (!argument.startsWith(QLatin1Char('-'))) {
            mainWindow.openFromCommandLine(argument);
        }
    }

    return app.exec();
}
