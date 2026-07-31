/**
 * @file Commands.hpp
 * @brief Concrete command implementations for scene editing
 *
 * Each command stores the minimum state needed for undo/redo.
 * No duplicate data - store only what changed.
 *
 * @author blitzcolo
 */

#pragma once

#include "UndoStack.hpp"
#include <QSet>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <glm/glm.hpp>
#include <functional>

// Forward declarations
class QuantiloomVulkanWindow;
class SelectionManager;

namespace quantiloom {
struct LightingParams;
}

// Command IDs for merging
enum class CommandId {
    TransformNode = 1,
    ModifyMaterial = 2,
    ModifyLighting = 3,
    MultiTransform = 4
};

/**
 * @class TransformNodeCommand
 * @brief Command for changing a node's transform
 *
 * Supports merging for smooth drag operations.
 */
class TransformNodeCommand : public Command {
public:
    TransformNodeCommand(QuantiloomVulkanWindow* window,
                         int nodeIndex,
                         const glm::mat4& oldTransform,
                         const glm::mat4& newTransform,
                         const QString& description = QString());

    void execute() override;
    void undo() override;
    bool mergeWith(const Command* other) override;
    [[nodiscard]] int id() const override { return static_cast<int>(CommandId::TransformNode); }

    /// Commands from the same drag gesture merge into one undo step;
    /// separate drags of the same node stay separate steps. Unstamped (-1)
    /// commands never merge.
    void setMergeGesture(int gestureId) { m_gestureId = gestureId; }

private:
    QuantiloomVulkanWindow* m_window;
    int m_nodeIndex;
    glm::mat4 m_oldTransform;
    glm::mat4 m_newTransform;
    int m_gestureId = -1;
};

/**
 * @class MultiTransformCommand
 * @brief Command for transforming multiple nodes at once
 */
class MultiTransformCommand : public Command {
public:
    struct NodeTransform {
        int nodeIndex;
        glm::mat4 oldTransform;
        glm::mat4 newTransform;
    };

    MultiTransformCommand(QuantiloomVulkanWindow* window,
                          const std::vector<NodeTransform>& transforms,
                          const QString& description = QString());

    void execute() override;
    void undo() override;
    bool mergeWith(const Command* other) override;
    [[nodiscard]] int id() const override { return static_cast<int>(CommandId::MultiTransform); }

    /// Same contract as TransformNodeCommand::setMergeGesture
    void setMergeGesture(int gestureId) { m_gestureId = gestureId; }

private:
    QuantiloomVulkanWindow* m_window;
    std::vector<NodeTransform> m_transforms;
    int m_gestureId = -1;
};

/**
 * @class ModifyMaterialCommand
 * @brief Command for changing material properties
 */
class ModifyMaterialCommand : public Command {
public:
    ModifyMaterialCommand(QuantiloomVulkanWindow* window,
                          int materialIndex,
                          const quantiloom::Material& oldMaterial,
                          const quantiloom::Material& newMaterial,
                          const QString& description = QString());

    void execute() override;
    void undo() override;
    bool mergeWith(const Command* other) override;
    [[nodiscard]] int id() const override { return static_cast<int>(CommandId::ModifyMaterial); }

private:
    QuantiloomVulkanWindow* m_window;
    int m_materialIndex;
    quantiloom::Material m_oldMaterial;
    quantiloom::Material m_newMaterial;
};

/**
 * @class SelectionCommand
 * @brief Command for changing selection (optional, for selection undo)
 */
class SelectionCommand : public Command {
public:
    SelectionCommand(SelectionManager* manager,
                     const QSet<int>& oldSelection,
                     const QSet<int>& newSelection);

    void execute() override;
    void undo() override;

private:
    SelectionManager* m_manager;
    QSet<int> m_oldSelection;
    QSet<int> m_newSelection;
};

/**
 * @class CompositeCommand
 * @brief Groups multiple commands into one undoable action
 */
class CompositeCommand : public Command {
public:
    explicit CompositeCommand(const QString& description);

    void addCommand(std::unique_ptr<Command> command);
    void execute() override;
    void undo() override;

    [[nodiscard]] bool isEmpty() const { return m_commands.empty(); }

private:
    std::vector<std::unique_ptr<Command>> m_commands;
};

/**
 * @class LambdaCommand
 * @brief Generic command using lambda functions
 *
 * Useful for one-off operations without creating a new class.
 */
class LambdaCommand : public Command {
public:
    using Action = std::function<void()>;

    LambdaCommand(const QString& description,
                  Action executeFunc,
                  Action undoFunc);

    void execute() override;
    void undo() override;

private:
    Action m_execute;
    Action m_undo;
};
