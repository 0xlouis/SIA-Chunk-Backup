#ifndef SIACOM_H
#define SIACOM_H

#include "config.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

#define SIA_BASE_URL                QString("http://"+Config::getSiaIpAdrress()+":"+Config::getSiaPort())
#define SIA_CONSENSUS               QUrl(SIA_BASE_URL+"/consensus")
#define SIA_RENTER_FILES            QUrl(SIA_BASE_URL+"/renter/files")
#define SIA_UPLOAD_FILE(SRC, DST)   QUrl(SIA_BASE_URL+"/renter/upload/"+DST+"?source="+SRC)
#define SIA_DELETE_FILE(DST)        QUrl(SIA_BASE_URL+"/renter/delete/"+DST)

struct t_UploadStatus
{
    bool    fileNotFound;
    bool    inUploading;
    bool    isUploaded;
    double  uploadProgress;
};

class SIACom : public QObject
{
    Q_OBJECT
public:
    explicit SIACom(QObject *parent = 0);
    ~SIACom(void);
    bool test(void);
    bool uploadFile(const QString srcPath, const QString siaPath);
    t_UploadStatus uploadFileState(const QString siaPath);
    bool deleteFile(const QString siaPath);
private slots:
    void finished(QNetworkReply *reply);
private:
    QNetworkAccessManager *m_netManager;
    QNetworkRequest       *m_netRequest;
};

#endif // SIACOM_H
