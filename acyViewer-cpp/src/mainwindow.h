#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPixmap>
#include <QQueue>
#include <QVector>
#include <QUrl>

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QMovie;
class QSettings;
QT_END_NAMESPACE

class ImageFetcher;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void showNextImage();
    void showPreviousImage();
    void downloadCurrentImage();
    void copyImageToClipboard();
    void openSettingsDialog();

    void onImageFetched(const QPixmap &pixmap, const QByteArray &imageData, const QUrl &imageUrl);
    void onFetchError(const QString &errorString);

private:
    void loadSettings();
    void saveSettings();
    void initUI();
    void createActions();
    void createMenus();
    void applyTheme();
    void fillCache();
    void startLoadingAnimation();
    void stopLoadingAnimation();
    void displayImage(const QPixmap &pixmap, const QByteArray &imageData, const QUrl &imageUrl);
    QPixmap scaleCropPixmap(const QPixmap &pixmap, const QSize &targetSize);
    QPixmap fitPixmapNoCrop(const QPixmap &pixmap, const QSize &targetSize);

    QLabel *m_imageLabel;
    QPushButton *m_nextButton;
    QPushButton *m_downloadButton;
    QPushButton *m_copyButton;

    QMovie *m_loadingMovie;

    QSettings *m_settings;
    QString m_apiUrl;
    int m_maxCacheSize;
    QString m_downloadDir;
    QString m_currentTheme;

    struct ImageInfo {
        QPixmap pixmap;
        QByteArray imageData;
        QUrl imageUrl;
    };

    QQueue<ImageInfo> m_imageCache;
    QVector<ImageInfo> m_history;
    int m_currentHistoryIndex;

    QVector<ImageFetcher*> m_fetchers;
};
#endif // MAINWINDOW_H
