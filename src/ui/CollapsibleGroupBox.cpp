/**
 * @file CollapsibleGroupBox.cpp
 */

#include "CollapsibleGroupBox.hpp"

#include <QLayout>
#include <QVBoxLayout>

CollapsibleGroupBox::CollapsibleGroupBox(QWidget* parent)
    : QGroupBox(parent)
{
    // A checkable group box already draws the checkbox in the title and
    // remembers the state; all this adds is hiding the contents so the box
    // actually shrinks. Qt disables the children of an unchecked group box,
    // which is invisible while they are hidden and undone on expand.
    setCheckable(true);
    setChecked(true);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_content = new QWidget(this);
    outer->addWidget(m_content);

    connect(this, &QGroupBox::toggled, m_content, &QWidget::setVisible);
}

void CollapsibleGroupBox::setContentLayout(QLayout* layout) {
    m_content->setLayout(layout);
}

void CollapsibleGroupBox::setCollapsed(bool collapsed) {
    setChecked(!collapsed);
    m_content->setVisible(!collapsed);
}

bool CollapsibleGroupBox::isCollapsed() const {
    return !isChecked();
}
