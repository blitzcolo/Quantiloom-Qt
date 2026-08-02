/**
 * @file WorkspaceManager.hpp
 * @brief Mode-based workspaces: one preset dock arrangement per work stage
 *
 * The application's use is naturally staged — lay the scene out, fix the band
 * and the atmosphere, prepare spectral material data, verify what the renderer
 * produced — and each stage wants a different set of panels open. A workspace
 * is a named preset arrangement of the independent docks, with the viewport
 * always in the centre.
 *
 * A workspace is a starting point, not a cage: inside one the user may open,
 * close, float and drag anything, and those adjustments are remembered per
 * workspace. "Reset layout" restores the preset for the current workspace
 * only.
 *
 * Panels themselves are shared objects, so a panel that appears in two
 * workspaces is the same widget with the same state — there is never a second,
 * out-of-sync copy.
 */

#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

QT_BEGIN_NAMESPACE
class QDockWidget;
class QMainWindow;
class QSettings;
class QTabBar;
QT_END_NAMESPACE

class WorkspaceManager : public QObject {
    Q_OBJECT

public:
    /// Bumped whenever the set of docks or the preset arrangements change.
    /// A stored layout carrying a different number is discarded rather than
    /// restored, which is what keeps an upgrade from resurrecting a layout
    /// that refers to docks this build no longer has.
    static constexpr int kLayoutVersion = 6;

    explicit WorkspaceManager(QMainWindow* window, QObject* parent = nullptr);

    /// The tab bar to place at the top of the window. Owned by the caller
    /// once it is added to a container.
    [[nodiscard]] QTabBar* tabBar() const { return m_tabBar; }

    /// Every dock that can take part in a workspace, keyed by its panel id.
    void registerDock(const QString& panelId, QDockWidget* dock);

    [[nodiscard]] QString currentWorkspace() const;
    void setCurrentWorkspace(const QString& id);

    /// Apply the built-in arrangement for the current workspace, discarding
    /// the user's adjustments to it.
    void resetCurrentToDefault();

    /// Persist every workspace's arrangement, including the one on screen.
    void save(QSettings& settings) const;

    /// Restore arrangements written by save(). Layouts from an older
    /// kLayoutVersion are ignored and the presets are used instead.
    void restore(QSettings& settings);

    /// Called after the docks are registered to put the first workspace up.
    void activateInitial();

    void retranslateUi();

    /// Localised title of a workspace, for menu entries.
    [[nodiscard]] static QString workspaceTitle(const QString& id);

    /// Ids in display order.
    [[nodiscard]] static QStringList workspaceIds();

signals:
    void workspaceChanged(const QString& id);

private:
    struct DockPlacement {
        QString panelId;
        Qt::DockWidgetArea area;
        bool tabifyWithPrevious;   ///< stack onto the previously placed dock
        /// Width for the column this dock leads, or 0 for kSideColumnWidth.
        /// Only read from a dock that starts a column; one that tabs onto the
        /// previous shares its width by definition.
        int width = 0;
        /// Height of this dock's row, or 0 to let Qt distribute the column.
        /// Relative rather than absolute -- resizeDocks reads these as
        /// proportions of the space available. Without them Qt divides the
        /// column by size hint, which hands most of it to whichever panel has
        /// the longest form and leaves the others a sliver.
        int height = 0;
    };

    [[nodiscard]] static QVector<DockPlacement> preset(const QString& workspaceId);

    void captureCurrent();
    void applyWorkspace(const QString& id);
    /// Take every dock out of the layout so a placement starts from nothing.
    /// Docks named in @p keepPlaced are un-floated first because they are
    /// about to be docked again; unnamed floating ones are left as they are.
    /// Rebuild the workspace: the preset for a known structure, then the saved
    /// layout on top of it. One path, used by both startup and switching.
    void restoreOrPreset(const QString& id);

    /// Re-hide the docks a restored blob silently brought back. See the
    /// definition: saveState() cannot record an absence.
    void applyVisibility(const QString& id);

    void detachDocks(const QSet<QString>& keepPlaced);
    void applyPreset(const QString& id);
    void onTabChanged(int index);

    QMainWindow* m_window = nullptr;
    QTabBar* m_tabBar = nullptr;
    QMap<QString, QDockWidget*> m_docks;
    QMap<QString, QByteArray> m_states;   ///< workspace id -> saveState() blob
    /// Panels on screen per workspace, kept beside m_states because
    /// saveState() cannot express "this dock should not appear".
    QMap<QString, QStringList> m_visible;
    QString m_current;
    bool m_switching = false;
};
