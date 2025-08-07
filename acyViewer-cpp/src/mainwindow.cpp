#include "mainwindow.h"
#include "settingsdialog.h"
#include "imagefetcher.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QKeyEvent>
#include <QStandardPaths>
#include <QDir>
#include <QMovie>
#include <QSettings>
#include <QMenuBar>
#include <QStatusBar>
#include <QStyle>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_currentHistoryIndex(-1)
{
    loadSettings();
    initUI();
    createActions();
    createMenus();
    applyTheme();
    fillCache();
}

MainWindow::~MainWindow()
{
    for (auto fetcher : m_fetchers) {
        fetcher->deleteLater();
    }
}

void MainWindow::loadSettings()
{
    m_settings = new QSettings(this);
    m_apiUrl = m_settings->value("api_url", "https://www.acy.moe/api/r18").toString();
    m_maxCacheSize = m_settings->value("max_cache_size", 5).toInt();
    m_downloadDir = m_settings->value("download_dir", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
    if (!QDir(m_downloadDir).exists()) {
        m_downloadDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        m_settings->setValue("download_dir", m_downloadDir);
    }
    m_currentTheme = m_settings->value("theme", "Dark").toString();
}

void MainWindow::saveSettings()
{
    m_settings->setValue("api_url", m_apiUrl);
    m_settings->setValue("max_cache_size", m_maxCacheSize);
    m_settings->setValue("download_dir", m_downloadDir);
    m_settings->setValue("theme", m_currentTheme);
}

void MainWindow::initUI()
{
    setWindowTitle("acyViewer");
    setGeometry(100, 100, 850, 650);
    setMinimumSize(600, 450);

    QWidget *centralWidget = new QWidget;
    centralWidget->setObjectName("centralWidget");
    setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_imageLabel = new QLabel("Loading image...");
    m_imageLabel->setObjectName("imageLabel");
    m_imageLabel->setAlignment(Qt::AlignCenter);

    mainLayout->addWidget(m_imageLabel, 1);

    m_loadingMovie = new QMovie(":/res/loading.gif");
    m_loadingMovie->setScaledSize(QSize(80, 80));

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(10);

    m_nextButton = new QPushButton("Next");
    m_nextButton->setIcon(style()->standardIcon(QStyle::SP_MediaSeekForward));
    m_nextButton->setToolTip("Show next image (Space / D)");
    connect(m_nextButton, &QPushButton::clicked, this, &MainWindow::showNextImage);
    buttonLayout->addWidget(m_nextButton);

    m_downloadButton = new QPushButton("Download");
    m_downloadButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    m_downloadButton->setToolTip("Download current image (Ctrl+S)");
    m_downloadButton->setEnabled(false);
    connect(m_downloadButton, &QPushButton::clicked, this, &MainWindow::downloadCurrentImage);
    buttonLayout->addWidget(m_downloadButton);

    m_copyButton = new QPushButton("Copy");
    m_copyButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogContentsView));
    m_copyButton->setToolTip("Copy current image to clipboard");
    m_copyButton->setEnabled(false);
    connect(m_copyButton, &QPushButton::clicked, this, &MainWindow::copyImageToClipboard);
    buttonLayout->addWidget(m_copyButton);

    buttonLayout->addStretch(1);
    mainLayout->addLayout(buttonLayout);

    statusBar()->showMessage("Ready | Shortcuts: A, Space/D, Ctrl+S, Ctrl+,");
}

void MainWindow::createActions()
{
    // Actions are created and connected in createMenus()
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu("File");
    QAction *settingsAction = fileMenu->addAction("Settings...");
    settingsAction->setShortcut(QKeySequence("Ctrl+,"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettingsDialog);

    QAction *exitAction = fileMenu->addAction("Exit");
    exitAction->setShortcut(QKeySequence("Ctrl+Q"));
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *navigateMenu = menuBar()->addMenu("Navigate");
    QAction *prevAction = navigateMenu->addAction("Previous");
    prevAction->setShortcut(QKeySequence(Qt::Key_A));
    connect(prevAction, &QAction::triggered, this, &MainWindow::showPreviousImage);

    QAction *nextAction = navigateMenu->addAction("Next");
    nextAction->setShortcut(QKeySequence(Qt::Key_Space));
    connect(nextAction, &QAction::triggered, this, &MainWindow::showNextImage);
}

void MainWindow::applyTheme()
{
    if (m_currentTheme == "Dark") {
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(45, 45, 45));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(30, 30, 30));
        darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ToolTipBase, QColor(45, 45, 45));
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor(60, 60, 60));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        qApp->setPalette(darkPalette);

        QString darkStyleSheet = R"(
            #centralWidget { background-color: #2D2D2D; }
            QLabel#imageLabel { background-color: transparent; border: none; }
            QPushButton { background-color: #4A4A4A; color: white; border: none; padding: 8px 16px; border-radius: 4px; font-size: 10pt; min-height: 28px; }
            QPushButton:hover { background-color: #5A5A5A; }
            QPushButton:pressed { background-color: #3A3A3A; }
            QPushButton:disabled { background-color: #383838; color: #777777; }
            QMenuBar { background-color: #3C3C3C; color: #E0E0E0; }
            QMenuBar::item { background-color: transparent; padding: 4px 8px; }
            QMenuBar::item:selected { background-color: #5A5A5A; }
            QMenu { background-color: #3C3C3C; color: #E0E0E0; border: 1px solid #5A5A5A; padding: 4px; }
            QMenu::item { padding: 4px 20px; }
            QMenu::item:selected { background-color: #5A5A5A; }
            QMenu::separator { height: 1px; background: #5A5A5A; margin-left: 5px; margin-right: 5px; }
            QStatusBar { background-color: #3C3C3C; color: #B0B0B0; }
            QToolTip { color: #E0E0E0; background-color: #5A5A5A; border: 1px solid #6A6A6A; border-radius: 3px; padding: 4px; }
            QDialog { background-color: #2D2D2D; }
        )";
        qApp->setStyleSheet(darkStyleSheet);
    } else {
        qApp->setPalette(style()->standardPalette());
        qApp->setStyleSheet("");
    }
}

void MainWindow::fillCache()
{
    int needed = m_maxCacheSize - m_imageCache.size() - m_fetchers.size();
    for (int i = 0; i < needed; ++i) {
        ImageFetcher *fetcher = new ImageFetcher(m_apiUrl);
        // Move fetcher to a new thread to avoid blocking the UI
        QThread* thread = new QThread();
        fetcher->moveToThread(thread);
        connect(thread, &QThread::started, fetcher, &ImageFetcher::start);
        connect(fetcher, &ImageFetcher::imageFetched, this, &MainWindow::onImageFetched);
        connect(fetcher, &ImageFetcher::fetchError, this, &MainWindow::onFetchError);
        connect(fetcher, &ImageFetcher::finished, thread, &QThread::quit);
        connect(fetcher, &ImageFetcher::finished, fetcher, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        

        connect(fetcher, &QObject::destroyed, this, [this, fetcher]() {
            m_fetchers.removeOne(fetcher);
        });

        m_fetchers.append(fetcher);
        thread->start();
    }
}

void MainWindow::startLoadingAnimation()
{
    m_imageLabel->setMovie(m_loadingMovie);
    m_loadingMovie->start();
    m_imageLabel->setAlignment(Qt::AlignCenter);
}

void MainWindow::stopLoadingAnimation()
{
    m_loadingMovie->stop();
    m_imageLabel->setMovie(nullptr);
}

// 根据目标 QLabel 大小，保持纵横比放大并裁剪到完全填充，同时考虑 HiDPI
QPixmap MainWindow::fitPixmapNoCrop(const QPixmap &pixmap, const QSize &labelLogicalSize)
{
    if (pixmap.isNull() || labelLogicalSize.isEmpty()) {
        return pixmap;
    }
    const qreal dpr = pixmap.devicePixelRatio();
    QSize targetPx = labelLogicalSize * dpr;

    QPixmap scaled = pixmap.scaled(targetPx,
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    return scaled;
}

// 保留旧方法便于切换
QPixmap MainWindow::scaleCropPixmap(const QPixmap &pixmap, const QSize &labelLogicalSize)
{
    if (pixmap.isNull() || labelLogicalSize.isEmpty()) {
        return pixmap;
    }

    const qreal dpr = pixmap.devicePixelRatio();
    QSize targetPx = labelLogicalSize * dpr;


    QPixmap scaled = pixmap.scaled(targetPx,
                                   Qt::KeepAspectRatioByExpanding,
                                   Qt::SmoothTransformation);


    int x = (scaled.width()  - targetPx.width())  / 2;
    int y = (scaled.height() - targetPx.height()) / 2;

    QPixmap cropped = scaled.copy(x, y, targetPx.width(), targetPx.height());

    cropped.setDevicePixelRatio(dpr);
    return cropped;
}

void MainWindow::displayImage(const QPixmap &pixmap, const QByteArray &imageData, const QUrl &imageUrl)
{
    stopLoadingAnimation();
        m_imageLabel->setPixmap(fitPixmapNoCrop(pixmap, m_imageLabel->size()));
    m_imageLabel->setAlignment(Qt::AlignCenter);

    m_downloadButton->setEnabled(true);
    m_copyButton->setEnabled(true);
}

void MainWindow::showNextImage()
{
    if (m_currentHistoryIndex != -1 && m_currentHistoryIndex < m_history.size() - 1) {
        m_currentHistoryIndex++;
        const auto &info = m_history[m_currentHistoryIndex];
        displayImage(info.pixmap, info.imageData, info.imageUrl);
        statusBar()->showMessage(QString("History: %1/%2").arg(m_currentHistoryIndex + 1).arg(m_history.size()));
    } else if (!m_imageCache.isEmpty()) {
        ImageInfo info = m_imageCache.dequeue();
        displayImage(info.pixmap, info.imageData, info.imageUrl);

        if (m_currentHistoryIndex == -1 ||
            (m_currentHistoryIndex < m_history.size() - 1 && m_history[m_currentHistoryIndex + 1].imageUrl != info.imageUrl) ||
            (m_currentHistoryIndex == m_history.size() - 1 && (m_history.isEmpty() || m_history.last().imageUrl != info.imageUrl)))
        {
            if (m_currentHistoryIndex != -1 && m_currentHistoryIndex < m_history.size() - 1) {
                m_history.remove(m_currentHistoryIndex + 1, m_history.size() - (m_currentHistoryIndex + 1));
            }
            m_history.append(info);
            m_currentHistoryIndex = m_history.size() - 1;
        }

        statusBar()->showMessage(QString("Showing from cache. Cache size: %1/%2").arg(m_imageCache.size()).arg(m_maxCacheSize));
        fillCache();
    } else {
        startLoadingAnimation();
        statusBar()->showMessage("Cache is empty, fetching new image...");
        fillCache();
        if (m_fetchers.empty() && m_imageCache.empty()) {
            QMessageBox::information(this, "Info", "All image sources failed or cache is empty.\nPlease check your network connection and API URL settings.");
            stopLoadingAnimation();
            m_imageLabel->setText("No available images.");
        }
    }
}

void MainWindow::showPreviousImage()
{
    if (!m_history.isEmpty() && m_currentHistoryIndex > 0) {
        m_currentHistoryIndex--;
        const auto &info = m_history[m_currentHistoryIndex];
        displayImage(info.pixmap, info.imageData, info.imageUrl);
        statusBar()->showMessage(QString("History: %1/%2").arg(m_currentHistoryIndex + 1).arg(m_history.size()));
    } else if (!m_history.isEmpty() && m_currentHistoryIndex == 0) {
        statusBar()->showMessage("This is the first image in history.");
    } else {
        statusBar()->showMessage("No more history.");
    }
}

void MainWindow::downloadCurrentImage()
{
    if (m_currentHistoryIndex == -1 || m_history.isEmpty()) {
        QMessageBox::warning(this, "Download Failed", "No current image to download.");
        return;
    }

    const auto &info = m_history[m_currentHistoryIndex];
    QString filename = QFileInfo(info.imageUrl.path()).fileName();
    if (filename.isEmpty()) {
        filename = "image.png";
    }

    QString filePath = QFileDialog::getSaveFileName(this, "Save Image",
                                                    QDir(m_downloadDir).filePath(filename),
                                                    "Image Files (*.png *.jpg *.jpeg *.gif *.bmp *.webp)");

    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(info.imageData);
            file.close();
            statusBar()->showMessage(QString("Image saved to: %1").arg(filePath));
        } else {
            QMessageBox::critical(this, "Save Failed", "Could not save image.");
        }
    }
}

void MainWindow::copyImageToClipboard()
{
    if (m_currentHistoryIndex != -1 && !m_history.isEmpty()) {
        QApplication::clipboard()->setImage(m_history[m_currentHistoryIndex].pixmap.toImage());
        statusBar()->showMessage("Image copied to clipboard.");
    } else {
        statusBar()->showMessage("No image to copy.");
    }
}

void MainWindow::openSettingsDialog()
{
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString oldApiUrl = m_apiUrl;
        int oldMaxCache = m_maxCacheSize;
        QString oldTheme = m_currentTheme;

        m_apiUrl = dialog.apiUrl();
        m_maxCacheSize = dialog.maxCacheSize();
        m_downloadDir = dialog.downloadDir();
        m_currentTheme = dialog.theme();

        saveSettings();

        if (m_currentTheme != oldTheme) {
            applyTheme();
        }

        if (m_apiUrl != oldApiUrl || m_maxCacheSize != oldMaxCache) {
            statusBar()->showMessage("Settings updated, re-initializing cache...");
            // Disconnect signals and request fetchers to stop
            for (ImageFetcher* fetcher : qAsConst(m_fetchers)) {
                fetcher->disconnect(this);
            }
            qDeleteAll(m_fetchers);
            m_fetchers.clear();
            m_imageCache.clear();
            m_history.clear();
            m_currentHistoryIndex = -1;
            m_downloadButton->setEnabled(false);
            m_copyButton->setEnabled(false);
            startLoadingAnimation();
            fillCache();
        }
        QMessageBox::information(this, "Settings", "Settings have been saved.");
        statusBar()->showMessage(QString("Settings saved. Cache: %1/%2").arg(m_imageCache.size()).arg(m_maxCacheSize));
    }
}

void MainWindow::onImageFetched(const QPixmap &pixmap, const QByteArray &imageData, const QUrl &imageUrl)
{
    if (m_imageCache.size() < m_maxCacheSize) {
        QPixmap highDpiPixmap = pixmap;
        highDpiPixmap.setDevicePixelRatio(devicePixelRatioF());
        m_imageCache.enqueue({highDpiPixmap, imageData, imageUrl});
        statusBar()->showMessage(QString("Cache success. Cache: %1/%2").arg(m_imageCache.size()).arg(m_maxCacheSize));
    }
    if (m_imageLabel->movie() == m_loadingMovie) { // If we are currently loading
        showNextImage();
    }
    fillCache();
}

void MainWindow::onFetchError(const QString &errorString)
{
    statusBar()->showMessage(QString("Error: %1").arg(errorString));
    if (m_imageLabel->movie() == m_loadingMovie && m_imageCache.isEmpty()) {
        stopLoadingAnimation();
        m_imageLabel->setText(QString("Failed to get image:\n%1").arg(errorString));
        m_imageLabel->setAlignment(Qt::AlignCenter);
    }
    fillCache();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_D) {
        showNextImage();
    } else if (event->key() == Qt::Key_A) {
        showPreviousImage();
    } else if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_S) {
        downloadCurrentImage();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    if (m_currentHistoryIndex != -1 && !m_history.isEmpty()) {
                                m_imageLabel->setPixmap(fitPixmapNoCrop(m_history[m_currentHistoryIndex].pixmap, m_imageLabel->size()));
    }
    QMainWindow::resizeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Disconnect signals to prevent further processing
    for (ImageFetcher* fetcher : qAsConst(m_fetchers)) {
        fetcher->disconnect(this);
    }


    QMainWindow::closeEvent(event);
}