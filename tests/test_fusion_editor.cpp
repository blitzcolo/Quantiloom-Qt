#include "config/ConfigManager.hpp"
#include "dialogs/FusionExportDialog.hpp"
#include <QApplication>
#include <QPushButton>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>
#include <stdexcept>

void require(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    try {
        QTemporaryDir temp;require(temp.isValid(),"temporary directory");
        for(const QString header:{QString("[[materials]]\nname='Lamp'\n"),QString("[material_overrides.Lamp]\n")}) {
            QFile file(temp.filePath("scene.toml"));require(file.open(QIODevice::WriteOnly),"open fixture");
            file.write((header+"emissive_curve='d65'\nemissive_scale='absolute'\nemissive_curve_column=2\nspectral_material_ref='old'\nspectral_material_type='quantiloom_usgs'\nfluorescence_excitation_curve='ex.csv'\nfluorescence_emission_curve='em.csv'\nfluorescence_yield=0.5\nfusion_transport='solid'\nfusion_absorption_m_inv=10.0\n").toUtf8());file.close();
            ConfigManager manager;SceneConfig scene;
            require(manager.loadConfig(file.fileName(),scene),"load fixture");
            require(scene.materialConfigs.size()==1,"one material");
            auto& m=scene.materialConfigs.front();m.hasPbr=true;
            m.emissiveCurve.clear();m.emissiveScale.clear();m.emissiveCurveColumn=0;
            m.spectralMaterialRefs.clear();m.spectralMaterialType.clear();
            m.fluorescenceExcitationCurve.clear();m.fluorescenceEmissionCurve.clear();m.fluorescenceYield=0;
            auto parsed=quantiloom::Config::Parse(manager.exportConfigToString(scene).toStdString());
            require(bool(parsed),"parse frozen snapshot");
            auto material=parsed.value().GetTableArray("materials").front();
            require(!material.Has("emissive_curve") && !material.Has("spectral_material_ref") && !material.Has("fluorescence_excitation_curve"),"cleared bindings resurrected");
            require(material.GetString("fusion_transport")=="solid" && material.GetDouble("fusion_absorption_m_inv")==10,"unowned transport lost");
            require(manager.exportConfig(temp.filePath("saved.toml"),scene),"save cleared document");
            SceneConfig again;require(manager.loadConfig(temp.filePath("saved.toml"),again),"reload cleared document");
            require(again.materialConfigs.front().emissiveCurve.isEmpty() && !again.materialConfigs.front().hasSpectral(),"clear roundtrip");
            // Switching alias must not leave both keys, which the SDK rejects.
            m.spectralMaterialRefs={"new1","new2"};
            parsed=quantiloom::Config::Parse(manager.exportConfigToString(scene).toStdString());require(bool(parsed),"parse mixture");
            material=parsed.value().GetTableArray("materials").front();
            require(!material.Has("spectral_material_ref") && material.GetStringArray("spectral_material_refs").size()==2,"singular alias survived mixture edit");
        }
        auto cfg=quantiloom::Config::Parse("[camera]\nposition=[0.0,0.0,3.0]\nlook_at=[0.0,0.0,0.0]\n");require(bool(cfg),"camera fixture");
        FusionExportDialog dialog(*cfg,QString());
        QPushButton *add=nullptr,*remove=nullptr;
        for(auto* b:dialog.findChildren<QPushButton*>()){if(b->text()=="Add camera")add=b;if(b->text()=="Remove")remove=b;}
        require(add && remove,"camera buttons");add->click();
        QComboBox* reference=nullptr;for(auto* b:dialog.findChildren<QComboBox*>())if(b->findText("camera_0")>=0)reference=b;
        require(reference,"reference selector");reference->setCurrentText("camera_1");add->click();
        require(reference->currentText()=="camera_1","adding camera reset reference");
        auto* list=dialog.findChild<QListWidget*>();auto* pairs=dialog.findChild<QLineEdit*>("explicitPairs");
        auto* mode=dialog.findChild<QComboBox*>("pairMode");require(list && pairs && mode,"rig controls");
        mode->setCurrentIndex(1);pairs->setText("camera_2 > camera_1; camera_0 > camera_1");
        QLineEdit* id=nullptr;for(auto* e:dialog.findChildren<QLineEdit*>())if(e->text()=="camera_2")id=e;
        require(id,"camera ID");id->setText("renamed");list->setCurrentRow(0);
        require(reference->findText("camera_2")==-1 && reference->findText("renamed")>=0,"stale reference option after rename");
        require(pairs->text().startsWith("renamed >"),"pair rename");
        list->setCurrentRow(2);remove->click();
        require(!pairs->text().contains("renamed") && pairs->text().contains("camera_0 > camera_1"),"pair removal");
        require(reference->currentText()=="camera_1","delete changed surviving reference");
        list->setCurrentRow(1);id->setText("reference_new");list->setCurrentRow(0);
        require(reference->currentText()=="reference_new" && pairs->text().contains("reference_new"),"selected reference rename");
        std::cout<<"Fusion editor regression passed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
}
