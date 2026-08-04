/**
 * @file SpectralLibraryPanel.cpp
 * @brief Measured-material database browser implementation
 */

#include "SpectralLibraryPanel.hpp"

#include "../ui/SpectrumPlotWidget.hpp"
#include "../ui/UiStyle.hpp"

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>

#include <algorithm>

namespace {

/// The three databases, in the order the filter offers them. The ids are the
/// suffix of the shipped file names and the value written into a config's
/// spectral_material_type as "quantiloom_<id>", so they are protocol rather
/// than display text and are never translated.
struct DatabaseInfo {
    const char* id;
    const char* csvSuffix;
};
const DatabaseInfo kDatabases[] = {
    {"usgs", "usgs"},
    {"ecostress", "ecostress"},
    {"rii", "rii"},
};

/// Split one CSV line, honouring the double quotes the material names carry --
/// "Alizarin_crimson (dk) GDS780" has no comma but plenty of entries do.
QStringList splitCsvLine(const QString& line) {
    QStringList fields;
    QString current;
    bool inQuotes = false;
    for (const QChar ch : line) {
        if (ch == QLatin1Char('"')) {
            inQuotes = !inQuotes;
        } else if (ch == QLatin1Char(',') && !inQuotes) {
            fields.append(current);
            current.clear();
        } else {
            current.append(ch);
        }
    }
    fields.append(current);
    return fields;
}

}  // namespace

// ============================================================================
// Model
// ============================================================================

/**
 * @class SpectralLibraryModel
 * @brief The merged contents of the three material_summary_*.csv files
 *
 * Read once at construction. Five thousand rows of short strings is well under
 * a megabyte, and holding them makes filtering instant; the alternative --
 * re-reading per keystroke -- would make the search box unusable.
 */
class SpectralLibraryModel : public QAbstractTableModel {
public:
    struct Entry {
        QString database;   ///< "usgs" / "ecostress" / "rii"; not translated
        QString name;       ///< The database entry name, verbatim
        QString category;   ///< USGS chapter, or the instrument for the others
        /// Per-band coverage, 0 when the database does not carry that band.
        /// USGS stops at 2.5 um, so its MWIR and LWIR are legitimately absent.
        double coverageVis = 0.0;
        double coverageNir = 0.0;
        double coverageSwir = 0.0;
    };

    enum Column { ColumnName = 0, ColumnDatabase, ColumnCategory, ColumnCoverage, ColumnCount };

    explicit SpectralLibraryModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {
        load();
    }

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override {
        return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
    }
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override {
        return parent.isValid() ? 0 : ColumnCount;
    }

    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() >= m_entries.size()) {
            return {};
        }
        const Entry& entry = m_entries.at(index.row());
        if (role == Qt::DisplayRole) {
            switch (index.column()) {
                case ColumnName:     return entry.name;
                case ColumnDatabase: return entry.database;
                case ColumnCategory: return entry.category;
                case ColumnCoverage:
                    // The widest band this entry actually covers, as a
                    // percentage. A row at 0% is in the database but has no
                    // usable spectrum for any band the browser offers.
                    return QStringLiteral("%1%").arg(
                        100.0 * std::max({entry.coverageVis, entry.coverageNir,
                                          entry.coverageSwir}),
                        0, 'f', 0);
                default: return {};
            }
        }
        if (role == Qt::ToolTipRole) {
            return QCoreApplication::translate(
                       "SpectralLibraryPanel",
                       "%1\nDatabase: %2\nCoverage — VIS %3%, NIR %4%, SWIR %5%")
                .arg(entry.name, entry.database)
                .arg(100.0 * entry.coverageVis, 0, 'f', 0)
                .arg(100.0 * entry.coverageNir, 0, 'f', 0)
                .arg(100.0 * entry.coverageSwir, 0, 'f', 0);
        }
        return {};
    }

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role) const override {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
            return {};
        }
        switch (section) {
            case ColumnName:     return QCoreApplication::translate("SpectralLibraryPanel", "Material");
            case ColumnDatabase: return QCoreApplication::translate("SpectralLibraryPanel", "Database");
            case ColumnCategory: return QCoreApplication::translate("SpectralLibraryPanel", "Category");
            case ColumnCoverage: return QCoreApplication::translate("SpectralLibraryPanel", "Coverage");
            default: return {};
        }
    }

    [[nodiscard]] const Entry* entryAt(int row) const {
        return (row >= 0 && row < m_entries.size()) ? &m_entries.at(row) : nullptr;
    }
    [[nodiscard]] bool isEmpty() const { return m_entries.isEmpty(); }
    /// Why the list is empty, for a panel that must say so rather than show a
    /// blank table.
    [[nodiscard]] const QString& loadError() const { return m_loadError; }

private:
    /// Where the baked databases are, following the same candidate order as
    /// the atmosphere model pack: the working directory first, so a Studio
    /// launched from the repo root finds the checked-in copy, then beside the
    /// executable for an installed build.
    [[nodiscard]] static QDir assetsDir() {
        const QStringList candidates{
            QDir::currentPath() + QStringLiteral("/assets/spectral"),
            QCoreApplication::applicationDirPath() + QStringLiteral("/assets/spectral"),
        };
        for (const QString& path : candidates) {
            if (QDir(path).exists()) {
                return QDir(path);
            }
        }
        return QDir(candidates.first());
    }

    void load() {
        const QDir assets = assetsDir();
        QStringList missing;

        for (const DatabaseInfo& db : kDatabases) {
            const QString path =
                assets.filePath(QStringLiteral("material_summary_%1.csv").arg(db.csvSuffix));
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                missing.append(QFileInfo(path).fileName());
                continue;
            }
            QTextStream stream(&file);
            const QString header = stream.readLine();
            const QStringList columns = splitCsvLine(header);

            // By name rather than by position: the three files share a schema
            // today, and pinning column 7 would break silently if one gained a
            // metric.
            const int nameCol = columns.indexOf(QStringLiteral("name"));
            const int categoryCol = columns.indexOf(QStringLiteral("chapter")) >= 0
                                        ? columns.indexOf(QStringLiteral("chapter"))
                                        : columns.indexOf(QStringLiteral("instrument"));
            const int visCol = columns.indexOf(QStringLiteral("VIS_coverage"));
            const int nirCol = columns.indexOf(QStringLiteral("NIR_coverage"));
            const int swirCol = columns.indexOf(QStringLiteral("SWIR_coverage"));
            if (nameCol < 0) {
                missing.append(QFileInfo(path).fileName());
                continue;
            }

            while (!stream.atEnd()) {
                const QString line = stream.readLine();
                if (line.trimmed().isEmpty()) continue;
                const QStringList fields = splitCsvLine(line);
                if (nameCol >= fields.size()) continue;

                Entry entry;
                entry.database = QString::fromLatin1(db.id);
                entry.name = fields.at(nameCol).trimmed();
                if (entry.name.isEmpty()) continue;
                if (categoryCol >= 0 && categoryCol < fields.size()) {
                    entry.category = fields.at(categoryCol).trimmed();
                }
                const auto readCoverage = [&fields](int column) {
                    return (column >= 0 && column < fields.size())
                               ? fields.at(column).toDouble() : 0.0;
                };
                entry.coverageVis = readCoverage(visCol);
                entry.coverageNir = readCoverage(nirCol);
                entry.coverageSwir = readCoverage(swirCol);
                m_entries.append(entry);
            }
        }

        if (m_entries.isEmpty()) {
            m_loadError = missing.isEmpty()
                ? QCoreApplication::translate("SpectralLibraryPanel",
                      "The spectral databases are installed but contain no entries.")
                : QCoreApplication::translate("SpectralLibraryPanel",
                      "Could not read the spectral databases: %1")
                      .arg(missing.join(QStringLiteral(", ")));
        }
    }

    QVector<Entry> m_entries;
    QString m_loadError;
};

// ============================================================================
// Panel
// ============================================================================

SpectralLibraryPanel::SpectralLibraryPanel(QWidget* parent)
    : PanelBase(parent)
{
    setObjectName(panelId());
    setupUi();
}

QString SpectralLibraryPanel::panelTitle() const {
    return tr("Spectral Library");
}

void SpectralLibraryPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    m_model = new SpectralLibraryModel(this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);   // name, database and category all match

    // --- search and filters --------------------------------------------
    auto* filterRow = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &SpectralLibraryPanel::onFilterChanged);
    filterRow->addWidget(m_searchEdit, 1);

    m_databaseCombo = new QComboBox(this);
    // The empty first entry is "all"; the rest carry their protocol id as data
    // so the display text can be translated without changing what is matched.
    m_databaseCombo->addItem(QString(), QString());
    for (const DatabaseInfo& db : kDatabases) {
        m_databaseCombo->addItem(QString::fromLatin1(db.id), QString::fromLatin1(db.id));
    }
    connect(m_databaseCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectralLibraryPanel::onFilterChanged);
    filterRow->addWidget(m_databaseCombo);
    mainLayout->addLayout(filterRow);

    // --- the list -------------------------------------------------------
    m_table = new QTableView(this);
    m_table->setModel(m_proxy);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(true);
    m_table->sortByColumn(SpectralLibraryModel::ColumnName, Qt::AscendingOrder);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(
        SpectralLibraryModel::ColumnName, QHeaderView::Stretch);
    m_table->setAlternatingRowColors(true);
    m_table->setMinimumHeight(160);
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &SpectralLibraryPanel::onSelectionChanged);
    mainLayout->addWidget(m_table, 1);

    m_countLabel = new QLabel(this);
    bindStyle([this] { uistyle::applyHintStyle(m_countLabel); });
    mainLayout->addWidget(m_countLabel);

    // --- preview --------------------------------------------------------
    auto* previewGroup = new QGroupBox(this);
    auto* previewLayout = new QVBoxLayout(previewGroup);

    auto* bandRow = new QHBoxLayout();
    auto* bandCaption = new QLabel(previewGroup);
    m_bandCombo = new QComboBox(previewGroup);
    // Band names are protocol -- the core matches them literally -- and are
    // acronyms that stay Latin by the glossary in any case.
    for (const char* band : {"VIS", "NIR", "SWIR"}) {
        m_bandCombo->addItem(QString::fromLatin1(band), QString::fromLatin1(band));
    }
    connect(m_bandCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectralLibraryPanel::onSelectionChanged);
    bandRow->addWidget(bandCaption);
    bandRow->addWidget(m_bandCombo, 1);
    previewLayout->addLayout(bandRow);

    m_plot = new uiplot::SpectrumPlotWidget(previewGroup);
    previewLayout->addWidget(m_plot);
    mainLayout->addWidget(previewGroup);

    // --- assign ---------------------------------------------------------
    m_targetLabel = new QLabel(this);
    m_targetLabel->setWordWrap(true);
    bindStyle([this] { uistyle::applyHintStyle(m_targetLabel); });
    mainLayout->addWidget(m_targetLabel);

    // --- endmembers ------------------------------------------------------
    // What the target is currently made of. A surface can stand for a mixture
    // of up to Material::MAX_ENDMEMBERS measured materials, in proportions
    // read out of its own base-colour texture, which is what lets a measured
    // material keep its texture instead of rendering as one flat reflectance.
    m_endmemberGroup = new QGroupBox(this);
    auto* endmemberLayout = new QVBoxLayout(m_endmemberGroup);

    m_endmemberList = new QListWidget(m_endmemberGroup);
    m_endmemberList->setMaximumHeight(96);
    connect(m_endmemberList, &QListWidget::itemSelectionChanged,
            this, &SpectralLibraryPanel::updateAssignEnabled);
    endmemberLayout->addWidget(m_endmemberList);

    m_removeEndmemberButton = new QPushButton(m_endmemberGroup);
    m_removeEndmemberButton->setEnabled(false);
    connect(m_removeEndmemberButton, &QPushButton::clicked,
            this, &SpectralLibraryPanel::onRemoveEndmemberClicked);
    endmemberLayout->addWidget(m_removeEndmemberButton);

    mainLayout->addWidget(m_endmemberGroup);

    auto* assignRow = new QHBoxLayout();
    m_assignButton = new QPushButton(this);
    m_assignButton->setEnabled(false);
    connect(m_assignButton, &QPushButton::clicked, this, &SpectralLibraryPanel::onAssignClicked);
    assignRow->addWidget(m_assignButton, 1);

    m_addEndmemberButton = new QPushButton(this);
    m_addEndmemberButton->setEnabled(false);
    connect(m_addEndmemberButton, &QPushButton::clicked,
            this, &SpectralLibraryPanel::onAddEndmemberClicked);
    assignRow->addWidget(m_addEndmemberButton, 1);
    mainLayout->addLayout(assignRow);

    bindText([this, previewGroup, bandCaption] {
        m_searchEdit->setPlaceholderText(tr("Search %n material(s)...", "",
                                            m_proxy->sourceModel()->rowCount()));
        m_databaseCombo->setItemText(0, tr("All databases"));
        previewGroup->setTitle(tr("Reflectance preview"));
        bandCaption->setText(tr("Band:"));
        m_plot->setValueAxisTitle(tr("Reflectance"));
        m_plot->setPlaceholderText(tr("Select a material to preview its measured spectrum."));
        m_assignButton->setText(tr("Assign to Material"));
        m_assignButton->setToolTip(
            tr("Replaces the material's colour with this measured spectrum, "
               "and any endmembers already bound. Undoable, and written to the "
               "configuration on save."));
        m_endmemberGroup->setTitle(tr("Endmembers"));
        m_addEndmemberButton->setText(tr("Add Endmember"));
        m_addEndmemberButton->setToolTip(
            tr("Adds this spectrum alongside the ones already bound. The surface "
               "then renders as a mixture of them, in proportions read out of its "
               "base-colour texture -- which is how a measured material keeps its "
               "texture instead of rendering as one flat reflectance."));
        m_removeEndmemberButton->setText(tr("Remove Endmember"));
        updateEndmemberList();
        onFilterChanged();      // the count line is translated too
        updateAssignEnabled();
    });

    // A missing or unreadable database is worth saying outright: the panel
    // would otherwise be an empty table with no explanation.
    if (m_model->isEmpty()) {
        m_table->setVisible(false);
        m_countLabel->setText(m_model->loadError());
    }
}

void SpectralLibraryPanel::retranslateUi() {
    PanelBase::retranslateUi();
}

void SpectralLibraryPanel::setTargetMaterial(int index, const QString& name,
                                             const QStringList& currentRefs) {
    m_targetMaterial = index;
    m_targetMaterialName = name;
    m_currentRefs = currentRefs;
    updateEndmemberList();
    updateAssignEnabled();
}

void SpectralLibraryPanel::updateEndmemberList() {
    m_endmemberList->clear();
    for (int i = 0; i < m_currentRefs.size(); ++i) {
        // Numbered from 1: the list is what the surface is made of, and
        // "endmember 0" is an implementation detail of where it is stored.
        m_endmemberList->addItem(tr("%1. %2").arg(i + 1).arg(m_currentRefs.at(i)));
    }
    if (m_currentRefs.isEmpty()) {
        auto* placeholder = new QListWidgetItem(tr("None -- this material renders from its colour"));
        placeholder->setFlags(Qt::NoItemFlags);
        m_endmemberList->addItem(placeholder);
    }
}

void SpectralLibraryPanel::updateAssignEnabled() {
    const bool haveRow = currentSourceRow() >= 0;
    const bool haveTarget = m_targetMaterial >= 0;
    const bool roomForMore = m_currentRefs.size() < kMaxEndmembers;

    m_assignButton->setEnabled(haveRow && haveTarget);
    // Adding needs somewhere to add to: a material with nothing bound gets its
    // first spectrum through Assign, which is also the only button that can
    // clear a mixture.
    m_addEndmemberButton->setEnabled(haveRow && haveTarget && !m_currentRefs.isEmpty() &&
                                     roomForMore);

    const int selected = m_endmemberList->currentRow();
    m_removeEndmemberButton->setEnabled(haveTarget && !m_currentRefs.isEmpty() &&
                                        selected >= 0 && selected < m_currentRefs.size());

    if (!haveTarget) {
        m_targetLabel->setText(tr("Select a material in the scene to assign to."));
    } else if (m_currentRefs.size() >= kMaxEndmembers) {
        m_targetLabel->setText(tr("Assigning to: %1 (at the limit of %n endmember(s))", "",
                                  kMaxEndmembers).arg(m_targetMaterialName));
    } else {
        m_targetLabel->setText(tr("Assigning to: %1").arg(m_targetMaterialName));
    }
}

void SpectralLibraryPanel::setPreviewCurve(const QVector<QPair<double, double>>& points,
                                           const QString& materialName) {
    if (points.isEmpty()) {
        m_plot->clear();
        return;
    }
    uiplot::Series series;
    series.name = materialName;
    series.points = points;
    series.colourIndex = 2;   // green: this is a reflectance, not an illuminant
    m_plot->setSeries({series});
}

int SpectralLibraryPanel::currentSourceRow() const {
    const auto rows = m_table->selectionModel()
                          ? m_table->selectionModel()->selectedRows()
                          : QModelIndexList();
    if (rows.isEmpty()) {
        return -1;
    }
    return m_proxy->mapToSource(rows.first()).row();
}

void SpectralLibraryPanel::onSelectionChanged() {
    updateAssignEnabled();
    const int row = currentSourceRow();
    const auto* entry = m_model->entryAt(row);
    if (!entry) {
        m_plot->clear();
        return;
    }
    // The shell owns the SDK call; this panel only says what it wants to see.
    emit previewRequested(entry->database, entry->name, m_bandCombo->currentData().toString());
}

void SpectralLibraryPanel::onAssignClicked() {
    const auto* entry = m_model->entryAt(currentSourceRow());
    if (!entry || m_targetMaterial < 0) {
        return;
    }
    emit assignRequested(entry->database, entry->name);
}

QPair<QString, QString> SpectralLibraryPanel::highlightedEntry() const {
    const auto* entry = m_model->entryAt(currentSourceRow());
    return entry ? qMakePair(entry->database, entry->name) : qMakePair(QString(), QString());
}

void SpectralLibraryPanel::onAddEndmemberClicked() {
    const auto* entry = m_model->entryAt(currentSourceRow());
    if (!entry || m_targetMaterial < 0 || m_currentRefs.size() >= kMaxEndmembers) {
        return;
    }
    emit endmemberAddRequested(entry->database, entry->name);
}

void SpectralLibraryPanel::onRemoveEndmemberClicked() {
    const int slot = m_endmemberList->currentRow();
    if (m_targetMaterial < 0 || slot < 0 || slot >= m_currentRefs.size()) {
        return;
    }
    emit endmemberRemoveRequested(slot);
}

void SpectralLibraryPanel::onFilterChanged() {
    const QString database = m_databaseCombo->currentData().toString();
    const QString search = m_searchEdit->text().trimmed();

    // Both terms at once, as a regular expression over every column: the
    // database id appears in its own column, so requiring it there and the
    // search text anywhere would need two proxies.
    QString pattern = QRegularExpression::escape(search);
    if (!database.isEmpty()) {
        // Anchored on the database column's exact value, then the free text.
        m_proxy->setFilterKeyColumn(SpectralLibraryModel::ColumnDatabase);
        m_proxy->setFilterFixedString(database);
        // A second pass is not possible with one proxy, so the free text is
        // applied over the database-filtered rows by widening the key column
        // only when there is no database filter. This keeps the common cases
        // -- browse one database, or search everything -- both exact.
        if (!search.isEmpty()) {
            m_proxy->setFilterKeyColumn(-1);
            m_proxy->setFilterRegularExpression(
                QRegularExpression(QStringLiteral("(?=.*%1)(?=.*%2)")
                                       .arg(QRegularExpression::escape(database), pattern),
                                   QRegularExpression::CaseInsensitiveOption));
        }
    } else {
        m_proxy->setFilterKeyColumn(-1);
        m_proxy->setFilterFixedString(search);
    }

    if (!m_model->isEmpty()) {
        m_countLabel->setText(tr("%n material(s) shown", "", m_proxy->rowCount()));
    }
    updateAssignEnabled();
}
