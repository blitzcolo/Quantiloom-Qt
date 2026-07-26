/**
 * @file PanelBase.cpp
 */

#include "PanelBase.hpp"

#include <QEvent>

void PanelBase::bindText(std::function<void()> setter) {
    setter();
    m_textSetters.push_back(std::move(setter));
}

void PanelBase::bindStyle(std::function<void()> setter) {
    m_styling.bind(std::move(setter));
}

void PanelBase::retranslateUi() {
    for (const auto& setter : m_textSetters) {
        setter();
    }
}

void PanelBase::restyleUi() {
    m_styling.reapply();
}

void PanelBase::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        emit panelTitleChanged();
    } else if (uistyle::isThemeChangeEvent(event)) {
        restyleUi();
    }
    QWidget::changeEvent(event);
}
