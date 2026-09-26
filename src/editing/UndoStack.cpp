/**
 * @file UndoStack.cpp
 * @brief UndoStack implementation
 */

#include "UndoStack.hpp"
#include <QCoreApplication>

UndoStack::UndoStack(QObject* parent)
    : QObject(parent)
{
}

void UndoStack::push(std::unique_ptr<Command> command) {
    if (!command) {
        return;
    }

    // Execute the command
    command->execute();

    // If we're not at the end, remove all commands after current position
    if (m_undoIndex < static_cast<int>(m_commands.size())) {
        m_commands.erase(m_commands.begin() + m_undoIndex, m_commands.end());
        // Clean index becomes invalid if we're past it
        if (m_cleanIndex > m_undoIndex) {
            m_cleanIndex = -1;  // Clean state is unreachable
        }
    }

    // Try to merge with previous command
    if (!m_commands.empty() && m_undoIndex > 0 && m_undoIndex != m_cleanIndex) {
        Command* lastCmd = m_commands.back().get();
        if (lastCmd->id() != -1 && lastCmd->id() == command->id()) {
            if (lastCmd->mergeWith(command.get())) {
                // Command was merged, don't add new one
                emitStateChanged();
                return;
            }
        }
    }

    // Add new command
    m_commands.push_back(std::move(command));
    m_undoIndex = static_cast<int>(m_commands.size());

    // Enforce undo limit
    while (static_cast<int>(m_commands.size()) > m_undoLimit && m_undoLimit > 0) {
        m_commands.erase(m_commands.begin());
        m_undoIndex--;
        if (m_cleanIndex > 0) {
            m_cleanIndex--;
        } else if (m_cleanIndex == 0) {
            m_cleanIndex = -1;  // Clean state removed
        }
    }

    emitStateChanged();
}

void UndoStack::undo() {
    if (!canUndo()) {
        return;
    }

    m_undoIndex--;
    m_commands[static_cast<size_t>(m_undoIndex)]->undo();
    emitStateChanged();
}

void UndoStack::redo() {
    if (!canRedo()) {
        return;
    }

    m_commands[static_cast<size_t>(m_undoIndex)]->execute();
    m_undoIndex++;
    emitStateChanged();
}

QString UndoStack::undoText() const {
    if (!canUndo()) {
        return QCoreApplication::translate("UndoStack", "Undo");
    }
    return QCoreApplication::translate("UndoStack", "Undo %1")
        .arg(m_commands[static_cast<size_t>(m_undoIndex - 1)]->description());
}

QString UndoStack::redoText() const {
    if (!canRedo()) {
        return QCoreApplication::translate("UndoStack", "Redo");
    }
    return QCoreApplication::translate("UndoStack", "Redo %1")
        .arg(m_commands[static_cast<size_t>(m_undoIndex)]->description());
}

void UndoStack::setClean() {
    if (m_cleanIndex != m_undoIndex) {
        m_cleanIndex = m_undoIndex;
        emit cleanChanged(true);
    }
}

void UndoStack::clear() {
    if (m_commands.empty()) {
        return;
    }

    m_commands.clear();
    m_undoIndex = 0;
    m_cleanIndex = 0;
    emitStateChanged();
}

void UndoStack::setUndoLimit(int limit) {
    m_undoLimit = limit;

    // Enforce new limit
    while (static_cast<int>(m_commands.size()) > m_undoLimit && m_undoLimit > 0) {
        m_commands.erase(m_commands.begin());
        m_undoIndex = std::max(0, m_undoIndex - 1);
        m_cleanIndex = std::max(-1, m_cleanIndex - 1);
    }
}

const Command* UndoStack::command(int index) const {
    if (index < 0 || index >= static_cast<int>(m_commands.size())) {
        return nullptr;
    }
    return m_commands[static_cast<size_t>(index)].get();
}

void UndoStack::emitStateChanged() {
    emit canUndoChanged(canUndo());
    emit canRedoChanged(canRedo());
    emit cleanChanged(isClean());
    emit indexChanged(m_undoIndex);
}
