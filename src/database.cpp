#include "database.h"

DataBase::DataBase(QObject *parent) : QObject(parent)
{
    m_siaCom            = new SIACom(this);
    m_archiveBuilder    = new ArchiveBuilder(this);
}

DataBase::~DataBase(void)
{
    delete m_siaCom;
    delete m_archiveBuilder;
}

bool DataBase::load(void)
{
    //Use the SQLITE V3 Driver
    m_sqlDb = QSqlDatabase::addDatabase("QSQLITE");

    //Check if the driver is valid
    if(!m_sqlDb.isValid())
        return false;

    //Setup the database with this name respectively
    m_sqlDb.setDatabaseName(QString(Config::getDatabasePath() +"/"+ Config::getDatabaseName()));

    //Open the database and look for error
    if(!m_sqlDb.open())
    {
        qCritical(m_sqlDb.lastError().text().toUtf8());
        return false;
    }

    return true;
}

void DataBase::unload(void)
{
    QSqlQuery query;

    //Clean the database
    query = m_sqlDb.exec("VACUUM;");
    qDebug(QString("Query : "+ query.executedQuery()).toUtf8());

    //Close the database
    m_sqlDb.close();
}

bool DataBase::setupDataBase(void)
{
    QSqlQuery query;

    if(!m_sqlDb.isOpen())
        return false;

    query = m_sqlDb.exec(SQL_QUERY_CREATE_TABLE_INDEX);
    qDebug(QString("Query : "+ query.executedQuery()).toUtf8());

    query = m_sqlDb.exec(SQL_QUERY_CREATE_TABLE_TEMP);
    qDebug(QString("Query : "+ query.executedQuery()).toUtf8());

    return true;
}

void DataBase::setSyncData(const t_SyncData *syncData)
{
    m_syncData.rootSrcPath = syncData->rootSrcPath;
    m_syncData.rootDstPath = syncData->rootDstPath;
}

t_SyncData DataBase::getSyncData(void) const
{
    return m_syncData;
}

void DataBase::getDirList(QStringList *dirList)
{
    QStringList subList;

    //Append the base directory in the top of the main research list
    (*dirList) << QFileInfo(m_syncData.rootSrcPath).absoluteFilePath();

    //Scan all directories recusively, the result is build in dirList
    for(int i(0); i < dirList->count(); i++)
    {
        //Search for sub directorys in current directory
        subList = QDir((*dirList)[i]).entryList(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System | QDir::CaseSensitive, QDir::Name);

        //Append the sub directorys to the main directorys list
        foreach(QString sub, subList)
            dirList->append((*dirList)[i] +"/"+ sub);
    }
}

void DataBase::buildTemporaryTable(const QString currentDir, const QStringList *fileList)
{
    t_TempTable entry;
    QFileInfo   file;
    QSqlQuery   query;

    qInfo(QString("Getting "+ QString::number(fileList->count()) +" files infos in the directory : ").toUtf8());
    qInfo(currentDir.toUtf8());

    //In the sperate by dir mode the temp database need to be wiped
    if(Config::getBackupMode() == BackupMode::SEPARTE_BY_DIR)
        this->resetTemporaryTable();

    //for each files in the current directory
    foreach(QString str_file, *fileList)
    {
        //Get file informations
        file = QFileInfo(currentDir +"/"+ str_file);

        //Pickup useful informations
        entry.source    = file.absoluteFilePath();
        entry.hash      = this->getFileHash(entry.source);
        entry.size      = file.size();

        //Store information in database
        query = m_sqlDb.exec(SQL_QUERY_INSERT_TABLE_TEMP(entry.source, entry.hash.toHex(), QString::number(entry.size)));
        qDebug(QString("Query : "+ query.executedQuery()).toUtf8());
    }
}

QByteArray DataBase::getFileHash(const QString str_file)
{
    QFile               file(str_file);
    QCryptographicHash  sha1(QCryptographicHash::Sha1);

    if(!file.open(QFile::ReadOnly))
        return QByteArray();

    if(!sha1.addData(&file))
        return QByteArray();

    return sha1.result();
}

void DataBase::syncDataBase(const QString currentDir)
{
    this->deleteProcedure(currentDir);
    this->changeProcedure(currentDir);
    this->appendProcedure(currentDir);
}

void DataBase::deleteProcedure(const QString currentDir)
{
    QLinkedList<t_IndexTable> list;
    QSqlQuery                 query;

    qInfo("Sync : Looking for missing files...");

    //Find the deleted files in the local directory
    list = this->lookForDeletedFiles(currentDir);

    //Delete from both SIA and database the entry
    foreach(t_IndexTable entry, list)
    {
        //Delete the entry from database
        this->deleteCluster(entry.cluster);

        //Delete target on SIA
        m_siaCom->deleteFile(entry.target);
    }

    qInfo(QString("Sync : Result "+QString::number(list.count())+" files removed from SIA.").toUtf8());
}

void DataBase::changeProcedure(const QString currentDir)
{
    QLinkedList<t_IndexTable> list;
    QSqlQuery                 query;

    qInfo("Sync : Looking for changes in files...");

    //Find the changed files in the local directory
    list = this->lookForChangedFiles(currentDir);

    //Delete from both SIA and database the entry
    foreach (t_IndexTable entry, list)
    {
        //Delete the entry from database
        this->deleteCluster(entry.cluster);

        //Delete target on SIA
        m_siaCom->deleteFile(entry.target);
    }

    qInfo(QString("Sync : Result "+QString::number(list.count())+" files removed from SIA.").toUtf8());
}

void DataBase::appendProcedure(const QString currentDir)
{
    t_clusterInfo clusterInfo;

    qInfo("Sync : Looking for new files...");

    //Sync the temp table with the index table, after that the remains entry in temp table is the files to upload
    this->syncTables();

    //To avoid fragmentation, the smaller cluster in this directory is delete
    if((Config::getAvoidFrag() == true) && (this->getFileCountInTempTable() > 0))
        this->deleteSmallerCluster(currentDir);

    qInfo(QString("Sync : Result "+QString::number(this->getFileCountInTempTable())+" files to upload on SIA.").toUtf8());

    //Continu if there is another files to upload
    while(this->getFileCountInTempTable() > 0)
    {
        //Form cluster
        clusterInfo = this->buildCluster(currentDir);

        qInfo("Submiting cluster to SIA");

        //upload source on SIA
        m_siaCom->uploadFile(clusterInfo.tarFile.absoluteFilePath(), clusterInfo.targetSiaName);

        qInfo("Deleting cluster from local directory");

        //Delete the temp archive
        QFile::remove(clusterInfo.tarFile.absoluteFilePath());
    }
}

t_clusterInfo DataBase::buildCluster(const QString currentDir)
{
    QSqlQuery                       query;
    QLinkedList<t_IndexTable*>      *clusterEntryList;
    QStringList                     filesToArchive;
    t_clusterInfo                   clusterInfo;

    clusterEntryList = new QLinkedList<t_IndexTable*>();

    //Get the cluster files list
    this->buildClusterFilesList(currentDir, clusterEntryList, &filesToArchive);

    //Create the cluster file with previous data
    clusterInfo = this->makeClusterFile(currentDir, clusterEntryList, &filesToArchive);

    qInfo("New cluster build !");
    qInfo("Recording in database...");

    //Record in database the new cluster and remove them from the temp table
    foreach(t_IndexTable *clusterEntry, (*clusterEntryList))
    {
        //Build the target path on SIA
        clusterInfo.targetSiaName  = m_syncData.rootDstPath;
        clusterInfo.targetSiaName += currentDir.section(m_syncData.rootSrcPath, 1);
        clusterInfo.targetSiaName += "/";
        clusterInfo.targetSiaName += clusterInfo.tarFile.fileName();

        query = m_sqlDb.exec(SQL_QUERY_INSERT_INDEX_TABLE(clusterInfo.clusterId, clusterEntry->source, clusterInfo.targetSiaName, clusterEntry->hash, QString::number(clusterEntry->size)));
        qDebug(QString("Query : "+ query.executedQuery()).toUtf8());
    }

    qInfo("Synchronizing the temporary files list with permanent database...");
    //Sync the temp table with the index table
    this->syncTables();

    //Delete the zip dir
    m_archiveBuilder->cleanMirrorDir();

    qInfo("Done !");
    qInfo("Result :");
    qInfo(QString("Cluster ID : "+ clusterInfo.clusterId +", Files : "+ QString::number(clusterEntryList->count()).toUtf8() +", Size : "+ QString::number(clusterInfo.tarFile.size())).toUtf8());

    //Unload all the entry list from memory
    while(!clusterEntryList->isEmpty())
          delete clusterEntryList->takeFirst();

    delete clusterEntryList;

    return clusterInfo;
}

void DataBase::buildClusterFilesList(const QString currentDir, QLinkedList<t_IndexTable *> *outDataList, QStringList *outStrList)
{
    QSqlQuery       query;
    int             sourceField(0), sizeField(0), hashField(0);
    int             counter(0);
    quint64         clusterSize(0), fileSize(0);
    QFileInfo       zipFile;
    t_IndexTable    *clusterEntry;

    m_archiveBuilder->setWorkingDirectory(currentDir);

    const quint64 CLUSTER_SIZE(Config::getClusterSize());

    qInfo("Selecting files to be in nesxt cluster...");

    //Order by size the temp list (facilities to build one cluster)
    query = m_sqlDb.exec(SQL_QUERY_GET_SRC_ORDER_BY_SIZE_DESC);
    qDebug(QString("Query : "+ query.executedQuery()).toUtf8());

    sourceField  = query.record().indexOf("Source");
    sizeField    = query.record().indexOf("Size");
    hashField    = query.record().indexOf("Hash");

    //In this section I don't unite all the features in one to keep the code readable

    //If compression feature is enabled
    if(Config::getUseCompression() == true)
    {
        qInfo("Creating mirror directory in compression mode... (can take a will)");

        //Read the file list in size order to converge to the max cluster size
        while((query.next()) && (clusterSize < CLUSTER_SIZE))
        {
            //Create the temporary mirror "ZIP_DIR" and compress the copy files
            zipFile  = m_archiveBuilder->createZIP(query.value(sourceField).toString());
            fileSize = zipFile.size();//Get the size of current pointed file

            //Check if the the file can be puted in the cluster without exceed is max size (expect if the file is alone)
            if(((clusterSize + fileSize) > CLUSTER_SIZE) && (!outDataList->isEmpty()))
            {
                //If the file is finally not is the list => delete it
                QFile::remove(zipFile.absoluteFilePath());

                //Record each false positive in this counter
                counter++;

                //Try again
                continue;
            }

            //Compress files take huge amount of time, so the building list is canceled if no significant progress occure
            if(counter >= MAX_FALSE_POSITIVE_IN_COMPRESION)
                break;

            //Record the current file in cluster
            clusterEntry            =  new t_IndexTable;
            clusterEntry->size      =  fileSize;
            clusterEntry->source    =  query.value(sourceField).toString();
            clusterEntry->hash      =  query.value(hashField).toString();
            (*outDataList)          << clusterEntry;
            //Update the size of current cluster
            clusterSize             += fileSize;
            //Record the file to archive in string list
            (*outStrList)           << zipFile.absoluteFilePath();
        }
    }
    else
    {
        //Read the file list in size order to converge to the max cluster size
        while((query.next()) && (clusterSize < CLUSTER_SIZE))
        {
            //Get the size of current pointed file
            fileSize = query.value(sizeField).toULongLong();

            //Check if the the file can be puted in the cluster without exceed is max size (expect if the file is alone)
            if(((clusterSize + fileSize) > CLUSTER_SIZE) && (!outDataList->isEmpty()))
                continue;

            //Record the current file in cluster
            clusterEntry            =  new t_IndexTable;
            clusterEntry->size      =  fileSize;
            clusterEntry->source    =  query.value(sourceField).toString();
            clusterEntry->hash      =  query.value(hashField).toString();
            (*outDataList)          << clusterEntry;
            //Update the size of current cluster
            clusterSize             += fileSize;
            //Record the file to archive in string list
            (*outStrList)           << clusterEntry->source;
        }
    }

    qInfo("Result : %d possibles files in next cluster", outStrList->count());
}

t_clusterInfo DataBase::makeClusterFile(const QString currentDir, QLinkedList<t_IndexTable *> *inDataList, QStringList *inStrList)
{
    t_archiveInfo   archiveInfo;
    t_clusterInfo   clusterInfo;

    const quint64   CLUSTER_SIZE(Config::getClusterSize());

    m_archiveBuilder->setWorkingDirectory(currentDir);

    //In compression mode the zipped file is stored in the temporary mirror "ZIP_DIR"
    if(Config::getUseCompression() == true)
        m_archiveBuilder->setWorkingDirectory(m_archiveBuilder->getMirrorDir());

    //Archive all the files in cluster
    archiveInfo = m_archiveBuilder->createTar("archive.tar", inStrList, CLUSTER_SIZE);

    //Delete from the list the remains files (not puted in archive according to the size limit)
    while((quint32)inStrList->count() > archiveInfo.entryCount)
    {
        inStrList->takeLast();
        delete inDataList->takeLast();
    }

    clusterInfo.tarFile = archiveInfo.archiveFile;

    //Rename the archive with an unique name
    clusterInfo.clusterId = this->getFileHash(clusterInfo.tarFile.absoluteFilePath()).toHex();
    QFile::rename(clusterInfo.tarFile.absoluteFilePath(), QString(m_archiveBuilder->getTempDir() +"/"+ clusterInfo.clusterId));
    clusterInfo.tarFile.setFile(QString(m_archiveBuilder->getTempDir() +"/"+ clusterInfo.clusterId));

    return clusterInfo;
}

void DataBase::resetTemporaryTable(void)
{
    QSqlQuery   query;

    query = m_sqlDb.exec(SQL_QUERY_DROP_TABLE_TEMP);
    qDebug(QString("Query : "+ query.executedQuery()).toUtf8());
    query = m_sqlDb.exec(SQL_QUERY_CREATE_TABLE_TEMP);
    qDebug(QString("Query : "+ query.executedQuery()).toUtf8());
}

QLinkedList<t_IndexTable> DataBase::lookForDeletedFiles(const QString dir)
{
    QLinkedList<t_IndexTable> list;
    t_IndexTable     entry;
    QSqlQuery        query;
    int              targetField, clusterField;

    if(Config::getBackupMode() == BackupMode::SEPARTE_BY_DIR)
    {
        //Search for new file in temp_table point view (not recursive)
        query = m_sqlDb.exec(SQL_QUERY_LOOK_FOR_DELETE(dir));
        qDebug(QString("Query : "+ query.executedQuery()).toUtf8());
    }
    else if(Config::getBackupMode() == BackupMode::RECURSIVE)
    {
        //Search for new file in temp_table point view (recursive)
        query = m_sqlDb.exec(SQL_QUERY_LOOK_FOR_DELETE_RECURSIVE(dir));
        qDebug(QString("Query : "+ query.executedQuery()).toUtf8());
    }

    //Find row number
    targetField  = query.record().indexOf("Target");
    clusterField = query.record().indexOf("Cluster");

    //Delete from both SIA and database the entry
    while(query.next())
    {
        entry.target  = query.value(targetField).toString();
        entry.cluster = query.value(clusterField).toString();

        list << entry;
    }

    return list;
}

QLinkedList<t_IndexTable> DataBase::lookForChangedFiles(const QString dir)
{
    QLinkedList<t_IndexTable> list;
    t_IndexTable     entry;
    QSqlQuery        query;
    int              targetField, clusterField;

    if(Config::getBackupMode() == BackupMode::SEPARTE_BY_DIR)
    {
        //Search for new file in temp_table point view (not recursive)
        query = m_sqlDb.exec(SQL_QUERY_LOOK_FOR_CHANGE(dir));
        qDebug(QString("Query : "+ query.executedQuery()).toUtf8());
    }
    else if(Config::getBackupMode() == BackupMode::RECURSIVE)
    {
        //Search for new file in temp_table point view (recursive)
        query = m_sqlDb.exec(SQL_QUERY_LOOK_FOR_CHANGE_RECURSIVE(dir));
        qDebug(QString("Query : "+ query.executedQuery()).toUtf8());
    }

    //Find row number
    targetField  = query.record().indexOf("Target");
    clusterField = query.record().indexOf("Cluster");

    //Delete from both SIA and database the entry
    while(query.next())
    {
        entry.target  = query.value(targetField).toString();
        entry.cluster = query.value(clusterField).toString();

        list << entry;
    }

    return list;
}

void DataBase::deleteCluster(const QString cluster)
{
    QSqlQuery query;

    query = m_sqlDb.exec(SQL_QUERY_DELETE_CLUSTER_DB(cluster));
    qDebug(QString("Query : "+ query.executedQuery()).toUtf8());
}

void DataBase::deleteSmallerCluster(const QString dir)
{
    QSqlQuery query;
    int       clusterField, clusterTarget;
    QString   strCluster, strTarget;

    if(Config::getBackupMode() == BackupMode::SEPARTE_BY_DIR)
    {
        //Get the smaller cluster name in this directory (not recusrive)
        query = m_sqlDb.exec(SQL_QUERY_DELETE_SMALLER_CLUSTER(dir));
        qDebug(QString("Query : "+ query.executedQuery()).toUtf8());
    }
    else if(Config::getBackupMode() == BackupMode::RECURSIVE)
    {
        //Get the smaller cluster name in this directory (recusrive)
        query = m_sqlDb.exec(SQL_QUERY_DELETE_SMALLER_CLUSTER_RECURSIVE(dir));
        qDebug(QString("Query : "+ query.executedQuery()).toUtf8());
    }

    //Find row number
    clusterField    = query.record().indexOf("Cluster");
    clusterTarget   = query.record().indexOf("Target");

    //If the query is a success
    if(query.next())
    {
        strCluster  = query.value(clusterField).toString();
        strTarget   = query.value(clusterTarget).toString();

        //Copy the cluster content to temp_table
        query = m_sqlDb.exec(SQL_QUERY_COPY_CLUSTER_TO_TEMP_TABLE(strCluster));
        qDebug(QString("Query : "+ query.executedQuery()).toUtf8());

        //Remove the cluster from database
        this->deleteCluster(strCluster);

        //Delete on SIA
        m_siaCom->deleteFile(strTarget);
    }
}

void DataBase::syncTables(void)
{
    QSqlQuery query;

    query = m_sqlDb.exec(SQL_QUERY_SYNC_TABLES);
    qDebug(QString("Query : "+ query.executedQuery()).toUtf8());
}

int DataBase::getFileCountInTempTable(void)
{
    QSqlQuery query;

    query = m_sqlDb.exec(SQL_QUERY_COUNT_TEMP_TABLE_ROW);
    qDebug(QString("Query : "+ query.executedQuery()).toUtf8());
    query.next();

    return query.value(0).toULongLong();
}
