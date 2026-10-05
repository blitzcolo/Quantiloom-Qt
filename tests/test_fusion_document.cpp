#include "config/ConfigManager.hpp"
#include <dataset/RigConfig.hpp>
#include <dataset/FusionExportJob.hpp>
#include <dataset/ExportSession.hpp>
#include <io/ImageIO.hpp>
#include <renderer/OfflineRenderer.hpp>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <filesystem>
#include <iostream>
#include <cmath>

using namespace quantiloom;
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    if(argc!=2)return 2;
    const std::filesystem::path fixture=argv[1];
    ConfigManager manager;SceneConfig scene;
    const auto path=fixture/"transmission_scene.toml";
    if(!manager.loadConfig(QString::fromStdString(path.string()),scene)){std::cerr<<manager.lastError().toStdString();return 1;}
    auto original=Config::Load(path);auto frozen=Config::Parse(manager.exportConfigToString(scene).toStdString());
    if(!original || !frozen)return 1;
    const auto tables=frozen.value().GetTableArray("materials");
    bool found=false;
    for(const auto& t:tables)if(t.GetString("name")=="Window"){
        found=t.GetString("fusion_transport")=="solid" && t.GetFloat("fusion_absorption_m_inv")==10 && t.GetFloat("ior")==1.5;
    }
    if(!found){std::cerr<<"Frozen material transport was lost\n";return 1;}
    for(auto& m:scene.materialConfigs)if(m.name=="Window")m.irTemperature_K=315;
    auto edited=Config::Parse(manager.exportConfigToString(scene).toStdString());
    if(!edited)return 1;
    for(const auto& t:edited.value().GetTableArray("materials"))if(t.GetString("name")=="Window" && t.GetFloat("ir_temperature_k")!=315){std::cerr<<"Old override won over edit\n";return 1;}
    QTemporaryDir temp;if(!temp.isValid())return 1;
    const auto saved=temp.filePath("roundtrip.toml");
    if(!manager.exportConfig(saved,scene))return 1;
    SceneConfig reload;ConfigManager reloader;if(!reloader.loadConfig(saved,reload))return 1;
    auto again=Config::Parse(reloader.exportConfigToString(reload).toStdString());if(!again)return 1;
    for(const auto& t:again.value().GetTableArray("materials"))if(t.GetString("name")=="Window" && t.GetString("fusion_transport")!="solid")return 1;
    auto job=Config::Load(fixture/"transmission_job.toml");if(!job)return 1;
    auto rig=dataset::ParseRigConfig(*job,fixture.string());if(!rig)return 1;
    rig.value().pairs={{"reference","offset"}};
    auto rigDoc=Config::Parse(dataset::RigConfigToToml(*rig));if(!rigDoc)return 1;
    auto rigAgain=dataset::ParseRigConfig(*rigDoc,fixture.string());if(!rigAgain || rigAgain.value().pairs.size()!=1 || rigAgain.value().pairs[0].sourceCamera!="reference")return 1;
    std::vector<float> reference;
    for(const auto* input:{&original.value(),&frozen.value()}){
        auto cfg=dataset::RigCameraScene(*input,*rig,"reference");if(!cfg)return 1;
        OfflineRenderer::InitParams init;init.baseDir=fixture.string();
        auto renderer=OfflineRenderer::Create(*cfg,init);if(!renderer){std::cerr<<renderer.error();return 1;}
        camera::CaptureState state;dataset::FusionCaptureOptionsV2 options;options.recordPaths=false;
        auto captured=renderer.value()->CaptureFusionV2(state,0,options);if(!captured){std::cerr<<captured.error();return 1;}
        const auto& pixels=captured.value().linearReference.data;
        if(reference.empty())reference=pixels;
        else {
            if(reference.size()!=pixels.size())return 1;
            for(size_t i=0;i<pixels.size();++i)if(std::abs(reference[i]-pixels[i])>1e-5*std::max(std::abs(reference[i]),1e-20f)){
                std::cerr<<"CLI/Studio frozen capture differs at "<<i<<"\n";return 1;
            }
        }
    }
    dataset::FusionExportOptions exportOptions;
    exportOptions.renderer.baseDir=fixture.string();exportOptions.sampleId="studio";exportOptions.rectify=true;
    exportOptions.maxOpticalSourcePaths=8;
    for(size_t variant=0;variant<2;++variant){
        exportOptions.outputDirectory=temp.filePath(variant ? "studio" : "cli").toStdString();
        auto exported=dataset::FusionExportJob::Run(variant ? *frozen : *original,*rig,exportOptions);
        if(!exported){std::cerr<<exported.error();return 1;}
        if(!dataset::ExportSession::Verify(exported.value().recordPath).valid)return 1;
    }
    for(const auto& c:rig.value().cameras){
        const QString path=QStringLiteral("/observations/")+QString::fromStdString(c.id)+QStringLiteral("/measurement.exr");
        const auto a=ImageIO::ReadEXR((temp.filePath("cli")+path).toStdString());
        const auto b=ImageIO::ReadEXR((temp.filePath("studio")+path).toStdString());
        if(!a || !b || a->data!=b->data){std::cerr<<"CLI/Studio package pixels differ\n";return 1;}
    }
    std::cout<<"PASS: material roundtrip, edited precedence, explicit rig pairs, CLI/Studio capture equality\n";
    return 0;
}
