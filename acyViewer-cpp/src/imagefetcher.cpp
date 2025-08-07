#include "imagefetcher.h"
#include <QNetworkRequest>
#include <QDebug>

ImageFetcher::ImageFetcher(const QString &apiUrl, QObject *parent)
    : QObject(parent), m_apiUrl(apiUrl)
{
    m_manager = new QNetworkAccessManager(this);
    connect(m_manager, &QNetworkAccessManager::finished, this, &ImageFetcher::onReplyFinished);
}

void ImageFetcher::start()
{
    QNetworkRequest request(m_apiUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");
    m_manager->get(request);
}

void ImageFetcher::onReplyFinished(QNetworkReply *reply)
{
    if (reply->error()) {
        emit fetchError(reply->errorString());
    } else {
        QByteArray imageData = reply->readAll();
        QPixmap pixmap;
        if (pixmap.loadFromData(imageData)) {
            emit imageFetched(pixmap, imageData, reply->url());
        } else {
            emit fetchError("Failed to load image data.");
        }
    }
    reply->deleteLater();
    emit finished();
}
