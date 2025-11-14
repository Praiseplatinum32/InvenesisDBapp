// UpdateChecker.cpp
#include "UpdateChecker.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QVersionNumber>

UpdateChecker::UpdateChecker(QObject* p) : QObject(p) {}

QVersionNumber UpdateChecker::currentVersion() const {
    return QVersionNumber::fromString(QCoreApplication::applicationVersion());
}

void UpdateChecker::checkNow(bool showNoUpdatesDialog) {
    qInfo() << "I am checking";
    // Host this file on HTTPS (e.g., https://updates.invenesis.local/version.json)
    const QUrl url(QStringLiteral("https://YOUR_UPDATE_HOST/version.json"));
    QNetworkRequest req(url);
    auto* reply = nam_.get(req);
    connect(reply, &QNetworkReply::finished, this, [=, this](){
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (showNoUpdatesDialog) emit updateAvailable(QString(), QString("Network error"), QUrl());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) return;
        const auto o = doc.object();
        const auto latest = QVersionNumber::fromString(o.value("latest_version").toString());
        const auto notes  = o.value("release_notes").toString();
        const auto url    = QUrl(o.value("download_url").toString());
        if (latest.isNull() || !url.isValid()) return;

        if (QVersionNumber::compare(latest, currentVersion()) > 0) {
            emit updateAvailable(latest.toString(), notes, url);
        } else if (showNoUpdatesDialog) {
            emit updateAvailable(QString(), QString(), QUrl()); // no update
        }
    });
}
