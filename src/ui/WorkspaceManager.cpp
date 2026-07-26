/**
 * @file WorkspaceManager.cpp
 */

#include "WorkspaceManager.hpp"

#include <QDockWidget>
#include <QMainWindow>
#include <QSettings>
#include <QTabBar>

namespace {
constexpr auto kStateGroup   = "layout";
constexpr auto kVersionKey   = "layout/version";
constexpr auto kCurrentKey   = "layout/workspace";
} // namespace

QStringList WorkspaceManager::workspaceIds() {
    return {
        QStringLiteral("layout"),
        QStringLiteral("environment"),
        QStringLiteral("material"),
        QStringLiteral("debug"),
    };
}

QString WorkspaceManager::workspaceTitle(const QString& id) {
    if (id == QLatin1String("layout"))      return tr("Layout");
    if (id == QLatin1String("environment")) return tr("Environment && Spectral");
    if (id == QLatin1String("material"))    return tr("Material Prep");
    if (id == QLatin1String("debug"))       return tr("Debug");
    return id;
}

QVector<WorkspaceManager::DockPlacement> WorkspaceManager::preset(const QString& id) {
    if (id == QLatin1String("layout")) {
        // Staging a scene: structure on the left, what-is-selected on the right.
        return {
            {QStringLiteral("scene"),      Qt::LeftDockWidgetArea,  false},
            {QStringLiteral("camera"),     Qt::LeftDockWidgetArea,  false},
            {QStringLiteral("properties"), Qt::RightDockWidgetArea, false},
            {QStringLiteral("lighting"),   Qt::RightDockWidgetArea, false},
        };
    }
    if (id == QLatin1String("environment")) {
        // Deciding what the image means: band, atmosphere, sensor, quality.
        // Sensor stacks onto atmosphere -- both are calibrate-once panels.
        return {
            {QStringLiteral("spectral"),   Qt::RightDockWidgetArea, false},
            {QStringLiteral("render"),     Qt::RightDockWidgetArea, false},
            {QStringLiteral("atmosphere"), Qt::RightDockWidgetArea, false},
            {QStringLiteral("sensor"),     Qt::RightDockWidgetArea, true},
        };
    }
    if (id == QLatin1String("material")) {
        // Offline data preparation: the generator wants width, so it takes the
        // whole bottom edge instead of a 300 px column.
        return {
            {QStringLiteral("scene"),      Qt::LeftDockWidgetArea,   false},
            {QStringLiteral("properties"), Qt::RightDockWidgetArea,  false},
            {QStringLiteral("spectralgen"), Qt::BottomDockWidgetArea, false},
        };
    }
    if (id == QLatin1String("debug")) {
        return {
            {QStringLiteral("scene"),      Qt::LeftDockWidgetArea,  false},
            {QStringLiteral("debug"),      Qt::RightDockWidgetArea, false},
            {QStringLiteral("properties"), Qt::RightDockWidgetArea, true},
        };
    }
    return {};
}

WorkspaceManager::WorkspaceManager(QMainWindow* window, QObject* parent)
    : QObject(parent)
    , m_window(window)
{
    m_tabBar = new QTabBar();
    m_tabBar->setExpanding(false);
    m_tabBar->setDrawBase(false);
    m_tabBar->setFocusPolicy(Qt::NoFocus);
    for (const QString& id : workspaceIds()) {
        const int index = m_tabBar->addTab(workspaceTitle(id));
        m_tabBar->setTabData(index, id);
    }
    m_current = workspaceIds().front();

    connect(m_tabBar, &QTabBar::currentChanged, this, &WorkspaceManager::onTabChanged);
}

void WorkspaceManager::registerDock(const QString& panelId, QDockWidget* dock) {
    m_docks.insert(panelId, dock);
}

QString WorkspaceManager::currentWorkspace() const {
    return m_current;
}

void WorkspaceManager::setCurrentWorkspace(const QString& id) {
    const int count = m_tabBar->count();
    for (int i = 0; i < count; ++i) {
        if (m_tabBar->tabData(i).toString() == id) {
            m_tabBar->setCurrentIndex(i);   // drives onTabChanged
            if (m_current != id) {
                applyWorkspace(id);         // index was already current
            }
            return;
        }
    }
}

void WorkspaceManager::onTabChanged(int index) {
    if (m_switching || index < 0) {
        return;
    }
    const QString id = m_tabBar->tabData(index).toString();
    if (id == m_current) {
        return;
    }
    applyWorkspace(id);
}

void WorkspaceManager::applyWorkspace(const QString& id) {
    captureCurrent();

    m_switching = true;
    m_current = id;

    const auto it = m_states.constFind(id);
    if (it != m_states.constEnd() && !it->isEmpty() &&
        m_window->restoreState(*it, kLayoutVersion)) {
        // restoreState brings back visibility and geometry together.
    } else {
        applyPreset(id);
    }

    m_switching = false;
    emit workspaceChanged(id);
}

void WorkspaceManager::captureCurrent() {
    if (m_current.isEmpty()) {
        return;
    }
    m_states.insert(m_current, m_window->saveState(kLayoutVersion));
}

void WorkspaceManager::applyPreset(const QString& id) {
    // Take every dock out of the layout first; whatever the preset does not
    // mention stays hidden rather than lingering from the previous workspace.
    for (QDockWidget* dock : std::as_const(m_docks)) {
        dock->setFloating(false);
        m_window->removeDockWidget(dock);
    }

    QDockWidget* previous = nullptr;
    QList<QDockWidget*> placed;
    for (const DockPlacement& placement : preset(id)) {
        QDockWidget* dock = m_docks.value(placement.panelId, nullptr);
        if (!dock) {
            continue;
        }
        if (placement.tabifyWithPrevious && previous) {
            m_window->tabifyDockWidget(previous, dock);
        } else {
            m_window->addDockWidget(placement.area, dock);
        }
        dock->show();
        previous = dock;
        placed.append(dock);
    }

    // Give the side columns a usable width instead of whatever the last
    // arrangement left behind. resizeDocks works on the visible layout, so it
    // has to come after the docks are shown.
    if (!placed.isEmpty()) {
        QList<int> widths;
        widths.reserve(placed.size());
        for (QDockWidget* dock : std::as_const(placed)) {
            widths.append(dock->objectName() == QLatin1String("dock_spectralgen") ? 900 : 340);
        }
        m_window->resizeDocks(placed, widths, Qt::Horizontal);
    }

    // The first tab of each stack should be the one the preset listed first.
    for (QDockWidget* dock : std::as_const(placed)) {
        if (m_window->tabifiedDockWidgets(dock).isEmpty()) {
            continue;
        }
        dock->raise();
        break;
    }
}

void WorkspaceManager::resetCurrentToDefault() {
    m_states.remove(m_current);
    m_switching = true;
    applyPreset(m_current);
    m_switching = false;
}

void WorkspaceManager::activateInitial() {
    m_switching = true;
    const auto it = m_states.constFind(m_current);
    if (it == m_states.constEnd() || it->isEmpty() ||
        !m_window->restoreState(*it, kLayoutVersion)) {
        applyPreset(m_current);
    }
    // Keep the tab bar in step with a workspace restored from settings.
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toString() == m_current) {
            m_tabBar->setCurrentIndex(i);
            break;
        }
    }
    m_switching = false;
    emit workspaceChanged(m_current);
}

void WorkspaceManager::save(QSettings& settings) const {
    const_cast<WorkspaceManager*>(this)->captureCurrent();

    settings.setValue(kVersionKey, kLayoutVersion);
    settings.setValue(kCurrentKey, m_current);
    settings.beginGroup(kStateGroup);
    for (auto it = m_states.constBegin(); it != m_states.constEnd(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();
}

void WorkspaceManager::restore(QSettings& settings) {
    const int version = settings.value(kVersionKey, 0).toInt();
    if (version != kLayoutVersion) {
        // Written by a build with a different set of docks. Dropping it is the
        // point of the version number: restoring it would produce a layout
        // missing panels that now exist.
        return;
    }

    settings.beginGroup(kStateGroup);
    for (const QString& id : workspaceIds()) {
        const QByteArray blob = settings.value(id).toByteArray();
        if (!blob.isEmpty()) {
            m_states.insert(id, blob);
        }
    }
    settings.endGroup();

    const QString stored = settings.value(kCurrentKey).toString();
    if (workspaceIds().contains(stored)) {
        m_current = stored;
    }
}

void WorkspaceManager::retranslateUi() {
    for (int i = 0; i < m_tabBar->count(); ++i) {
        m_tabBar->setTabText(i, workspaceTitle(m_tabBar->tabData(i).toString()));
    }
}
