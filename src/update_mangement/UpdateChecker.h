// UpdateChecker.h
#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QVersionNumber>

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent=nullptr);
    void checkNow(bool showNoUpdatesDialog = false); // call on startup & from menu

signals:
    void updateAvailable(const QString& latestVersion, const QString& notes, const QUrl& downloadUrl);

private:
    QNetworkAccessManager nam_;
    QVersionNumber currentVersion() const;
};
