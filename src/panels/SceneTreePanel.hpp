/**
 * @file SceneTreePanel.hpp
 * @brief Scene hierarchy tree view panel
 */

#pragma once

#include "../ui/PanelBase.hpp"

#include <QSet>

QT_BEGIN_NAMESPACE
class QTreeWidget;
class QTreeWidgetItem;
QT_END_NAMESPACE

namespace quantiloom {
class Scene;
}

/**
 * @class SceneTreePanel
 * @brief Displays scene hierarchy (meshes, nodes, materials)
 */
class SceneTreePanel : public PanelBase {
    Q_OBJECT

public:
    explicit SceneTreePanel(QWidget* parent = nullptr);

    [[nodiscard]] QString panelTitle() const override;
    [[nodiscard]] QString panelId() const override { return QStringLiteral("scene"); }
    void retranslateUi() override;

    void setScene(const quantiloom::Scene* scene);
    void refresh();

    /**
     * @brief Highlight nodes selected via SelectionManager
     * @param nodeIndices Set of node indices to highlight
     */
    void setSelectedNodes(const QSet<int>& nodeIndices);

    /**
     * @brief Clear all visual selection highlights
     */
    void clearSelectionHighlight();

signals:
    void nodeSelected(int nodeIndex);
    void materialSelected(int materialIndex);
    /// A multi-selection made in the tree, for the shell to mirror into the
    /// SelectionManager. Distinct from nodeSelected, which is one click.
    void nodesSelected(const QSet<int>& nodeIndices);
    /// Right-click in the tree. The panel owns no scene operations, so the
    /// shell builds and shows the menu.
    void contextMenuRequested(const QPoint& globalPos);

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onTreeSelectionChanged();
    void applyFilter(const QString& text);

private:
    void populateTree();
    void applySelectionHighlight();
    QTreeWidgetItem* findNodeItem(int nodeIndex);

    QTreeWidget* m_tree = nullptr;
    class QLineEdit* m_filterEdit = nullptr;
    class QLabel* m_emptyLabel = nullptr;
    const quantiloom::Scene* m_scene = nullptr;
    QSet<int> m_highlightedNodes;
    QString m_filter;
    /// True while the shell's selection is being written into the tree, so
    /// the resulting itemSelectionChanged is not mistaken for a user action.
    bool m_syncingSelection = false;
};
