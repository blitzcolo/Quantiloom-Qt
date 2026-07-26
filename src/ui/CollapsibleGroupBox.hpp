/**
 * @file CollapsibleGroupBox.hpp
 * @brief Group box whose contents can be folded away
 *
 * Used by the taller panels (sensor calibration above all) so that rarely
 * touched groups do not force the panel to be tall enough for every control
 * at once. Collapsing is purely visual — the widgets keep their values and
 * keep emitting signals when set programmatically.
 */

#pragma once

#include <QGroupBox>

QT_BEGIN_NAMESPACE
class QLayout;
QT_END_NAMESPACE

class CollapsibleGroupBox : public QGroupBox {
    Q_OBJECT

public:
    explicit CollapsibleGroupBox(QWidget* parent = nullptr);

    /// Install the layout holding the group's controls.
    void setContentLayout(QLayout* layout);

    /// The widget the content layout is installed on, for parenting.
    [[nodiscard]] QWidget* contentWidget() const { return m_content; }

    void setCollapsed(bool collapsed);
    [[nodiscard]] bool isCollapsed() const;

private:
    QWidget* m_content = nullptr;
};
