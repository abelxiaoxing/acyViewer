#include "settingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QSettings>
#include <QStandardPaths>
#include <QFileDialog>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Settings");

    QSettings settings;

    QFormLayout *formLayout = new QFormLayout;

    m_apiUrlEdit = new QLineEdit(settings.value("api_url", "https://www.acy.moe/api/r18").toString());
    formLayout->addRow("API URL:", m_apiUrlEdit);

    m_cacheSizeSpinBox = new QSpinBox;
    m_cacheSizeSpinBox->setRange(1, 20);
    m_cacheSizeSpinBox->setValue(settings.value("max_cache_size", 5).toInt());
    formLayout->addRow("Max Cache Size:", m_cacheSizeSpinBox);

    QHBoxLayout *downloadPathLayout = new QHBoxLayout;
    m_downloadDirEdit = new QLineEdit(settings.value("download_dir", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString());
    QPushButton *browseButton = new QPushButton("Browse...");
    connect(browseButton, &QPushButton::clicked, this, &SettingsDialog::browseDownloadDir);
    downloadPathLayout->addWidget(m_downloadDirEdit);
    downloadPathLayout->addWidget(browseButton);
    formLayout->addRow("Default Download Directory:", downloadPathLayout);

    m_themeCombo = new QComboBox;
    m_themeCombo->addItems({"Light", "Dark"});
    m_themeCombo->setCurrentText(settings.value("theme", "Dark").toString());
    formLayout->addRow("Theme:", m_themeCombo);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(m_buttonBox);
}

QString SettingsDialog::apiUrl() const
{
    return m_apiUrlEdit->text();
}

int SettingsDialog::maxCacheSize() const
{
    return m_cacheSizeSpinBox->value();
}

QString SettingsDialog::downloadDir() const
{
    return m_downloadDirEdit->text();
}

QString SettingsDialog::theme() const
{
    return m_themeCombo->currentText();
}

void SettingsDialog::browseDownloadDir()
{
    QString directory = QFileDialog::getExistingDirectory(this, "Select Download Directory", m_downloadDirEdit->text());
    if (!directory.isEmpty()) {
        m_downloadDirEdit->setText(directory);
    }
}
