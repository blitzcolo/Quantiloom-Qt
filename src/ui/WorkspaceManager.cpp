/**
 * @file WorkspaceManager.cpp
 */

#include "WorkspaceManager.hpp"

#include <QDockWidget>
#include <QMainWindow>
#include <QSet>
#include <QSettings>
#include <QTabBar>

namespace {
constexpr auto kStateGroup   = "layout";
constexpr auto kVersionKey   = "layout/version";
constexpr auto kCurrentKey   = "layout/workspace";

/// Width a docked column starts at. Wide enough for a form row with a label
/// and a spin box without the label eliding, narrow enough to leave the
/// viewport the bulk of a 1600 px window.
///
/// 330 was measured against a label and a spin box and nothing else; the panels
/// that pair a label with a spin box *and* a unit suffix, or a three-field
/// vector row, were eliding at that width.
constexpr int kSideColumnWidth = 400;
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
        // Two stacks of two rather than four docks sharing the column: four
        // vertical slots left every panel too short to use and the viewport
        // too narrow.
        return {
            {QStringLiteral("spectral"),   Qt::RightDockWidgetArea, false},
            {QStringLiteral("render"),     Qt::RightDockWidgetArea, true},
            {QStringLiteral("atmosphere"), Qt::RightDockWidgetArea, false},
            {QStringLiteral("sensor"),     Qt::RightDockWidgetArea, true},
        };
    }
    if (id == QLatin1String("material")) {
        // The scene and material context the generator is used against. The
        // generator itself is deliberately absent: it is a wide, occasional,
        // offline tool and it opens as a floating window from the Tools menu.
        // Docked along the bottom edge it spent the whole session competing
        // for vertical space with the panels it is meant to be used beside.
        return {
            {QStringLiteral("scene"),      Qt::LeftDockWidgetArea,  false},
            {QStringLiteral("properties"), Qt::RightDockWidgetArea, false},
        };
    }
    if (id == QLatin1String("debug")) {
        return {
            {QStringLiteral("scene"),      Qt::LeftDockWidgetArea,  false},
            {QStringLiteral("debug"),      Qt::RightDockWidgetArea, false},
            {QStringLiteral("properties"), Qt::RightDockWidgetArea, true},
        };  // one stack: the debug workspace is about the image, not the panels
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
    if (it != m_states.constEnd() && !it->isEmpty()) {
        hideDocks();
        if (!m_window->restoreState(*it, kLayoutVersion)) {
            applyPreset(id);
        }
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

void WorkspaceManager::hideDocks() {
    // Hide, deliberately, rather than removeDockWidget().
    //
    // restoreState() names only the docks that were in the layout when the
    // state was captured and leaves every other one alone, so without this a
    // panel placed by a *different* workspace survives into this one and the
    // columns fill up a little more with every switch. But detaching them is
    // the wrong instrument: it takes them out of the layout tree that
    // restoreState then has to rebuild against, and the arrangement that comes
    // back is not the one that was saved. Hiding keeps every dock in the tree,
    // and restoreState shows exactly those its blob mentions.
    //
    // Floating docks are exempt: that is the spectral generator standing open
    // as a tool window, and switching workspace to look something up should not
    // slam it shut.
    for (QDockWidget* dock : std::as_const(m_docks)) {
        if (!dock->isFloating()) {
            dock->hide();
        }
    }
}

void WorkspaceManager::detachDocks(const QSet<QString>& keepPlaced) {
    // A floating dock nobody asked about is left alone: that is the spectral
    // generator standing open as a tool window, and switching workspace to
    // look something up should not slam it shut. One that *is* wanted gets
    // docked back, so it has to be un-floated first.
    for (auto it = m_docks.constBegin(); it != m_docks.constEnd(); ++it) {
        QDockWidget* dock = it.value();
        if (!keepPlaced.contains(it.key()) && dock->isFloating()) {
            continue;
        }
        dock->setFloating(false);
        m_window->removeDockWidget(dock);
    }
}

void WorkspaceManager::applyPreset(const QString& id) {
    const QVector<DockPlacement> placements = preset(id);

    QSet<QString> mentioned;
    for (const DockPlacement& placement : placements) {
        mentioned.insert(placement.panelId);
    }

    // Take every dock out of the layout first; whatever the preset does not
    // mention stays hidden rather than lingering from the previous workspace.
    detachDocks(mentioned);

    QDockWidget* previous = nullptr;
    QList<QDockWidget*> placed;
    QList<QDockWidget*> stackLeaders;
    for (const DockPlacement& placement : placements) {
        QDockWidget* dock = m_docks.value(placement.panelId, nullptr);
        if (!dock) {
            continue;
        }
        if (placement.tabifyWithPrevious && previous) {
            m_window->tabifyDockWidget(previous, dock);
        } else {
            m_window->addDockWidget(placement.area, dock);
            stackLeaders.append(dock);
        }
        dock->show();
        previous = dock;
        placed.append(dock);
    }

    // Give the side columns a usable width instead of whatever the last
    // arrangement left behind, and leave the viewport the larger share.
    // resizeDocks works on the visible layout, so it has to come after the
    // docks are shown.
    if (!stackLeaders.isEmpty()) {
        // Every docked column starts at the same width; the viewport keeps the
        // rest. (The one dock that wanted a width of its own, the spectral
        // generator, is a floating tool window now.)
        const QList<int> widths(stackLeaders.size(), kSideColumnWidth);
        m_window->resizeDocks(stackLeaders, widths, Qt::Horizontal);
    }

    // Each stack should open on the panel the preset listed first, not on
    // whichever tab Qt happened to leave in front.
    for (QDockWidget* dock : std::as_const(stackLeaders)) {
        dock->raise();
    }
}

void WorkspaceManager::resetCurrentToDefault() {
    m_states.remove(m_current);
    m_switching = true;
    applyPreset(m_current);
    m_switching = false;

    // Record the result as this workspace's state right away. Dropping the old
    // entry and leaving it at that meant the reset layout existed only on
    // screen: nothing held it until the next captureCurrent(), so returning
    // from another workspace could bring the pre-reset arrangement back.
    captureCurrent();
}

void WorkspaceManager::activateInitial() {
    m_switching = true;
    const auto it = m_states.constFind(m_current);
    if (it == m_states.constEnd() || it->isEmpty()) {
        applyPreset(m_current);
    } else {
        // The same two steps applyWorkspace() takes. These two paths differing
        // is what made a layout correct on startup and wrong after switching
        // away and back: setupDockWidgets() adds every panel to the layout, so
        // whichever path does not clear them first inherits the lot.
        hideDocks();
        if (!m_window->restoreState(*it, kLayoutVersion)) {
            applyPreset(m_current);
        }
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
    // Clear the group rather than writing over it key by key. m_states is the
    // whole truth about which workspaces have a saved layout, and a workspace
    // that no longer has one -- because it was reset -- would otherwise keep
    // whatever blob was last written for it and come back from the dead on the
    // next launch.
    settings.remove(QString());
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
