/**
 * @file PreferencesDialog.hpp
 * @brief Application-level preferences
 *
 * Replaces the old Settings → Properties… entry, which opened a dialog titled
 * "Settings" containing one screenshot path — three different names for the
 * same thing, and the language switch living in a separate menu elsewhere.
 * Everything that is a property of the application rather than of the scene is
 * here, reached from Edit → Preferences as desktop convention expects.
 *
 * Language and theme both take effect immediately on OK; there is nothing to
 * warn about and no restart to ask for.
 */

#pragma once

#include "../ui/UiStyle.hpp"

#include <QDialog>

QT_BEGIN_NAMESPACE
class QComboBox;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
QT_END_NAMESPACE

class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);

    /// Locale selected in the dialog ("en", "zh_CN").
    [[nodiscard]] QString selectedLocale() const;
    void setSelectedLocale(const QString& locale);

    /// Theme selected in the dialog ("blender-dark", "classic", "windows11").
    [[nodiscard]] QString selectedThemeId() const;
    void setSelectedThemeId(const QString& id);

    [[nodiscard]] QString screenshotPath() const;
    void setScreenshotPath(const QString& path);

    /// The platform default, also used as the line edit's placeholder.
    [[nodiscard]] static QString defaultScreenshotPath();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onBrowse();
    void onRestoreDefaults();

private:
    void setupUi();
    void retranslateUi();

    QGroupBox* m_appearanceGroup = nullptr;
    QLabel* m_themeLabel = nullptr;
    QComboBox* m_themeCombo = nullptr;

    QGroupBox* m_languageGroup = nullptr;
    QLabel* m_languageLabel = nullptr;
    QComboBox* m_languageCombo = nullptr;

    QGroupBox* m_captureGroup = nullptr;
    QLabel* m_pathLabel = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    QLabel* m_captureInfo = nullptr;
    QPushButton* m_restoreButton = nullptr;

    QDialogButtonBox* m_buttonBox = nullptr;

    uistyle::StyleBindings m_styling;
};
