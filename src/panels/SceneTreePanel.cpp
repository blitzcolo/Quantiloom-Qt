/**
 * @file SceneTreePanel.cpp
 * @brief Scene hierarchy tree view implementation
 */

#include "SceneTreePanel.hpp"

#include "../ui/UiStyle.hpp"

#include <QTreeWidget>
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

    m_tree = new QTreeWidget();
    m_tree->header()->setStretchLastSection(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    bindText([this] {
        m_tree->setHeaderLabels({tr("Name"), tr("Type")});
    });

    layout->addWidget(m_tree, 1);  // stretch factor 1

    // Empty state. The tree used to return early with no scene, leaving a
    // blank rectangle that gave no hint whether anything was wrong.
    m_emptyLabel = new QLabel(this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    uistyle::applyHintStyle(m_emptyLabel);
    bindText([this] { m_emptyLabel->setText(tr("Open a scene to see its contents.")); });
    layout->addWidget(m_emptyLabel);

    // The paragraph of grey 11px text that used to live here -- the only place
    // the key bindings were written down -- has moved to Help ▸ Keyboard
    // Shortcuts, where it is generated from the bindings that are actually
    // registered instead of retyped and left to drift.

    connect(m_tree, &QTreeWidget::itemClicked, this, &SceneTreePanel::onItemClicked);

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

    if (!m_scene) {
        return;
    }

    // Root: Scene name
    auto* sceneRoot = new QTreeWidgetItem(m_tree);
    sceneRoot->setText(0, QString::fromStdString(m_scene->name));
    sceneRoot->setText(1, tr("Scene"));
    sceneRoot->setExpanded(true);

    // Nodes section
    auto* nodesRoot = new QTreeWidgetItem(sceneRoot);
    nodesRoot->setText(0, tr("Nodes (%1)").arg(m_scene->nodes.size()));
    nodesRoot->setText(1, tr("Group"));
    nodesRoot->setExpanded(true);

    for (size_t i = 0; i < m_scene->nodes.size(); ++i) {
        const auto& node = m_scene->nodes[i];
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
    // Clear previous highlight
    clearSelectionHighlight();

    m_highlightedNodes = nodeIndices;

    // Apply highlight to selected nodes
    for (int nodeIndex : nodeIndices) {
        QTreeWidgetItem* item = findNodeItem(nodeIndex);
        if (item) {
            item->setBackground(0, QColor(74, 144, 217));  // Blue highlight
            item->setBackground(1, QColor(74, 144, 217));
            item->setForeground(0, Qt::white);
            item->setForeground(1, Qt::white);

            // Ensure the item is visible
            m_tree->scrollToItem(item);

            // Select it in the tree as well
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
        if (groupItem->data(0, Qt::UserRole + 1).toString() == "Group" &&
            groupItem->text(0).startsWith(tr("Nodes"))) {
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
