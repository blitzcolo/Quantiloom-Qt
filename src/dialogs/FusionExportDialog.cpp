#include "FusionExportDialog.hpp"
#include "../panels/SensorPanel.hpp"
#include <scene/Camera.hpp>
#include <postprocess/CameraPresets.hpp>
#include <postprocess/CameraConfigIO.hpp>
#include <algorithm>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QFileDialog>
#include <QMessageBox>
#include <QThread>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <QEvent>
#include <QShowEvent>
#include <QRegularExpression>

using namespace quantiloom;
using namespace quantiloom::dataset;
FusionExportDialog::FusionExportDialog(const Config& scene,const QString& base,SpectralMode spectralMode,QWidget* parent)
    :QDialog(parent),m_scene(scene),m_baseDirectory(base) {
    setWindowTitle(tr("Fusion Dataset Export"));resize(1050,800);
    m_rig.id="studio-rig";m_rig.referenceCamera="camera_0";
    RigCamera first;first.id="camera_0";
    auto preset=camera::MakePresetCameraConfig(camera::CameraPresetKind::GenericCmos);
    first.sensor=preset.value();
    auto authored=ParseCameraConfig(scene,spectralMode,base.toStdString());
    if(authored && authored.value().enabled) first.sensor=authored.value();
    first.sensor.enabled=true;first.sensor.motion.keys.clear();
    auto c=Camera::FromConfig(scene,1.0f);
    if(c) {
        const auto p=c.value().GetCameraData();
        first.cameraToRig={p.right.x,-p.up.x,p.forward.x,p.origin.x,
            p.right.y,-p.up.y,p.forward.y,p.origin.y,p.right.z,-p.up.z,p.forward.z,p.origin.z,0,0,0,1};
    }
    m_rig.cameras.push_back(first);
    auto* layout=new QVBoxLayout(this);
    auto* top=new QFormLayout;
    m_sample=new QLineEdit(QStringLiteral("sample"));m_output=new QLineEdit(QDir::current().filePath("fusion-output"));
    top->addRow(QStringLiteral(QT_TR_NOOP("Sample ID:")),m_sample);
    auto* outputRow=new QHBoxLayout;outputRow->addWidget(m_output);
    auto* browse=new QPushButton(QStringLiteral(QT_TR_NOOP("Browse...")));outputRow->addWidget(browse);
    connect(browse,&QPushButton::clicked,this,[this]{auto p=QFileDialog::getExistingDirectory(this,tr("Output directory"),m_output->text());if(!p.isEmpty())m_output->setText(p);});
    top->addRow(QStringLiteral(QT_TR_NOOP("Output:")),outputRow);layout->addLayout(top);
    auto* split=new QSplitter;layout->addWidget(split,1);
    auto* left=new QWidget;auto* listLayout=new QVBoxLayout(left);
    m_cameras=new QListWidget;listLayout->addWidget(m_cameras);
    auto* row=new QHBoxLayout;auto* add=new QPushButton(QStringLiteral(QT_TR_NOOP("Add camera")));auto* remove=new QPushButton(QStringLiteral(QT_TR_NOOP("Remove")));
    row->addWidget(add);row->addWidget(remove);listLayout->addLayout(row);
    m_reference=new QComboBox;listLayout->addWidget(new QLabel(QStringLiteral(QT_TR_NOOP("Reference camera:"))));listLayout->addWidget(m_reference);
    m_pairMode=new QComboBox;m_pairMode->setObjectName("pairMode");
    m_pairMode->addItem(tr("Default reference pairs"));m_pairMode->addItem(tr("Explicit pairs"));
    listLayout->addWidget(m_pairMode);m_pairs=new QLineEdit;m_pairs->setObjectName("explicitPairs");
    m_pairs->setPlaceholderText(tr("source > target; source > target"));listLayout->addWidget(m_pairs);m_pairs->setEnabled(false);
    connect(m_pairMode,&QComboBox::currentIndexChanged,this,[this](int i){m_pairs->setEnabled(i==1);});
    auto* load=new QPushButton(QStringLiteral(QT_TR_NOOP("Load rig...")));auto* save=new QPushButton(QStringLiteral(QT_TR_NOOP("Save rig...")));
    listLayout->addWidget(load);listLayout->addWidget(save);split->addWidget(left);
    auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);auto* right=new QWidget;
    auto* rightLayout=new QVBoxLayout(right);auto* pose=new QFormLayout;
    m_id=new QLineEdit;pose->addRow(QStringLiteral(QT_TR_NOOP("Camera ID:")),m_id);
    const char* labels[]={QT_TR_NOOP("X (world units):"),QT_TR_NOOP("Y (world units):"),QT_TR_NOOP("Z (world units):"),QT_TR_NOOP("Pitch (degrees):"),QT_TR_NOOP("Yaw (degrees):"),QT_TR_NOOP("Roll (degrees):")};
    for(int i=0;i<6;++i){m_pose[i]=new QDoubleSpinBox;m_pose[i]->setRange(i<3 ? -1e8 : -360,i<3 ? 1e8 : 360);m_pose[i]->setDecimals(6);pose->addRow(QString::fromLatin1(labels[i]),m_pose[i]);}
    rightLayout->addLayout(pose);m_sensor=new SensorPanel;rightLayout->addWidget(m_sensor);
    auto* previewButton=new QPushButton(QStringLiteral(QT_TR_NOOP("Preview selected camera")));rightLayout->addWidget(previewButton);
    connect(previewButton,&QPushButton::clicked,this,[this]{updatePreview();});
    scroll->setWidget(right);split->addWidget(scroll);split->setStretchFactor(1,1);
    m_rectify=new QCheckBox(tr("Also export undistorted images and coordinate maps"));layout->addWidget(m_rectify);
    m_progress=new QProgressBar;layout->addWidget(m_progress);m_status=new QLabel; m_status->setWordWrap(true);layout->addWidget(m_status);
    auto* buttons=new QHBoxLayout;m_start=new QPushButton(QStringLiteral(QT_TR_NOOP("Export")));auto* close=new QPushButton(QStringLiteral(QT_TR_NOOP("Close")));buttons->addStretch();buttons->addWidget(m_start);buttons->addWidget(close);layout->addLayout(buttons);
    connect(close,&QPushButton::clicked,this,&FusionExportDialog::reject);
    connect(m_start,&QPushButton::clicked,this,&FusionExportDialog::start);
    connect(m_cameras,&QListWidget::currentRowChanged,this,&FusionExportDialog::selectCamera);
    connect(m_reference,&QComboBox::currentTextChanged,this,[this](const QString& id){if(!m_loading)m_rig.referenceCamera=id.toStdString();});
    for(auto* field:m_pose)connect(field,&QDoubleSpinBox::valueChanged,this,[this]{updatePreview();});
    connect(m_sensor,&SensorPanel::cameraConfigChanged,this,[this]{updatePreview();});
    connect(add,&QPushButton::clicked,this,[this]{if(m_thread)return;saveCamera();RigCamera c=m_rig.cameras[m_selected<0 ? 0 : m_selected];int i=static_cast<int>(m_rig.cameras.size());do{c.id="camera_"+std::to_string(i++);}while(std::any_of(m_rig.cameras.begin(),m_rig.cameras.end(),[&](const auto& v){return v.id==c.id;}));c.cameraToRig[3]+=.1;m_rig.cameras.push_back(c);refreshList();m_cameras->setCurrentRow(static_cast<int>(m_rig.cameras.size()-1));});
    connect(remove,&QPushButton::clicked,this,[this]{if(m_thread||m_selected<0||m_rig.cameras.size()<2)return;if(!collectPairs())return;const auto removed=m_rig.cameras[m_selected].id;
        std::erase_if(m_rig.pairs,[&](const auto& p){return p.sourceCamera==removed || p.targetCamera==removed;});
        QStringList kept;for(const auto& p:m_rig.pairs)kept<<QString::fromStdString(p.sourceCamera+" > "+p.targetCamera);
        m_pairs->setText(kept.join("; "));m_status->setText(tr("Removed camera and its pairs."));
        m_rig.cameras.erase(m_rig.cameras.begin()+m_selected);m_selected=-1;if(m_rig.referenceCamera==removed)m_rig.referenceCamera=m_rig.cameras.front().id;refreshList();m_cameras->setCurrentRow(0);});
    connect(load,&QPushButton::clicked,this,[this]{if(m_thread)return;auto path=QFileDialog::getOpenFileName(this,tr("Load rig"),{},tr("TOML (*.toml)"));if(path.isEmpty())return;auto doc=Config::Load(path.toStdString());if(!doc){QMessageBox::warning(this,tr("Rig"),QString::fromStdString(doc.error()));return;}auto rig=ParseRigConfig(*doc,QFileInfo(path).absolutePath().toStdString());if(!rig){QMessageBox::warning(this,tr("Rig"),QString::fromStdString(rig.error()));return;}m_rig=*rig;m_selected=-1;
        m_pairMode->setCurrentIndex(doc.value().Has("rig.pairs") ? 1 : 0);
        QStringList pairs;for(const auto& pair:m_rig.pairs)pairs<<QString::fromStdString(pair.sourceCamera+" > "+pair.targetCamera);
        m_pairs->setText(pairs.join("; "));refreshList();m_cameras->setCurrentRow(0);});
    connect(save,&QPushButton::clicked,this,[this]{saveCamera();if(!collectPairs())return;auto path=QFileDialog::getSaveFileName(this,tr("Save rig"),{},tr("TOML (*.toml)"));if(path.isEmpty())return;QFile file(path);if(!file.open(QIODevice::WriteOnly)||file.write(QByteArray::fromStdString(RigConfigToToml(m_rig)))<0)QMessageBox::warning(this,tr("Rig"),tr("Could not write the rig."));});
    for(auto* label:findChildren<QLabel*>())if(label!=m_status && !m_sensor->isAncestorOf(label))label->setProperty("fusionSourceText",label->text().toUtf8());
    for(auto* button:findChildren<QPushButton*>())if(!m_sensor->isAncestorOf(button))button->setProperty("fusionSourceText",button->text().toUtf8());
    retranslate();refreshList();m_cameras->setCurrentRow(0);
}
FusionExportDialog::~FusionExportDialog(){m_cancelled=true;if(m_thread){m_thread->wait();delete m_thread.data();}}
void FusionExportDialog::reject(){if(m_thread){if(m_publishing)return;m_cancelled=true;m_status->setText(tr("Stopping before publication..."));return;}QDialog::reject();}
void FusionExportDialog::refreshList(){m_loading=true;m_cameras->clear();m_reference->clear();for(const auto& c:m_rig.cameras){m_cameras->addItem(QString::fromStdString(c.id));m_reference->addItem(QString::fromStdString(c.id));}m_reference->setCurrentText(QString::fromStdString(m_rig.referenceCamera));m_loading=false;}
void FusionExportDialog::saveCamera(){if(m_loading||m_selected<0)return;auto& c=m_rig.cameras[m_selected];const auto previous=c.id;c.id=m_id->text().toStdString();
    if(previous!=c.id){for(auto& p:m_rig.pairs){if(p.sourceCamera==previous)p.sourceCamera=c.id;if(p.targetCamera==previous)p.targetCamera=c.id;}
        m_pairs->setText(m_pairs->text().replace(QRegularExpression("(?<![A-Za-z0-9_-])"+QRegularExpression::escape(QString::fromStdString(previous))+"(?![A-Za-z0-9_-])"),QString::fromStdString(c.id)));
        m_cameras->item(m_selected)->setText(QString::fromStdString(c.id));
        const int referenceIndex=m_reference->findText(QString::fromStdString(previous));
        if(referenceIndex>=0)m_reference->setItemText(referenceIndex,QString::fromStdString(c.id));}
if(m_rig.referenceCamera==previous)m_rig.referenceCamera=c.id;c.sensor=m_sensor->getCameraConfig();c.sensor.enabled=true;c.sensor.motion.keys.clear();const auto q=glm::dquat(glm::radians(glm::dvec3(m_pose[3]->value(),m_pose[4]->value(),m_pose[5]->value())));const auto r=glm::mat3_cast(q);for(int row=0;row<3;++row){for(int col=0;col<3;++col)c.cameraToRig[row*4+col]=r[col][row];c.cameraToRig[row*4+3]=m_pose[row]->value();}}
void FusionExportDialog::selectCamera(int index){if(m_loading)return;saveCamera();m_selected=index;if(index<0)return;m_loading=true;const auto& c=m_rig.cameras[index];m_id->setText(QString::fromStdString(c.id));glm::dmat3 r;for(int row=0;row<3;++row){for(int col=0;col<3;++col)r[col][row]=c.cameraToRig[row*4+col];m_pose[row]->setValue(c.cameraToRig[row*4+3]);}const auto angles=glm::degrees(glm::eulerAngles(glm::quat_cast(r)));for(int i=0;i<3;++i)m_pose[i+3]->setValue(angles[i]);m_sensor->setCameraConfig(c.sensor);m_sensor->setCameraEnabled(c.sensor.enabled);m_loading=false;updatePreview();}
void FusionExportDialog::start(){
    if(m_thread){if(m_publishing)return;m_cancelled=true;m_start->setEnabled(false);return;}
    saveCamera();m_rig.referenceCamera=m_reference->currentText().toStdString();
    if(!collectPairs())return;
    auto doc=Config::Parse(RigConfigToToml(m_rig));auto rig=doc ? ParseRigConfig(*doc,m_baseDirectory.toStdString()) : Result<RigConfig,String>(Result<RigConfig,String>::Err(doc.error()));
    if(!rig){QMessageBox::warning(this,tr("Rig"),QString::fromStdString(rig.error()));return;}
    FusionExportOptions options;options.outputDirectory=m_output->text().toStdString();options.sampleId=m_sample->text().toStdString();options.rectify=m_rectify->isChecked();options.renderer.baseDir=m_baseDirectory.toStdString();
    const QString models=qEnvironmentVariable("QUANTILOOM_ATMOS_MODELS",QDir::current().filePath("assets/atmos_models"));if(QDir(models).exists())options.renderer.atmosphereModelPackFallback=models.toStdString();
    m_cancelled=false;m_publishing=false;options.cancelled=[this]{return m_cancelled.load();};
    QPointer<FusionExportDialog> self(this);options.onProgress=[self](const FusionExportProgress& p){if(!self)return;if(p.phase=="publish")self->m_publishing=true;QMetaObject::invokeMethod(self.data(),[self,p]{if(!self)return;self->m_progress->setRange(0,static_cast<int>(p.totalCameras));self->m_progress->setValue(static_cast<int>(p.completedCameras));if(p.phase=="publish"){self->m_start->setEnabled(false);self->m_status->setText(self->tr("Completing publication..."));}
        else self->m_status->setText(QString::fromStdString(p.cameraId+": "+p.phase));},Qt::QueuedConnection);};
    const auto frozen=m_scene;const auto cameras=*rig;
    m_thread=QThread::create([self,frozen,cameras,options]{auto result=FusionExportJob::Run(frozen,cameras,options);if(!self)return;QMetaObject::invokeMethod(self.data(),[self,result]{if(!self)return;self->m_status->setText(result ? self->tr("Saved %1").arg(QString::fromStdString(result.value().manifestPath)) : QString::fromStdString(result.error()));},Qt::QueuedConnection);});
    connect(m_thread.data(),&QThread::finished,this,[this]{auto* thread=m_thread.data();m_thread=nullptr;m_publishing=false;if(thread)thread->deleteLater();m_start->setEnabled(true);m_start->setText(tr("Export"));m_cameras->setEnabled(true);});
    m_start->setText(tr("Cancel"));m_cameras->setEnabled(false);m_thread->start();
}

void FusionExportDialog::updatePreview(){
    if(m_loading || !preview || m_selected<0 || m_thread)return;
    saveCamera();const auto doc=Config::Parse(RigConfigToToml(m_rig));if(!doc)return;
    const auto rig=ParseRigConfig(*doc,m_baseDirectory.toStdString());if(!rig)return;
    const auto& camera=rig.value().cameras[m_selected];
    auto config=RigCameraScene(m_scene,*rig,camera.id);if(config)preview(*config,camera.sensor);
}
void FusionExportDialog::changeEvent(QEvent* event){QDialog::changeEvent(event);if(event->type()==QEvent::LanguageChange)retranslate();}
void FusionExportDialog::retranslate(){
    for(auto* label:findChildren<QLabel*>())if(label->property("fusionSourceText").isValid())label->setText(tr(label->property("fusionSourceText").toByteArray().constData()));
    for(auto* button:findChildren<QPushButton*>())if(button->property("fusionSourceText").isValid())button->setText(tr(button->property("fusionSourceText").toByteArray().constData()));
    setWindowTitle(tr("Fusion Dataset Export"));m_start->setText(m_thread ? tr("Cancel") : tr("Export"));
    m_rectify->setText(tr("Also export undistorted images and coordinate maps"));
    m_pairMode->setItemText(0,tr("Default reference pairs"));m_pairMode->setItemText(1,tr("Explicit pairs"));
    m_pairs->setPlaceholderText(tr("source > target; source > target"));
}

bool FusionExportDialog::collectPairs(){
    m_rig.referenceCamera=m_reference->currentText().toStdString();
    m_rig.pairs.clear();
    if(m_pairMode->currentIndex()==0){for(const auto& c:m_rig.cameras)if(c.id!=m_rig.referenceCamera)m_rig.pairs.push_back({c.id,m_rig.referenceCamera});}
    else for(const auto& pair:m_pairs->text().split(';',Qt::SkipEmptyParts)) {
        const auto parts=pair.split('>');if(parts.size()!=2){QMessageBox::warning(this,tr("Rig"),tr("Use source > target for each pair."));return false;}
        m_rig.pairs.push_back({parts[0].trimmed().toStdString(),parts[1].trimmed().toStdString()});
    }
    return true;
}

void FusionExportDialog::showEvent(QShowEvent* event){QDialog::showEvent(event);updatePreview();}
