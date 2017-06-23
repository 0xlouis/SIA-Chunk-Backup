#include "siacom.h"

SIACom::SIACom(QObject *parent) : QObject(parent)
{
    m_netManager = new QNetworkAccessManager(this);
    m_netRequest = new QNetworkRequest();
    m_netRequest->setRawHeader("User-Agent", "Sia-Agent");
    m_netRequest->setRawHeader("content-type", "application/x-www-form-urlencoded");

    QObject::connect(m_netManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(finished(QNetworkReply*)));
}

SIACom::~SIACom(void)
{
    delete m_netManager;
    delete m_netRequest;
}

void SIACom::finished(QNetworkReply *reply)
{
    reply->deleteLater();
}

bool SIACom::test(void)
{
    QNetworkReply *reply;
    QEventLoop loop;

    m_netRequest->setUrl(SIA_CONSENSUS);
    reply = m_netManager->get(*m_netRequest);

    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    if(reply->error() != QNetworkReply::NoError)
        return false;

    return true;
}

bool SIACom::uploadFile(const QString srcPath, const QString siaPath)
{
    QNetworkReply   *reply;
    QEventLoop      loop;
    QJsonObject     jsonObj;
    t_UploadStatus  uploadStatus;
    double          oldValue(100.0);

    //Get the file status
    uploadStatus = this->uploadFileState(siaPath);

    //Check is the file is currently in uload state
    if(uploadStatus.inUploading == false)
    {
        m_netRequest->setUrl(SIA_UPLOAD_FILE(srcPath, siaPath));
        reply = m_netManager->post(*m_netRequest, QByteArray());

        QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
        loop.exec();

        jsonObj = QJsonDocument::fromJson(reply->readAll()).object();

        if(reply->error() != QNetworkReply::NoError)
        {
            qInfo(jsonObj.value("message").toString().toUtf8());
            return false;
        }
    }
    //Check if the file is already uploaded (should never happen)
    else if(uploadStatus.isUploaded == true)
    {
        qWarning("The file is already uploaded !");
        qWarning("It isn't normal ! (Database corruption ? SHA1 collision ?)");
    }

    qInfo(QString("Uploading "+ siaPath).toUtf8());

    do
    {
        QTimer::singleShot(5000, &loop, SLOT(quit()));
        loop.exec();

        uploadStatus = this->uploadFileState(siaPath);

        if(uploadStatus.fileNotFound == true)
        {
            qWarning("Cannot found the uploading file !");
            return false;
        }

        if(uploadStatus.uploadProgress != oldValue)
        {
            qInfo("Upload progress : %.2f%%", uploadStatus.uploadProgress);
            oldValue = uploadStatus.uploadProgress;
        }
    }while(!uploadStatus.isUploaded);

    qInfo("The file was uploaded !");

    return true;
}

bool SIACom::deleteFile(const QString siaPath)
{
    QNetworkReply   *reply;
    QEventLoop      loop;

    m_netRequest->setUrl(SIA_DELETE_FILE(siaPath));
    reply = m_netManager->post(*m_netRequest, QByteArray());

    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    if(reply->error() != QNetworkReply::NoError)
        return false;

    return true;
}

t_UploadStatus SIACom::uploadFileState(const QString siaPath)
{
    QNetworkReply   *reply;
    QEventLoop      loop;
    QJsonObject     jsonObj;
    QJsonArray      jsonArray;
    t_UploadStatus  uploadStatus;

    uploadStatus.fileNotFound   = true;
    uploadStatus.inUploading    = false;
    uploadStatus.isUploaded     = false;
    uploadStatus.uploadProgress = 0.0;

    m_netRequest->setUrl(SIA_RENTER_FILES);
    reply = m_netManager->get(*m_netRequest);

    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    jsonObj = QJsonDocument::fromJson(reply->readAll()).object();

    if(reply->error() != QNetworkReply::NoError)
    {
        qInfo(jsonObj.value("message").toString().toUtf8());
        return uploadStatus;
    }

    jsonArray = jsonObj.value("files").toArray();

    foreach(QJsonValue tmp, jsonArray)
    {
        jsonObj = tmp.toObject();

        if(jsonObj.value("siapath").toString() == siaPath)
        {
            uploadStatus.fileNotFound   = false;

            uploadStatus.uploadProgress = jsonObj.value("uploadprogress").toDouble();

            if(uploadStatus.uploadProgress < 100.0)
            {
                uploadStatus.inUploading = true;
                uploadStatus.isUploaded  = false;
            }
            else
            {
                uploadStatus.inUploading = false;
                uploadStatus.isUploaded  = true;
            }

            break;
        }
    }

    return uploadStatus;
}
