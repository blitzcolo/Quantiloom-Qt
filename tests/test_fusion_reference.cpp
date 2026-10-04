#include "dialogs/FusionExportDialog.hpp"
#include <QApplication>
#include <QPushButton>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <iostream>
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    auto cfg=quantiloom::Config::Parse("[camera]\nposition=[0.0,0.0,3.0]\nlook_at=[0.0,0.0,0.0]\n");
    if(!cfg)return 1;
    FusionExportDialog dialog(*cfg,QString());
    QPushButton* add=nullptr;for(auto* b:dialog.findChildren<QPushButton*>())if(b->text()=="Add camera")add=b;
    if(!add)return 2;add->click();
    QComboBox* reference=nullptr;for(auto* b:dialog.findChildren<QComboBox*>())if(b->findText("camera_0")>=0)reference=b;
    if(!reference)return 3;
    reference->setCurrentText("camera_1");add->click();
    if(reference->currentText()!="camera_1"){std::cerr<<"Adding a camera reset the reference\n";return 4;}
    QLineEdit* id=nullptr;for(auto* e:dialog.findChildren<QLineEdit*>())if(e->text()=="camera_2")id=e;
    auto* list=dialog.findChild<QListWidget*>();if(!id || !list)return 5;
    id->setText("renamed");list->setCurrentRow(0);
    if(reference->findText("camera_2")>=0 || reference->findText("renamed")<0){std::cerr<<"Stale nonselected camera reference\n";return 6;}
    list->setCurrentRow(1);id->setText("reference_new");list->setCurrentRow(0);
    if(reference->currentText()!="reference_new"){std::cerr<<"Selected reference rename lost\n";return 7;}
    std::cout<<"Fusion reference regression passed\n";return 0;
}
