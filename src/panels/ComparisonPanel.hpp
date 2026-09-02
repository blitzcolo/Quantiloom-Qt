/**
 * @file ComparisonPanel.hpp
 * @brief The render against a measurement, or against another render
 *
 * A renderer that claims physical units has to be checkable against
 * something that was measured, and until now the checking happened outside:
 * export an EXR, open a Python session, subtract. That loop is long enough
 * that it was run at the end of a study rather than while one was being set
 * up, which is the wrong end -- the numbers that matter are the ones that say
 * a scene is wrong before a day of renders is spent on it.
 *
 * So: load a reference EXR, and the panel reports what the current frame
 * differs from it by. Statistics rather than a picture, because the question
 * is how far off and where, and "a bit darker" is not an answer anyone can
 * put in a table. The difference image is there too, for the where.
 *
 * @author blitzcolo
 */

#pragma once

#include "../ui/PanelBase.hpp"

#include <core/Image.hpp>

#include <memory>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QPushButton;
QT_END_NAMESPACE

/**
 * @class ComparisonPanel
 * @brief Loads a reference image and reports how the render differs from it
 */
class ComparisonPanel : public PanelBase {
    Q_OBJECT

public:
    explicit ComparisonPanel(QWidget* parent = nullptr);
    ~ComparisonPanel() override;

    [[nodiscard]] QString panelTitle() const override;
    [[nodiscard]] QString panelId() const override { return QStringLiteral("comparison"); }
    void retranslateUi() override;

    /// Hand the panel the frame to compare. Called when the user asks for a
    /// comparison rather than every frame: a path-traced image is still moving
    /// while it accumulates, and statistics against a moving image would be
    /// statistics about the noise.
    void setRenderedImage(std::unique_ptr<quantiloom::Image> image);

signals:
    /// The panel wants the current frame. The window is what can capture one.
    void frameRequested();

private slots:
    void onLoadReference();
    void onCompare();

private:
    void updateReport();
    void showDifference();

    QPushButton* m_loadButton = nullptr;
    QPushButton* m_compareButton = nullptr;
    QLabel* m_referenceCaption = nullptr;
    QComboBox* m_channelCombo = nullptr;
    QLabel* m_report = nullptr;
    QLabel* m_differenceView = nullptr;

    std::unique_ptr<quantiloom::Image> m_reference;
    std::unique_ptr<quantiloom::Image> m_rendered;
    QString m_referencePath;
};
