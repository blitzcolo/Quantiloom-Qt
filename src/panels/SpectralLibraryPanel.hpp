/**
 * @file SpectralLibraryPanel.hpp
 * @brief Browse the measured-material databases and assign one to a material
 *
 * The SDK ships three NMF spectral databases in assets/spectral -- USGS (1374
 * entries), ECOSTRESS (3450) and RefractiveIndex.INFO (585) -- and until this
 * panel existed the application referenced none of them. They are the core's
 * flagship quantitative path: a material carrying a database reference renders
 * from a measured reflectance spectrum rather than from an RGB triple guessed
 * up into one. Reaching that took hand-editing TOML.
 *
 * The rows come from the material_summary_*.csv files rather than from the
 * multi-megabyte JSONs, which are the basis loader's input and not something to
 * parse for a list view.
 *
 * @author blitzcolo
 */

#pragma once

#include "../ui/PanelBase.hpp"

#include <QStringList>
#include <QVector>

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QComboBox;
class QGroupBox;
class QListWidget;
class QPushButton;
class QTableView;
class QSortFilterProxyModel;
QT_END_NAMESPACE

namespace uiplot {
class SpectrumPlotWidget;
}

class SpectralLibraryModel;

/**
 * @class SpectralLibraryPanel
 * @brief Searchable list of measured materials, with a preview and an assign
 */
class SpectralLibraryPanel : public PanelBase {
    Q_OBJECT

public:
    explicit SpectralLibraryPanel(QWidget* parent = nullptr);

    [[nodiscard]] QString panelTitle() const override;
    [[nodiscard]] QString panelId() const override { return QStringLiteral("spectral_library"); }
    void retranslateUi() override;

    /// Which scene material an assignment would land on, its name for the
    /// button caption, and the endmembers already bound to it. A negative
    /// index disables assigning.
    void setTargetMaterial(int index, const QString& name, const QStringList& currentRefs = {});

    /// How many endmembers the target carries, so the shell can decide whether
    /// adding another is still allowed.
    [[nodiscard]] int endmemberCount() const { return m_currentRefs.size(); }

    /// The (database, name) of the highlighted row, or two empty strings. The
    /// menu entries act on it, which is what keeps them equivalent to the
    /// panel's own buttons rather than a second way of choosing.
    [[nodiscard]] QPair<QString, QString> highlightedEntry() const;

    /// The curve the shell reconstructed for the highlighted row, for preview.
    /// Empty clears the plot back to its placeholder.
    void setPreviewCurve(const QVector<QPair<double, double>>& points,
                         const QString& materialName);

signals:
    /// A row was highlighted; the shell reconstructs its curve and calls
    /// setPreviewCurve. Panels do not touch the SDK.
    void previewRequested(const QString& databaseId, const QString& materialName,
                          const QString& band);

    /// Assign the highlighted material to the current target, replacing every
    /// endmember it already had.
    /// @param databaseId  "usgs", "ecostress" or "rii"
    /// @param materialName The database entry name, verbatim
    void assignRequested(const QString& databaseId, const QString& materialName);

    /// Add the highlighted material as a further endmember, keeping the ones
    /// already bound. The surface then renders as a mixture of them, in
    /// proportions unmixed from its base-colour texture.
    void endmemberAddRequested(const QString& databaseId, const QString& materialName);

    /// Drop endmember @p slot from the target. Slot 0 is the material's
    /// primary reference; removing it promotes the next one.
    void endmemberRemoveRequested(int slot);

private slots:
    void onSelectionChanged();
    void onAssignClicked();
    void onAddEndmemberClicked();
    void onRemoveEndmemberClicked();
    void onFilterChanged();

private:
    /// How many endmembers a mixture may hold. Mirrors the SDK's
    /// Material::MAX_ENDMEMBERS, which panels may not include -- this only
    /// greys out a button, and MainWindow enforces the real limit.
    static constexpr int kMaxEndmembers = 4;

    void setupUi();
    /// The row highlighted in the view, mapped through the filter, or -1.
    [[nodiscard]] int currentSourceRow() const;
    void updateAssignEnabled();
    void updateEndmemberList();

    SpectralLibraryModel* m_model = nullptr;
    QSortFilterProxyModel* m_proxy = nullptr;

    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_databaseCombo = nullptr;
    QComboBox* m_bandCombo = nullptr;
    QTableView* m_table = nullptr;
    QLabel* m_countLabel = nullptr;
    QLabel* m_targetLabel = nullptr;
    QGroupBox* m_endmemberGroup = nullptr;
    QListWidget* m_endmemberList = nullptr;
    QPushButton* m_removeEndmemberButton = nullptr;
    QPushButton* m_assignButton = nullptr;
    QPushButton* m_addEndmemberButton = nullptr;
    uiplot::SpectrumPlotWidget* m_plot = nullptr;

    int m_targetMaterial = -1;
    QString m_targetMaterialName;
    QStringList m_currentRefs;
};
