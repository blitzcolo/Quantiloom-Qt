/**
 * @file WorkspaceManager.cpp
 */

#include "WorkspaceManager.hpp"

#include <algorithm>

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
    // Captured from a hand-arranged session rather than designed at a desk, so
    // the reasoning below describes what the arrangement turned out to be.
    if (id == QLatin1String("layout")) {
        // Staging a scene: what exists and what is selected on the left, the
        // two things being aimed -- camera and light -- on the right. The left
        // column is the wider one in both workspaces that carry the properties
        // panel: its rows pair a label with a spin box and a unit, which is
        // the widest form in the application.
        return {
            {QStringLiteral("scene"),      Qt::LeftDockWidgetArea,  false, 495, 513},
            {QStringLiteral("properties"), Qt::LeftDockWidgetArea,  false, 0,   460},
            {QStringLiteral("camera"),     Qt::RightDockWidgetArea, false, 432, 658},
            {QStringLiteral("lighting"),   Qt::RightDockWidgetArea, false, 0,   315},
            // Under the viewport, the width of the window: a transport is read
            // left to right and has nothing to gain from a column.
            {QStringLiteral("timeline"),   Qt::BottomDockWidgetArea, false, 0, 190},
        };
    }
    if (id == QLatin1String("environment")) {
        // Deciding what the image means. Four panels in two columns of two,
        // none tabbed: they are read against each other, and a tab hides half
        // of what the comparison needs.
        return {
            {QStringLiteral("render"),     Qt::LeftDockWidgetArea,  false, 380, 623},
            {QStringLiteral("sensor"),     Qt::LeftDockWidgetArea,  false, 0,   350},
            {QStringLiteral("spectral"),   Qt::RightDockWidgetArea, false, 363, 559},
            {QStringLiteral("atmosphere"), Qt::RightDockWidgetArea, false, 0,   414},
        };
    }
    if (id == QLatin1String("material")) {
        // The spectral generator is docked here, full height on the right and
        // the widest column in any workspace, beside the scene and material
        // context it is used against. It used to
        // be excluded on the grounds that a wide occasional tool belongs in a
        // floating window; given a column of its own rather than a strip along
        // the bottom, it earns the space.
        return {
            {QStringLiteral("scene"),            Qt::LeftDockWidgetArea,  false, 495, 665},
            {QStringLiteral("properties"),       Qt::LeftDockWidgetArea,  false, 0,   308},
            // The measured-material databases belong in the workspace where
            // materials are prepared, beside the generator that makes curves
            // by hand rather than by measurement.
            {QStringLiteral("spectral_library"), Qt::RightDockWidgetArea, false, 474, 620},
            {QStringLiteral("spectralgen"),      Qt::RightDockWidgetArea, false, 474},
        };
    }
    if (id == QLatin1String("debug")) {
        // One panel, and the rest of the window is the image. That is the
        // whole point of this workspace: the debug visualisation is read off
        // the render, so anything else in the frame is competing with the
        // thing being looked at. The properties panel was tabbed in here in
        // the session this was captured from, left over rather than wanted.
        return {
            {QStringLiteral("debug"), Qt::RightDockWidgetArea, false},
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

    restoreOrPreset(id);

    m_switching = false;
    emit workspaceChanged(id);
}

void WorkspaceManager::captureCurrent() {
    if (m_current.isEmpty()) {
        return;
    }
    // Which panels are on screen, recorded beside the blob. saveState() only
    // describes docks that are *in* the layout, so the ones applyPreset()
    // removed leave no trace in it -- and restoreState() answers a dock it
    // finds no entry for by putting it back and showing it. The blob therefore
    // cannot express "not this one"; this list can.
    QStringList visible;
    for (auto vt = m_docks.constBegin(); vt != m_docks.constEnd(); ++vt) {
        if (vt.value()->isVisible() && !vt.value()->isFloating()) {
            visible << vt.key();
        }
    }
    visible.sort();
    m_visible.insert(m_current, visible);

    m_states.insert(m_current, m_window->saveState(kLayoutVersion));
}

void WorkspaceManager::applyVisibility(const QString& id) {
    // Put right what restoreState() cannot: a dock its blob never mentioned
    // comes back visible, so the workspace gains every panel the others use.
    // The recorded list is what was actually on screen when the layout was
    // saved, so a panel the user added by hand survives and one the preset
    // never placed does not come back.
    const auto it = m_visible.constFind(id);
    if (it == m_visible.constEnd()) {
        return;
    }
    for (auto dt = m_docks.constBegin(); dt != m_docks.constEnd(); ++dt) {
        if (!dt.value()->isFloating()) {
            dt.value()->setVisible(it->contains(dt.key()));
        }
    }
}

void WorkspaceManager::restoreOrPreset(const QString& id) {
    // The preset first, always, and then the saved layout on top of it.
    //
    // restoreState() is not authoritative: it restores what it can onto the
    // dock tree that is already there, so the same blob produces different
    // arrangements depending on where the window was before. That is why a
    // workspace could restore correctly on startup -- where the tree is always
    // the one setupDockWidgets() built -- and then come back wrong after
    // switching away and returning, where the tree is whatever the other
    // workspace left. Neither hiding the docks nor detaching them fixed that,
    // because both leave the *structure* they were in.
    //
    // applyPreset() rebuilds the tree from nothing to a known shape, so the
    // blob is always applied to the same starting point for a given workspace.
    // The preset also stands as the fallback when the blob will not load.
    applyPreset(id);

    const auto it = m_states.constFind(id);
    if (it != m_states.constEnd() && !it->isEmpty()) {
        m_window->restoreState(*it, kLayoutVersion);
        applyVisibility(id);
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

    // Width belongs to a *column*, height to a *row*, and conflating the two
    // is what made every side column come out the same width. A column is a
    // dock area, so only the first dock placed in each area carries its width;
    // asking for one on the second row of the same column is asking Qt for two
    // widths at once, and the answer was neither.
    QList<QDockWidget*> columnLeaders;
    QList<int> columnWidths;
    QSet<Qt::DockWidgetArea> areasSized;

    // Heights are per row, and only from rows that name one. A zero in the
    // list makes Qt reject the whole call -- "all sizes need to be larger than
    // 0" -- so a single-row column that has no height to give would otherwise
    // take every other column's heights down with it.
    QList<QDockWidget*> rowLeaders;
    QList<int> rowHeights;
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

            if (!areasSized.contains(placement.area)) {
                areasSized.insert(placement.area);
                columnLeaders.append(dock);
                columnWidths.append(placement.width > 0 ? placement.width
                                                        : kSideColumnWidth);
            }
            if (placement.height > 0) {
                rowLeaders.append(dock);
                rowHeights.append(placement.height);
            }
        }
        dock->show();
        previous = dock;
        placed.append(dock);
    }

    // Give the side columns a usable width instead of whatever the last
    // arrangement left behind, and leave the viewport the larger share.
    // resizeDocks works on the visible layout, so it has to come after the
    // docks are shown.
    if (!columnLeaders.isEmpty()) {
        // Each column gets the width its preset asked for, falling back to
        // kSideColumnWidth; the viewport keeps the rest.
        m_window->resizeDocks(columnLeaders, columnWidths, Qt::Horizontal);

        // And the rows their share of the height. Without this Qt divides a
        // column by size hint, which gives most of it to whichever panel has
        // the longest form -- the properties panel took the left column and
        // left the scene tree a strip.
        if (!rowLeaders.isEmpty()) {
            m_window->resizeDocks(rowLeaders, rowHeights, Qt::Vertical);
        }
    }

    // Each stack should open on the panel the preset listed first, not on
    // whichever tab Qt happened to leave in front.
    for (QDockWidget* dock : std::as_const(stackLeaders)) {
        dock->raise();
    }
}

void WorkspaceManager::resetCurrentToDefault() {
    m_states.remove(m_current);
    m_visible.remove(m_current);
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
    restoreOrPreset(m_current);
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

    // Clear the group before writing anything, not after. m_states is the whole
    // truth about which workspaces have a saved layout, and a workspace that no
    // longer has one -- because it was reset -- would otherwise keep whatever
    // blob was last written for it and come back on the next launch.
    //
    // The order matters more than it looks: kVersionKey and kCurrentKey live
    // *inside* this same group, so clearing it after writing them deleted the
    // version stamp, restore() then read 0, decided the settings came from an
    // incompatible build, and dropped every layout on the floor. Nothing was
    // ever restored across a restart.
    settings.beginGroup(kStateGroup);
    settings.remove(QString());
    settings.endGroup();

    settings.setValue(kVersionKey, kLayoutVersion);
    settings.setValue(kCurrentKey, m_current);
    settings.beginGroup(kStateGroup);
    for (auto it = m_states.constBegin(); it != m_states.constEnd(); ++it) {
        settings.setValue(it.key(), it.value());
        settings.setValue(it.key() + QStringLiteral("_visible"),
                          m_visible.value(it.key()));
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
            m_visible.insert(id, settings.value(id + QStringLiteral("_visible"))
                                     .toStringList());
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
