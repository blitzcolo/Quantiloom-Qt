#pragma once
#include <QDialog>
#include <QPointer>
#include <core/Config.hpp>
#include <dataset/FusionExportJob.hpp>
#include <atomic>
#include <functional>
class QListWidget;
class QComboBox;
class QLineEdit;
class QDoubleSpinBox;
class QCheckBox;
class QProgressBar;
class QLabel;
class QThread;
class QPushButton;
class SensorPanel;
class FusionExportDialog : public QDialog {
    Q_OBJECT
public:
    FusionExportDialog(const quantiloom::Config& scene,const QString& baseDirectory,QWidget* parent=nullptr);
    ~FusionExportDialog() override;
    std::function<void(const quantiloom::Config&,const quantiloom::camera::CameraConfig&)> preview;
protected:
    void reject() override;
private:
    void selectCamera(int index);
    void saveCamera();
    void refreshList();
    void start();
    quantiloom::Config m_scene;
    QString m_baseDirectory;
    quantiloom::dataset::RigConfig m_rig;
    QListWidget* m_cameras=nullptr;
    QComboBox* m_reference=nullptr;
    QLineEdit* m_id=nullptr;
    QLineEdit* m_output=nullptr;
    QLineEdit* m_sample=nullptr;
    QDoubleSpinBox* m_pose[6]{};
    QCheckBox* m_rectify=nullptr;
    QProgressBar* m_progress=nullptr;
    QLabel* m_status=nullptr;
    QPushButton* m_start=nullptr;
    SensorPanel* m_sensor=nullptr;
    QPointer<QThread> m_thread;
    std::atomic_bool m_cancelled=false;
    bool m_loading=false;
    int m_selected=-1;
};
