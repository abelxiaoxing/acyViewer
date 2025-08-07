#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;
class QDialogButtonBox;
QT_END_NAMESPACE

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    QString apiUrl() const;
    int maxCacheSize() const;
    QString downloadDir() const;
    QString theme() const;

private slots:
    void browseDownloadDir();

private:
    QLineEdit *m_apiUrlEdit;
    QSpinBox *m_cacheSizeSpinBox;
    QLineEdit *m_downloadDirEdit;
    QComboBox *m_themeCombo;
    QDialogButtonBox *m_buttonBox;
};

#endif // SETTINGSDIALOG_H
