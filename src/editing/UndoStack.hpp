/**
 * @file UndoStack.hpp
 * @brief Command pattern based undo/redo system
 *
 * Core data structure: two stacks (undo and redo).
 * Commands are objects with execute() and undo() methods.
 * No special cases - every operation is a command.
 *
 * @author blitzcolo
 */

#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include <vector>

class UndoStack;

/**
 * @class Command
 * @brief Abstract base class for undoable commands
 *
 * Design:
 * - execute(): Apply the command
 * - undo(): Reverse the command
 * - mergeWith(): Optionally merge with previous command (for drag operations)
 */
class Command {
public:
    explicit Command(const QString& description) : m_description(description) {}
    virtual ~Command() = default;

    // Execute the command (first time or redo)
    virtual void execute() = 0;

    // Undo the command
    virtual void undo() = 0;

    // Try to merge with another command (returns true if merged)
    // Used for merging consecutive similar operations (e.g., drag transform)
    virtual bool mergeWith(const Command* /*other*/) { return false; }

    // Command description for UI
    [[nodiscard]] const QString& description() const { return m_description; }

    // Command ID for merging (commands with same ID can potentially merge)
    [[nodiscard]] virtual int id() const { return -1; }

protected:
    QString m_description;
};

/**
 * @class UndoStack
 * @brief Manages command history for undo/redo operations
 *
 * Features:
 * - Unlimited undo depth (configurable)
 * - Command merging for smooth drag operations
 * - Clean state tracking for save prompts
 */
class UndoStack : public QObject {
    Q_OBJECT

public:
    explicit UndoStack(QObject* parent = nullptr);

    // Execute a new command
    void push(std::unique_ptr<Command> command);

    // Undo/Redo
    void undo();
    void redo();

    // State queries
    [[nodiscard]] bool canUndo() const { return m_undoIndex > 0; }
    [[nodiscard]] bool canRedo() const { return m_undoIndex < static_cast<int>(m_commands.size()); }
    [[nodiscard]] bool isClean() const { return m_undoIndex == m_cleanIndex; }

    // Get descriptions for UI
    [[nodiscard]] QString undoText() const;
    [[nodiscard]] QString redoText() const;

    // Mark current state as clean (after save)
    void setClean();

    // Clear all history
    void clear();

    // Configuration
    void setUndoLimit(int limit);
    [[nodiscard]] int undoLimit() const { return m_undoLimit; }

    // History access
    [[nodiscard]] int count() const { return static_cast<int>(m_commands.size()); }
    [[nodiscard]] const Command* command(int index) const;

signals:
    // Emitted when undo/redo availability changes
    void canUndoChanged(bool canUndo);
    void canRedoChanged(bool canRedo);

    // Emitted when clean state changes
    void cleanChanged(bool clean);

    // Emitted when any command is pushed/undone/redone
    void indexChanged(int index);

private:
    void emitStateChanged();

    std::vector<std::unique_ptr<Command>> m_commands;
    int m_undoIndex = 0;       // Points to next command to undo
    int m_cleanIndex = 0;      // Index at last save
    int m_undoLimit = 100;     // Maximum number of commands to keep
};
