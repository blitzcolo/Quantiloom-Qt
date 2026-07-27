/**
 * @file PreferencesDialog.cpp
 */

#include "PreferencesDialog.hpp"

#include "../i18n/LanguageManager.hpp"
#include "../ui/UiStyle.hpp"
#include "../ui/theme/ThemeManager.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
{
    m_styling.attach(this);
    setMinimumWidth(520);
    setupUi();
    retranslateUi();
}

void PreferencesDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);

    // --- appearance ------------------------------------------------------
    m_appearanceGroup = new QGroupBox(this);
    auto* appearanceForm = new QFormLayout(m_appearanceGroup);

    // Items are added in retranslateUi(), not here: theme names are
    // translated, so the list has to be rebuilt on every language change.
    m_themeCombo = new QComboBox(m_appearanceGroup);
    m_themeLabel = new QLabel(m_appearanceGroup);
    appearanceForm->addRow(m_themeLabel, m_themeCombo);

    mainLayout->addWidget(m_appearanceGroup);

    // --- language ------------------------------------------------------
    m_languageGroup = new QGroupBox(this);
    auto* languageForm = new QFormLayout(m_languageGroup);

    m_languageCombo = new QComboBox(m_languageGroup);
    for (const auto& language : LanguageManager::availableLanguages()) {
        m_languageCombo->addItem(language.nativeName, language.locale);
    }
    m_languageLabel = new QLabel(m_languageGroup);
    languageForm->addRow(m_languageLabel, m_languageCombo);

    mainLayout->addWidget(m_languageGroup);

    // --- capture -------------------------------------------------------
    m_captureGroup = new QGroupBox(this);
    auto* captureForm = new QFormLayout(m_captureGroup);

    auto* pathRow = new QHBoxLayout();
    m_pathEdit = new QLineEdit(m_captureGroup);
    m_pathEdit->setPlaceholderText(defaultScreenshotPath());
    m_browseButton = new QPushButton(m_captureGroup);
    connect(m_browseButton, &QPushButton::clicked, this, &PreferencesDialog::onBrowse);
    pathRow->addWidget(m_pathEdit);
    pathRow->addWidget(m_browseButton);

    m_pathLabel = new QLabel(m_captureGroup);
    captureForm->addRow(m_pathLabel, pathRow);

    m_captureInfo = new QLabel(m_captureGroup);
    m_styling.bind([this] { uistyle::applyHintStyle(m_captureInfo); });
    captureForm->addRow(m_captureInfo);

    m_restoreButton = new QPushButton(m_captureGroup);
    connect(m_restoreButton, &QPushButton::clicked,
            this, &PreferencesDialog::onRestoreDefaults);
    captureForm->addRow(m_restoreButton);

    mainLayout->addWidget(m_captureGroup);
    mainLayout->addStretch();

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(m_buttonBox);
}

void PreferencesDialog::retranslateUi() {
    setWindowTitle(tr("Preferences"));

    m_appearanceGroup->setTitle(tr("Appearance"));
    m_themeLabel->setText(tr("Theme:"));
    // Theme names are translated, so the list is refilled rather than
    // relabelled. Restore the selection by value: restoring by index would
    // silently change the user's theme if the list ever reorders.
    const QString selectedTheme = m_themeCombo->currentData().toString();
    m_themeCombo->clear();
    for (const auto& theme : ThemeManager::availableThemes()) {
        m_themeCombo->addItem(ThemeManager::displayName(theme.id), theme.id);
    }
    setSelectedThemeId(selectedTheme.isEmpty()
                           ? ThemeManager::instance().currentThemeId()
                           : selectedTheme);

    m_languageGroup->setTitle(tr("Language"));
    m_languageLabel->setText(tr("Interface language:"));

    m_captureGroup->setTitle(tr("Screenshots and image export"));
    m_pathLabel->setText(tr("Save location:"));
    m_browseButton->setText(tr("Browse..."));
    m_restoreButton->setText(tr("Restore Defaults"));
    m_captureInfo->setText(tr(
        "Screenshots are written as a matching pair, named YYYY-MM-DD_HH-MM-SS-mmm:\n"
        "• EXR — the displayed image at full precision, including display enhancement\n"
        "• PNG — an 8-bit sRGB preview\n"
        "File → Export Image writes the unenhanced render instead."));
}

void PreferencesDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    } else if (uistyle::isThemeChangeEvent(event)) {
        m_styling.reapply();
    }
    QDialog::changeEvent(event);
}

QString PreferencesDialog::selectedLocale() const {
    return m_languageCombo->currentData().toString();
}

void PreferencesDialog::setSelectedLocale(const QString& locale) {
    const int index = m_languageCombo->findData(locale);
    if (index >= 0) {
        m_languageCombo->setCurrentIndex(index);
    }
}

QString PreferencesDialog::selectedThemeId() const {
    return m_themeCombo->currentData().toString();
}

void PreferencesDialog::setSelectedThemeId(const QString& id) {
    const int index = m_themeCombo->findData(id);
    if (index >= 0) {
        m_themeCombo->setCurrentIndex(index);
    }
}

QString PreferencesDialog::defaultScreenshotPath() {
#ifdef Q_OS_WIN
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return QDir(tempDir).filePath(QStringLiteral("Quantiloom/screenshots"));
#else
    return QStringLiteral("/tmp/Quantiloom/screenshots");
#endif
}

QString PreferencesDialog::screenshotPath() const {
    const QString path = m_pathEdit->text().trimmed();
    return path.isEmpty() ? defaultScreenshotPath() : path;
}

void PreferencesDialog::setScreenshotPath(const QString& path) {
    m_pathEdit->setText(path);
}

void PreferencesDialog::onBrowse() {
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select screenshot save location"),
        screenshotPath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        m_pathEdit->setText(dir);
    }
}

void PreferencesDialog::onRestoreDefaults() {
    m_pathEdit->clear();   // empty means "use the platform default"
}
