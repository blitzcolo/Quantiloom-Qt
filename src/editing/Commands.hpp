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
    MultiTransform = 4,
    PasteNodes = 5,
    RemoveNodes = 6,
    // The settings below reached the renderer without ever entering the
    // history: changing the sun, the atmosphere or the spectral mode could
    // not be undone, and Ctrl+Z silently skipped past them to whatever
    // transform came before.
    ModifyAtmosphere = 7,
    ModifySensor = 8,
    ModifySpectralMode = 9,
    ModifyWavelength = 10,
    ModifyEnvironmentMap = 11,
};

/**
 * @class SettingCommand
 * @brief A whole-value snapshot of one setting, applied through its dispatcher
 *
 * Every setting in this shell already has a single idempotent `apply*`
 * function that writes both the renderer and the widgets (the "one dispatcher
 * per setting" rule). That makes undo for all of them the same command:
 * remember the value before and after, and call the dispatcher with whichever
 * one the stack asks for. Twenty bespoke command classes would have differed
 * only in their type.
 *
 * Merging is by gesture, following TransformNodeCommand: commands stamped with
 * the same gesture id collapse into one entry, so a slider drag is a single
 * undo step whose "before" is the value from before the drag started. A combo
 * or a checkbox never merges, which is correct -- each click is its own step.
 */
template <typename T>
class SettingCommand : public Command {
public:
    SettingCommand(CommandId commandId, int gestureId, const QString& description,
                   T oldValue, T newValue, std::function<void(const T&)> apply)
        : Command(description)
        , m_id(commandId)
        , m_gestureId(gestureId)
        , m_old(std::move(oldValue))
        , m_new(std::move(newValue))
        , m_apply(std::move(apply)) {}

    void execute() override { m_apply(m_new); }
    void undo() override { m_apply(m_old); }

    bool mergeWith(const Command* other) override {
        const auto* typed = dynamic_cast<const SettingCommand<T>*>(other);
        // Unstamped (-1) never merges, matching TransformNodeCommand. The id
        // check is the stack's job, but a different gesture is ours.
        if (!typed || m_gestureId < 0 || typed->m_gestureId != m_gestureId) {
            return false;
        }
        // Keep this command's "before" and adopt the newer "after": the whole
        // gesture becomes one step from where it started to where it ended.
        m_new = typed->m_new;
        return true;
    }

    [[nodiscard]] int id() const override { return static_cast<int>(m_id); }

private:
    CommandId m_id;
    int m_gestureId = -1;
    T m_old;
    T m_new;
    std::function<void(const T&)> m_apply;
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
 * @class PasteNodesCommand
 * @brief Paste (or duplicate) nodes as shallow copies of existing ones
 *
 * The first execute() creates the copies through the SDK's DuplicateNode
 * and remembers their indices; undo tombstones them via removeNode, and a
 * redo brings the same indices back via restoreNode -- so the indices a
 * paste produced stay valid across the whole undo history, which is what
 * lets later commands (a drag of the pasted node) refer to them safely.
 */
class PasteNodesCommand : public Command {
public:
    struct Spec {
        int sourceIndex;      ///< node to instance, valid at execute time
        QString name;         ///< unique name for the copy
        glm::mat4 transform;  ///< world transform for the copy
    };

    PasteNodesCommand(QuantiloomVulkanWindow* window,
                      const std::vector<Spec>& specs,
                      const QString& description = QString());

    void execute() override;
    void undo() override;
    [[nodiscard]] int id() const override { return static_cast<int>(CommandId::PasteNodes); }

    /// Node indices the first execute() created, for selecting the copies
    [[nodiscard]] const QVector<int>& createdIndices() const { return m_created; }

private:
    QuantiloomVulkanWindow* m_window;
    std::vector<Spec> m_specs;
    QVector<int> m_created;
};

/**
 * @class RemoveNodesCommand
 * @brief Delete nodes (tombstone); undo restores the same indices
 */
class RemoveNodesCommand : public Command {
public:
    RemoveNodesCommand(QuantiloomVulkanWindow* window,
                       const QVector<int>& nodeIndices,
                       const QString& description = QString());

    void execute() override;
    void undo() override;
    [[nodiscard]] int id() const override { return static_cast<int>(CommandId::RemoveNodes); }

private:
    QuantiloomVulkanWindow* m_window;
    QVector<int> m_indices;
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
