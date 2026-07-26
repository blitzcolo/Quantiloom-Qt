/**
 * @file PropertiesPanel.cpp
 */

#include "PropertiesPanel.hpp"

#include "MaterialEditorPanel.hpp"
#include "../ui/UiStyle.hpp"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

namespace {

/// Split a transform into the translation / rotation / scale a person edits.
/// Done by hand rather than through glm::decompose so the file does not have
/// to enable GLM's experimental extensions for three lines of arithmetic.
void decompose(const glm::mat4& m, glm::vec3& translation,
               glm::vec3& eulerDegrees, glm::vec3& scale) {
    translation = glm::vec3(m[3]);

    const glm::vec3 c0(m[0]);
    const glm::vec3 c1(m[1]);
    const glm::vec3 c2(m[2]);
    scale = glm::vec3(glm::length(c0), glm::length(c1), glm::length(c2));

    // A zero-scaled axis has no direction to recover; substitute the identity
    // rather than dividing by zero and producing NaN in every field.
    const glm::mat3 rotation(
        scale.x > 1e-6f ? c0 / scale.x : glm::vec3(1.0f, 0.0f, 0.0f),
        scale.y > 1e-6f ? c1 / scale.y : glm::vec3(0.0f, 1.0f, 0.0f),
        scale.z > 1e-6f ? c2 / scale.z : glm::vec3(0.0f, 0.0f, 1.0f));

    eulerDegrees = glm::degrees(glm::eulerAngles(glm::quat_cast(rotation)));
}

glm::mat4 compose(const glm::vec3& translation, const glm::vec3& eulerDegrees,
                  const glm::vec3& scale) {
    const glm::quat rotation(glm::radians(eulerDegrees));
    return glm::translate(glm::mat4(1.0f), translation)
         * glm::mat4_cast(rotation)
         * glm::scale(glm::mat4(1.0f), scale);
}

} // namespace

PropertiesPanel::PropertiesPanel(MaterialEditorPanel* materialEditor, QWidget* parent)
    : PanelBase(parent)
    , m_materialEditor(materialEditor)
{
    setObjectName(panelId());
    setupUi();
}

QString PropertiesPanel::panelTitle() const {
    return tr("Properties");
}

void PropertiesPanel::setupUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);
    outer->addWidget(m_stack);

    // --- page 0: nothing selected ---------------------------------------
    auto* emptyPage = new QWidget();
    auto* emptyLayout = new QVBoxLayout(emptyPage);
    emptyLayout->setAlignment(Qt::AlignCenter);
    m_emptyLabel = new QLabel(emptyPage);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    uistyle::applyHintStyle(m_emptyLabel);
    emptyLayout->addWidget(m_emptyLabel);
    m_stack->addWidget(emptyPage);

    // --- page 1: node ----------------------------------------------------
    auto* nodePage = new QWidget();
    auto* nodeLayout = new QVBoxLayout(nodePage);
    nodeLayout->setContentsMargins(4, 4, 4, 4);

    m_nodeName = new QLabel(nodePage);
    uistyle::applyHeadingStyle(m_nodeName);
    nodeLayout->addWidget(m_nodeName);

    m_transformGroup = new QGroupBox(nodePage);
    auto* transformLayout = new QFormLayout(m_transformGroup);

    auto makeTriple = [this](QDoubleSpinBox* (&fields)[3], double minimum, double maximum,
                             double step, int decimals) {
        auto* row = new QHBoxLayout();
        for (int axis = 0; axis < 3; ++axis) {
            auto* spin = new QDoubleSpinBox();
            spin->setRange(minimum, maximum);
            spin->setSingleStep(step);
            spin->setDecimals(decimals);
            spin->setKeyboardTracking(false);   // one edit, one undo entry
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &PropertiesPanel::onTransformFieldChanged);
            fields[axis] = spin;
            row->addWidget(spin);
        }
        return row;
    };

    m_positionCaption = new QLabel(m_transformGroup);
    transformLayout->addRow(m_positionCaption, makeTriple(m_position, -1e6, 1e6, 0.1, 3));

    m_rotationCaption = new QLabel(m_transformGroup);
    transformLayout->addRow(m_rotationCaption, makeTriple(m_rotation, -360.0, 360.0, 1.0, 2));

    m_scaleCaption = new QLabel(m_transformGroup);
    transformLayout->addRow(m_scaleCaption, makeTriple(m_scale, 0.001, 1e4, 0.1, 3));

    nodeLayout->addWidget(m_transformGroup);

    m_editMaterialButton = new QPushButton(nodePage);
    connect(m_editMaterialButton, &QPushButton::clicked, this, [this]() {
        if (m_materialIndex >= 0) {
            emit materialRequested(m_materialIndex);
        }
    });
    nodeLayout->addWidget(m_editMaterialButton);

    nodeLayout->addStretch();
    m_stack->addWidget(nodePage);

    // --- page 2: material -------------------------------------------------
    if (m_materialEditor) {
        m_stack->addWidget(m_materialEditor);
    } else {
        m_stack->addWidget(new QWidget());
    }

    // --- page 3: several nodes -------------------------------------------
    auto* multiPage = new QWidget();
    auto* multiLayout = new QVBoxLayout(multiPage);
    multiLayout->setAlignment(Qt::AlignCenter);
    m_multiLabel = new QLabel(multiPage);
    m_multiLabel->setAlignment(Qt::AlignCenter);
    uistyle::applyHintStyle(m_multiLabel);
    multiLayout->addWidget(m_multiLabel);
    m_stack->addWidget(multiPage);

    bindText([this] {
        m_emptyLabel->setText(tr("Nothing selected.\n"
                                 "Pick a node or a material in the scene tree."));
        m_transformGroup->setTitle(tr("Transform"));
        m_positionCaption->setText(tr("Position:"));
        m_rotationCaption->setText(tr("Rotation (°):"));
        m_scaleCaption->setText(tr("Scale:"));
        m_editMaterialButton->setText(tr("Edit Material"));
        m_multiLabel->setText(tr("%n nodes selected. Drag in the viewport to transform them "
                                 "together.", "", m_multiCount));
        if (m_nodeIndex < 0) {
            m_nodeName->setText(QString());
        }
    });

    showEmptyState();
}

void PropertiesPanel::retranslateUi() {
    PanelBase::retranslateUi();
    if (m_materialEditor) {
        m_materialEditor->retranslateUi();
    }
}

void PropertiesPanel::showEmptyState() {
    m_nodeIndex = -1;
    m_materialIndex = -1;
    m_stack->setCurrentIndex(0);
}

void PropertiesPanel::showNode(int nodeIndex, const QString& name,
                               const glm::mat4& transform, int materialIndex) {
    m_nodeIndex = nodeIndex;
    m_materialIndex = materialIndex;

    m_nodeName->setText(name);
    writeTransformFields(transform);

    m_editMaterialButton->setVisible(materialIndex >= 0);
    m_stack->setCurrentIndex(1);
}

void PropertiesPanel::showMultipleSelection(int count) {
    m_nodeIndex = -1;
    m_multiCount = count;
    m_multiLabel->setText(tr("%n nodes selected. Drag in the viewport to transform them "
                             "together.", "", count));
    m_stack->setCurrentIndex(3);
}

void PropertiesPanel::showMaterial() {
    m_stack->setCurrentIndex(2);
}

void PropertiesPanel::updateNodeTransform(const glm::mat4& transform) {
    if (m_nodeIndex < 0 || m_stack->currentIndex() != 1) {
        return;
    }
    writeTransformFields(transform);
}

void PropertiesPanel::writeTransformFields(const glm::mat4& transform) {
    glm::vec3 translation;
    glm::vec3 euler;
    glm::vec3 scale;
    decompose(transform, translation, euler, scale);

    m_updatingFields = true;
    for (int axis = 0; axis < 3; ++axis) {
        m_position[axis]->setValue(translation[axis]);
        m_rotation[axis]->setValue(euler[axis]);
        m_scale[axis]->setValue(scale[axis]);
    }
    m_updatingFields = false;
}

glm::mat4 PropertiesPanel::readTransformFields() const {
    glm::vec3 translation;
    glm::vec3 euler;
    glm::vec3 scale;
    for (int axis = 0; axis < 3; ++axis) {
        translation[axis] = static_cast<float>(m_position[axis]->value());
        euler[axis] = static_cast<float>(m_rotation[axis]->value());
        scale[axis] = static_cast<float>(m_scale[axis]->value());
    }
    return compose(translation, euler, scale);
}

void PropertiesPanel::onTransformFieldChanged() {
    if (m_updatingFields || m_nodeIndex < 0) {
        return;
    }
    emit nodeTransformEdited(m_nodeIndex, readTransformFields());
}
