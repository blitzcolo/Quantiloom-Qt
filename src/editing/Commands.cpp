/**
 * @file Commands.cpp
 * @brief Command implementations
 */

#include "Commands.hpp"
#include "SelectionManager.hpp"
#include "../vulkan/QuantiloomVulkanWindow.hpp"

#include <renderer/LightingParams.hpp>
#include <QCoreApplication>

// ============================================================================
// TransformNodeCommand
// ============================================================================

TransformNodeCommand::TransformNodeCommand(QuantiloomVulkanWindow* window,
                                           int nodeIndex,
                                           const glm::mat4& oldTransform,
                                           const glm::mat4& newTransform,
                                           const QString& description)
    : Command(description.isEmpty()
        ? QCoreApplication::translate("Commands", "Transform Node")
        : description)
    , m_window(window)
    , m_nodeIndex(nodeIndex)
    , m_oldTransform(oldTransform)
    , m_newTransform(newTransform)
{
}

void TransformNodeCommand::execute() {
    if (m_window && m_nodeIndex >= 0) {
        m_window->setNodeTransform(m_nodeIndex, m_newTransform);
    }
}

void TransformNodeCommand::undo() {
    if (m_window && m_nodeIndex >= 0) {
        m_window->setNodeTransform(m_nodeIndex, m_oldTransform);
    }
}

bool TransformNodeCommand::mergeWith(const Command* other) {
    auto* otherCmd = dynamic_cast<const TransformNodeCommand*>(other);
    if (!otherCmd || otherCmd->m_nodeIndex != m_nodeIndex) {
        return false;
    }
    // Only within the same drag gesture: without this, two separate drags of
    // the same node collapsed into a single undo step
    if (m_gestureId < 0 || m_gestureId != otherCmd->m_gestureId) {
        return false;
    }
    // Keep our old transform, take their new transform
    m_newTransform = otherCmd->m_newTransform;
    return true;
}

// ============================================================================
// MultiTransformCommand
// ============================================================================

MultiTransformCommand::MultiTransformCommand(QuantiloomVulkanWindow* window,
                                             const std::vector<NodeTransform>& transforms,
                                             const QString& description)
    : Command(description.isEmpty()
        ? QCoreApplication::translate("Commands", "Transform %n Node(s)", nullptr,
            static_cast<int>(transforms.size()))
        : description)
    , m_window(window)
    , m_transforms(transforms)
{
}

void MultiTransformCommand::execute() {
    if (!m_window) return;

    for (const auto& t : m_transforms) {
        m_window->setNodeTransform(t.nodeIndex, t.newTransform);
    }
}

void MultiTransformCommand::undo() {
    if (!m_window) return;

    // Undo in reverse order
    for (auto it = m_transforms.rbegin(); it != m_transforms.rend(); ++it) {
        m_window->setNodeTransform(it->nodeIndex, it->oldTransform);
    }
}

bool MultiTransformCommand::mergeWith(const Command* other) {
    auto* otherCmd = dynamic_cast<const MultiTransformCommand*>(other);
    if (!otherCmd || m_gestureId < 0 || m_gestureId != otherCmd->m_gestureId) {
        return false;
    }
    // Same node set (in push order both come from the same selection walk)
    if (m_transforms.size() != otherCmd->m_transforms.size()) {
        return false;
    }
    for (size_t i = 0; i < m_transforms.size(); ++i) {
        if (m_transforms[i].nodeIndex != otherCmd->m_transforms[i].nodeIndex) {
            return false;
        }
    }
    // Keep our old transforms, take their new ones
    for (size_t i = 0; i < m_transforms.size(); ++i) {
        m_transforms[i].newTransform = otherCmd->m_transforms[i].newTransform;
    }
    return true;
}

// ============================================================================
// PasteNodesCommand
// ============================================================================

PasteNodesCommand::PasteNodesCommand(QuantiloomVulkanWindow* window,
                                     const std::vector<Spec>& specs,
                                     const QString& description)
    : Command(description.isEmpty()
        ? QCoreApplication::translate("Commands", "Paste %n Object(s)", nullptr,
            static_cast<int>(specs.size()))
        : description)
    , m_window(window)
    , m_specs(specs)
{
}

void PasteNodesCommand::execute() {
    if (!m_window) return;

    if (m_created.isEmpty()) {
        // First run: create the copies and remember their indices
        for (const auto& spec : m_specs) {
            const int index = m_window->duplicateNode(spec.sourceIndex, spec.name);
            if (index < 0) {
                continue;  // warned by duplicateNode; the rest still paste
            }
            // The copy inherits the source's transform, which may have moved
            // since the copy was taken; the clipboard's snapshot wins
            m_window->setNodeTransformInteractive(index, spec.transform);
            m_created.append(index);
        }
    } else {
        // Redo: the same indices come back from their tombstones
        for (const int index : m_created) {
            m_window->restoreNode(index);
        }
    }
    m_window->rebuildSceneTopology();
}

void PasteNodesCommand::undo() {
    if (!m_window) return;
    for (const int index : m_created) {
        m_window->removeNode(index);
    }
    m_window->rebuildSceneTopology();
}

// ============================================================================
// RemoveNodesCommand
// ============================================================================

RemoveNodesCommand::RemoveNodesCommand(QuantiloomVulkanWindow* window,
                                       const QVector<int>& nodeIndices,
                                       const QString& description)
    : Command(description.isEmpty()
        ? QCoreApplication::translate("Commands", "Delete %n Object(s)", nullptr,
            nodeIndices.size())
        : description)
    , m_window(window)
    , m_indices(nodeIndices)
{
}

void RemoveNodesCommand::execute() {
    if (!m_window) return;
    for (const int index : m_indices) {
        m_window->removeNode(index);
    }
    m_window->rebuildSceneTopology();
}

void RemoveNodesCommand::undo() {
    if (!m_window) return;
    for (const int index : m_indices) {
        m_window->restoreNode(index);
    }
    m_window->rebuildSceneTopology();
}

// ============================================================================
// ModifyMaterialCommand
// ============================================================================

ModifyMaterialCommand::ModifyMaterialCommand(QuantiloomVulkanWindow* window,
                                             int materialIndex,
                                             const quantiloom::Material& oldMaterial,
                                             const quantiloom::Material& newMaterial,
                                             const QString& description)
    : Command(description.isEmpty()
        ? QCoreApplication::translate("Commands", "Modify Material")
        : description)
    , m_window(window)
    , m_materialIndex(materialIndex)
    , m_oldMaterial(oldMaterial)
    , m_newMaterial(newMaterial)
{
}

void ModifyMaterialCommand::execute() {
    if (m_window && m_materialIndex >= 0) {
        m_window->updateMaterial(m_materialIndex, m_newMaterial);
    }
}

void ModifyMaterialCommand::undo() {
    if (m_window && m_materialIndex >= 0) {
        m_window->updateMaterial(m_materialIndex, m_oldMaterial);
    }
}

bool ModifyMaterialCommand::mergeWith(const Command* other) {
    auto* otherCmd = dynamic_cast<const ModifyMaterialCommand*>(other);
    if (!otherCmd || otherCmd->m_materialIndex != m_materialIndex) {
        return false;
    }
    // Keep our old material, take their new material
    m_newMaterial = otherCmd->m_newMaterial;
    return true;
}

// ============================================================================
// SelectionCommand
// ============================================================================

SelectionCommand::SelectionCommand(SelectionManager* manager,
                                   const QSet<int>& oldSelection,
                                   const QSet<int>& newSelection)
    : Command(QCoreApplication::translate("Commands", "Change Selection"))
    , m_manager(manager)
    , m_oldSelection(oldSelection)
    , m_newSelection(newSelection)
{
}

void SelectionCommand::execute() {
    if (m_manager) {
        m_manager->selectMultiple(m_newSelection);
    }
}

void SelectionCommand::undo() {
    if (m_manager) {
        m_manager->selectMultiple(m_oldSelection);
    }
}

// ============================================================================
// CompositeCommand
// ============================================================================

CompositeCommand::CompositeCommand(const QString& description)
    : Command(description)
{
}

void CompositeCommand::addCommand(std::unique_ptr<Command> command) {
    m_commands.push_back(std::move(command));
}

void CompositeCommand::execute() {
    for (auto& cmd : m_commands) {
        cmd->execute();
    }
}

void CompositeCommand::undo() {
    // Undo in reverse order
    for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
        (*it)->undo();
    }
}

// ============================================================================
// LambdaCommand
// ============================================================================

LambdaCommand::LambdaCommand(const QString& description,
                             Action executeFunc,
                             Action undoFunc)
    : Command(description)
    , m_execute(std::move(executeFunc))
    , m_undo(std::move(undoFunc))
{
}

void LambdaCommand::execute() {
    if (m_execute) {
        m_execute();
    }
}

void LambdaCommand::undo() {
    if (m_undo) {
        m_undo();
    }
}
