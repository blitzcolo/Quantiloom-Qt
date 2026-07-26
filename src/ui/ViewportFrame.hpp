/**
 * @file ViewportFrame.hpp
 * @brief The central area: mode strip, empty state, and the Vulkan viewport
 *
 * Three jobs the bare window container could not do:
 *
 *  - **Mode strip.** The spectral mode decides what the image physically
 *    *means*, and the debug mode decides whether it is an image at all. Both
 *    are shown permanently beside the viewport. They are a strip above the
 *    render surface rather than a floating badge on top of it because a
 *    QWindow container does not respect widget stacking order — anything drawn
 *    over it is unreliable.
 *  - **Empty state.** Before a scene is loaded the viewport is a black
 *    rectangle with no affordances. The frame shows a guidance page in its
 *    place and swaps to the render surface once geometry arrives.
 *  - **Progress.** Shader compilation on first run used to be an application
 *    modal dialog with no cancel button. It reports here instead, leaving the
 *    window up and interactive.
 */

#pragma once

#include "UiStyle.hpp"

#include <QWidget>
#include <QStringList>

#include <core/Types.hpp>

QT_BEGIN_NAMESPACE
class QLabel;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;
QT_END_NAMESPACE

class ViewportFrame : public QWidget {
    Q_OBJECT

public:
    /// @param viewport the window container wrapping QuantiloomVulkanWindow
    explicit ViewportFrame(QWidget* viewport, QWidget* parent = nullptr);

    void setSpectralMode(quantiloom::SpectralMode mode);
    void setDebugMode(quantiloom::DebugVisualizationMode mode);

    /// Swap between the guidance page and the render surface.
    void setSceneLoaded(bool loaded);
    [[nodiscard]] bool isSceneLoaded() const { return m_sceneLoaded; }

    /// Non-modal busy indicator in the mode strip. An empty message hides it.
    void setBusyMessage(const QString& message);

    /// Recent scenes offered as buttons on the guidance page.
    void setRecentFiles(const QStringList& files);

    void retranslateUi();

signals:
    void openSceneRequested();
    void openRecentRequested(const QString& path);

protected:
    void changeEvent(QEvent* event) override;

private:
    /// Re-apply every theme-derived colour. Driven from changeEvent rather
    /// than called by the shell, so a theme change cannot be forgotten here
    /// the way an explicit call would be.
    void restyleUi();

    void buildEmptyState();
    void updateChips();
    void rebuildRecentButtons();

    QWidget* m_viewport = nullptr;
    QStackedWidget* m_stack = nullptr;

    // Mode strip
    QLabel* m_spectralChip = nullptr;
    QLabel* m_debugChip = nullptr;
    QLabel* m_previewOnlyChip = nullptr;
    QLabel* m_busyLabel = nullptr;
    QProgressBar* m_busyBar = nullptr;

    // Empty state
    QWidget* m_emptyPage = nullptr;
    QLabel* m_emptyTitle = nullptr;
    QLabel* m_emptyHint = nullptr;
    QPushButton* m_openButton = nullptr;
    QLabel* m_recentHeading = nullptr;
    QVBoxLayout* m_recentLayout = nullptr;

    QStringList m_recentFiles;
    quantiloom::SpectralMode m_spectralMode = quantiloom::SpectralMode::RGB;
    quantiloom::DebugVisualizationMode m_debugMode = quantiloom::DebugVisualizationMode::None;
    bool m_sceneLoaded = false;

    uistyle::StyleBindings m_styling;
};
