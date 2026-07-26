/**
 * @file SelectionManager.hpp
 * @brief Selection state management for scene objects
 *
 * Core data structure: a set of selected node indices.
 * Simple, no special cases, just a container with signals.
 *
 * @author blitzccolo
 */

#pragma once

#include <QObject>
#include <QSet>
#include <glm/glm.hpp>

namespace quantiloom {
class Scene;
}

/**
 * @class SelectionManager
 * @brief Manages selection state for scene nodes
 *
 * Design philosophy:
 * - Single source of truth for selection
 * - Emits signals on change, all UI syncs via signals
 * - Supports single and multi-selection
 */
class SelectionManager : public QObject {
    Q_OBJECT

public:
    explicit SelectionManager(QObject* parent = nullptr);

    // Selection queries
    [[nodiscard]] bool hasSelection() const { return !m_selectedNodes.isEmpty(); }
    [[nodiscard]] int selectionCount() const { return m_selectedNodes.size(); }
    [[nodiscard]] bool isSelected(int nodeIndex) const { return m_selectedNodes.contains(nodeIndex); }
    [[nodiscard]] const QSet<int>& selectedNodes() const { return m_selectedNodes; }

    // Get primary selection (first selected, for single-selection operations)
    [[nodiscard]] int primarySelection() const;

    // Selection modification
    void select(int nodeIndex, bool addToSelection = false);
    void selectMultiple(const QSet<int>& nodeIndices);
    void deselect(int nodeIndex);
    void clearSelection();
    void toggleSelection(int nodeIndex);

    // Compute selection center (for gizmo placement)
    [[nodiscard]] glm::vec3 computeSelectionCenter(const quantiloom::Scene* scene) const;

    // Compute selection bounding box
    void computeSelectionBounds(const quantiloom::Scene* scene,
                                glm::vec3& outMin, glm::vec3& outMax) const;

signals:
    // Emitted when selection changes
    void selectionChanged(const QSet<int>& selectedNodes);

    // Emitted when a single node is selected (for UI convenience)
    void nodeSelected(int nodeIndex);

    // Emitted when selection is cleared
    void selectionCleared();

private:
    QSet<int> m_selectedNodes;
};
