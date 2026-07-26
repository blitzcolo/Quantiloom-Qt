/**
 * @file PanelBase.cpp
 */

#include "PanelBase.hpp"

#include <QEvent>

void PanelBase::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        emit panelTitleChanged();
    }
    QWidget::changeEvent(event);
}
