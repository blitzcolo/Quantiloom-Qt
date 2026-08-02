/**
 * @file SceneTreePanel.cpp
 * @brief Scene hierarchy tree view implementation
 */

#include "SceneTreePanel.hpp"

#include "../ui/UiStyle.hpp"

#include <QTreeWidget>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QBrush>
#include <QColor>
#include <QGroupBox>
#include <QLabel>

// SDK headers
#include <scene/Scene.hpp>
#include <scene/Material.hpp>

SceneTreePanel::SceneTreePanel(QWidget* parent)
    : PanelBase(parent)
{
    setObjectName(panelId());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Filter first: a scene with a thousand nodes is a scroll, not a list.
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setClearButtonEnabled(true);
    bindText([this] { m_filterEdit->setPlaceholderText(tr("Filter by name...")); });
    connect(m_filterEdit, &QLineEdit::textChanged, this, &SceneTreePanel::applyFilter);
    layout->addWidget(m_filterEdit);

    m_tree = new QTreeWidget();
    m_tree->header()->setStretchLastSection(true);
    m_tree->setAlternatingRowColors(true);
    // The shell has supported multi-selection since Select All and Invert
    // arrived; the tree was still single-select, so it could neither show a
    // multi-selection nor make one.
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    bindText([this] {
        m_tree->setHeaderLabels({tr("Name"), tr("Type")});
    });

    layout->addWidget(m_tree, 1);  // stretch factor 1

    // Empty state. The tree used to return early with no scene, leaving a
    // blank rectangle that gave no hint whether anything was wrong.
    m_emptyLabel = new QLabel(this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    bindStyle([this] { uistyle::applyHintStyle(m_emptyLabel); });
    bindText([this] { m_emptyLabel->setText(tr("Open a scene to see its contents.")); });
    layout->addWidget(m_emptyLabel);

    // The paragraph of grey 11px text that used to live here -- the only place
    // the key bindings were written down -- has moved to Help ▸ Keyboard
    // Shortcuts, where it is generated from the bindings that are actually
    // registered instead of retyped and left to drift.

    // The selection highlight paints palette colours onto items, so it has to
    // be laid down again whenever the theme changes.
    bindStyle([this] { applySelectionHighlight(); });

    connect(m_tree, &QTreeWidget::itemClicked, this, &SceneTreePanel::onItemClicked);
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &SceneTreePanel::onTreeSelectionChanged);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) {
                // The panel does not know what a scene operation is; the shell
                // owns those actions and builds the menu.
                emit contextMenuRequested(m_tree->viewport()->mapToGlobal(pos));
            });

    populateTree();
}

QString SceneTreePanel::panelTitle() const {
    return tr("Scene");
}

void SceneTreePanel::retranslateUi() {
    PanelBase::retranslateUi();
    populateTree();   // node and material headings are translated too
}

void SceneTreePanel::setScene(const quantiloom::Scene* scene) {
    m_scene = scene;
    populateTree();
}

void SceneTreePanel::refresh() {
    populateTree();
}

void SceneTreePanel::populateTree() {
    m_tree->clear();

    m_emptyLabel->setVisible(m_scene == nullptr);
    m_tree->setVisible(m_scene != nullptr);
    m_filterEdit->setVisible(m_scene != nullptr);

    if (!m_scene) {
        return;
    }

    // Root: Scene name
    auto* sceneRoot = new QTreeWidgetItem(m_tree);
    sceneRoot->setText(0, QString::fromStdString(m_scene->name));
    sceneRoot->setText(1, tr("Scene"));
    sceneRoot->setExpanded(true);

    // Nodes section. Tombstoned nodes (deleted, or a paste that was undone)
    // keep their indices in the scene but leave the tree, and the count
    // shows what actually renders.
    size_t activeNodes = 0;
    for (const auto& node : m_scene->nodes) {
        if (node.active) {
            ++activeNodes;
        }
    }
    auto* nodesRoot = new QTreeWidgetItem(sceneRoot);
    nodesRoot->setText(0, tr("Nodes (%1)").arg(activeNodes));
    nodesRoot->setText(1, tr("Group"));
    // Role tag, not the display text: the heading is translated, so matching
    // on it broke selection highlighting outright (the tag was never set) and
    // would have broken again in Chinese.
    nodesRoot->setData(0, Qt::UserRole + 1, QStringLiteral("group:nodes"));
    nodesRoot->setExpanded(true);

    for (size_t i = 0; i < m_scene->nodes.size(); ++i) {
        const auto& node = m_scene->nodes[i];
        if (!node.active) {
            continue;
        }
        auto* nodeItem = new QTreeWidgetItem(nodesRoot);

        QString nodeName = tr("Node %1").arg(i);
        if (node.meshIndex < m_scene->meshes.size()) {
            const auto& mesh = m_scene->meshes[node.meshIndex];
            if (!mesh.name.empty()) {
                nodeName = QString::fromStdString(mesh.name);
            }
        }

        nodeItem->setText(0, nodeName);
        nodeItem->setText(1, tr("Node"));
        nodeItem->setData(0, Qt::UserRole, static_cast<int>(i));
        nodeItem->setData(0, Qt::UserRole + 1, QStringLiteral("node"));
    }

    // Materials section
    auto* materialsRoot = new QTreeWidgetItem(sceneRoot);
    materialsRoot->setText(0, tr("Materials (%1)").arg(m_scene->materials.size()));
    materialsRoot->setText(1, tr("Group"));
    materialsRoot->setData(0, Qt::UserRole + 1, QStringLiteral("group:materials"));
    materialsRoot->setExpanded(true);

    for (size_t i = 0; i < m_scene->materials.size(); ++i) {
        const auto& mat = m_scene->materials[i];
        auto* matItem = new QTreeWidgetItem(materialsRoot);

        QString matName = mat.name.empty()
            ? tr("Material %1").arg(i)
            : QString::fromStdString(mat.name);

        matItem->setText(0, matName);
        matItem->setText(1, tr("Material"));
        matItem->setData(0, Qt::UserRole, static_cast<int>(i));
        matItem->setData(0, Qt::UserRole + 1, QStringLiteral("material"));
    }

    // Textures section
    auto* texturesRoot = new QTreeWidgetItem(sceneRoot);
    texturesRoot->setText(0, tr("Textures (%1)").arg(m_scene->textures.size()));
    texturesRoot->setText(1, tr("Group"));
    texturesRoot->setData(0, Qt::UserRole + 1, QStringLiteral("group:textures"));

    for (size_t i = 0; i < m_scene->textures.size(); ++i) {
        const auto& tex = m_scene->textures[i];
        auto* texItem = new QTreeWidgetItem(texturesRoot);

        QString texName = tex.name.empty()
            ? tr("Texture %1").arg(i)
            : QString::fromStdString(tex.name);

        texItem->setText(0, texName);
        texItem->setText(1, QStringLiteral("%1×%2").arg(tex.width).arg(tex.height));
    }

    // Statistics
    auto* statsRoot = new QTreeWidgetItem(sceneRoot);
    statsRoot->setText(0, tr("Statistics"));
    statsRoot->setText(1, tr("Info"));

    auto addStat = [&](const QString& name, const QString& value) {
        auto* item = new QTreeWidgetItem(statsRoot);
        item->setText(0, name);
        item->setText(1, value);
    };

    addStat(tr("Meshes"), QString::number(m_scene->meshes.size()));
    addStat(tr("Triangles"), QString::number(m_scene->GetTotalTriangleCount()));
    addStat(tr("Vertices"), QString::number(m_scene->GetTotalVertexCount()));

    m_tree->resizeColumnToContents(0);

    // The tree was just rebuilt from scratch (refresh, or a language switch),
    // so the highlight and the filter have to be applied to the new items.
    applySelectionHighlight();
    applyFilter(m_filter);
}

void SceneTreePanel::onItemClicked(QTreeWidgetItem* item, int /*column*/) {
    QString type = item->data(0, Qt::UserRole + 1).toString();
    int index = item->data(0, Qt::UserRole).toInt();

    if (type == "node") {
        emit nodeSelected(index);
    } else if (type == "material") {
        emit materialSelected(index);
    }
}

void SceneTreePanel::setSelectedNodes(const QSet<int>& nodeIndices) {
    // Everything below selects items, which the selection-changed handler
    // would otherwise read back out as a user action.
    const QSignalBlocker blocker(m_tree);
    m_syncingSelection = true;

    // Clear previous highlight
    clearSelectionHighlight();

    m_highlightedNodes = nodeIndices;

    applySelectionHighlight();
    m_syncingSelection = false;

    // Ensure the first highlighted item is visible
    for (int nodeIndex : nodeIndices) {
        if (QTreeWidgetItem* item = findNodeItem(nodeIndex)) {
            m_tree->scrollToItem(item);
            break;
        }
    }
}

void SceneTreePanel::onTreeSelectionChanged() {
    // Guarded: setSelectedNodes() selects items itself, and without this the
    // shell's answer would come straight back as a fresh user selection.
    if (m_syncingSelection) {
        return;
    }
    QSet<int> nodes;
    const auto selected = m_tree->selectedItems();
    for (const QTreeWidgetItem* item : selected) {
        if (item->data(0, Qt::UserRole + 1).toString() == QLatin1String("node")) {
            nodes.insert(item->data(0, Qt::UserRole).toInt());
        }
    }
    // Only for node selections. Clicking a material or a heading is not a way
    // of clearing the scene selection.
    if (!nodes.isEmpty()) {
        emit nodesSelected(nodes);
    }
}

void SceneTreePanel::applyFilter(const QString& text) {
    m_filter = text.trimmed();
    // Applied to the live items rather than by rebuilding: rebuilding would
    // drop the selection on every keystroke.
    QTreeWidgetItem* sceneRoot = m_tree->topLevelItem(0);
    if (!sceneRoot) {
        return;
    }
    for (int i = 0; i < sceneRoot->childCount(); ++i) {
        QTreeWidgetItem* group = sceneRoot->child(i);
        int visibleChildren = 0;
        for (int j = 0; j < group->childCount(); ++j) {
            QTreeWidgetItem* child = group->child(j);
            const bool matches =
                m_filter.isEmpty() ||
                child->text(0).contains(m_filter, Qt::CaseInsensitive);
            child->setHidden(!matches);
            visibleChildren += matches ? 1 : 0;
        }
        // A group with nothing left in it is noise, but a group that never had
        // children (Statistics) is not something the filter should hide.
        group->setHidden(group->childCount() > 0 && visibleChildren == 0 &&
                         !m_filter.isEmpty());
        if (!m_filter.isEmpty() && visibleChildren > 0) {
            group->setExpanded(true);
        }
    }
}

void SceneTreePanel::applySelectionHighlight() {
    // Colours come from the palette so the highlight follows the nine themes;
    // this runs again on every theme switch, and is idempotent because it
    // assigns absolute palette values rather than deriving from current ones.
    const QColor bg = palette().color(QPalette::Highlight);
    const QColor fg = palette().color(QPalette::HighlightedText);

    for (int nodeIndex : m_highlightedNodes) {
        QTreeWidgetItem* item = findNodeItem(nodeIndex);
        if (item) {
            item->setBackground(0, bg);
            item->setBackground(1, bg);
            item->setForeground(0, fg);
            item->setForeground(1, fg);
            item->setSelected(true);
        }
    }
}

void SceneTreePanel::clearSelectionHighlight() {
    // Remove highlight from previously highlighted nodes
    for (int nodeIndex : m_highlightedNodes) {
        QTreeWidgetItem* item = findNodeItem(nodeIndex);
        if (item) {
            item->setBackground(0, QBrush());  // Clear background
            item->setBackground(1, QBrush());
            item->setForeground(0, QBrush());  // Reset to default
            item->setForeground(1, QBrush());
            item->setSelected(false);
        }
    }
    m_highlightedNodes.clear();
}

QTreeWidgetItem* SceneTreePanel::findNodeItem(int nodeIndex) {
    // Find the Nodes group item
    QTreeWidgetItem* sceneRoot = m_tree->topLevelItem(0);
    if (!sceneRoot) return nullptr;

    // First child of scene root should be Nodes group
    for (int i = 0; i < sceneRoot->childCount(); ++i) {
        QTreeWidgetItem* groupItem = sceneRoot->child(i);
        if (groupItem->data(0, Qt::UserRole + 1).toString() == QLatin1String("group:nodes")) {
            // Search in the nodes group
            for (int j = 0; j < groupItem->childCount(); ++j) {
                QTreeWidgetItem* nodeItem = groupItem->child(j);
                if (nodeItem->data(0, Qt::UserRole).toInt() == nodeIndex &&
                    nodeItem->data(0, Qt::UserRole + 1).toString() == "node") {
                    return nodeItem;
                }
            }
            break;
        }
    }
    return nullptr;
}
