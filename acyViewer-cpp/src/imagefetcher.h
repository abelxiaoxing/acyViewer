#ifndef IMAGEFETCHER_H
#define IMAGEFETCHER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPixmap>
#include <QThread>

class ImageFetcher : public QObject
{
    Q_OBJECT

public:
    explicit ImageFetcher(const QString &apiUrl, QObject *parent = nullptr);
    void start();

signals:
    void imageFetched(const QPixmap &pixmap, const QByteArray &imageData, const QUrl &imageUrl);
    void fetchError(const QString &errorString);
    void finished();

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QString m_apiUrl;
    QNetworkAccessManager *m_manager;
};

#endif // IMAGEFETCHER_H
