#ifndef DATABASE_H
#define DATABASE_H

#include "archivebuilder.h"
#include "config.h"
#include "siacom.h"
#include "apptypeutils.h"

#include <QObject>
#include <QtSql>
#include <QByteArray>
#include <QCryptographicHash>
#include <QLinkedList>

#define SQL_QUERY_CREATE_TABLE_INDEX                    QString("CREATE TABLE IF NOT EXISTS \"index_table\" ( `Cluster` TEXT NOT NULL, `Source` TEXT NOT NULL UNIQUE, `Target` TEXT NOT NULL, `Hash` TEXT NOT NULL, `Size` UNSIGNED BIG INT );")
#define SQL_QUERY_CREATE_TABLE_TEMP                     QString("CREATE TEMPORARY TABLE \"temp_table\" ( `Source` TEXT NOT NULL UNIQUE, `Hash` TEXT NOT NULL, `Size` UNSIGNED BIG INT );")
#define SQL_QUERY_DROP_TABLE_TEMP                       QString("DROP TABLE IF EXISTS temp_table;")
#define SQL_QUERY_INSERT_TABLE_TEMP(SOURCE, HASH, SIZE) QString("INSERT INTO temp_table (Source, Hash, Size) VALUES ('"+QString(SOURCE)+"', '"+QString(HASH)+"', "+QString(SIZE)+");")
//#define SQL_QUERY_LOOK_FOR_DELETE(DIR)                  QString("SELECT Cluster, Target FROM index_table WHERE Source REGEXP '"+QString(DIR)+"/(?!.*/).*' AND Source NOT IN (SELECT Source FROM temp_table WHERE Source REGEXP '"+QString(DIR)+"/(?!.*/).*');")
#define SQL_QUERY_LOOK_FOR_DELETE(DIR)                  QString("SELECT Cluster,Target FROM index_table WHERE Source LIKE '"+QString(DIR)+"/%' AND Source NOT LIKE '"+QString(DIR)+"/%/%' AND Source NOT IN (SELECT Source FROM temp_table WHERE Source LIKE '"+QString(DIR)+"/%' AND Source NOT LIKE '"+QString(DIR)+"/%/%');")
#define SQL_QUERY_DELETE_CLUSTER_DB(CLUSTER)            QString("DELETE FROM index_table WHERE Cluster='"+QString(CLUSTER)+"';")
//#define SQL_QUERY_LOOK_FOR_DELETE(DIR)                  QString("SELECT Cluster, Target FROM index_table WHERE Source REGEXP '"+QString(DIR)+"/(?!.*/).*' AND Source IN (SELECT Source FROM temp_table) AND Hash NOT IN (SELECT Hash FROM temp_table);")
#define SQL_QUERY_LOOK_FOR_CHANGE(DIR)                  QString("SELECT Cluster,Target FROM index_table WHERE Source LIKE '"+QString(DIR)+"/%' AND Source NOT LIKE '"+QString(DIR)+"/%/%' AND Source IN (SELECT Source FROM temp_table) AND Hash NOT IN (SELECT Hash FROM temp_table);")
#define SQL_QUERY_SYNC_TABLES                           QString("DELETE FROM temp_table WHERE Source IN (SELECT Source FROM index_table);")
#define SQL_QUERY_GET_SRC_ORDER_BY_SIZE_DESC            QString("SELECT Source,Hash,Size FROM temp_table ORDER BY Size DESC;")
#define SQL_QUERY_COUNT_TEMP_TABLE_ROW                  QString("SELECT count(*) FROM temp_table;")
#define SQL_QUERY_DELETE_SMALLER_CLUSTER(DIR)           QString("SELECT Cluster,SUM(Size) AS CSize FROM index_table WHERE Source LIKE '"+QString(DIR)+"/%' AND Source NOT LIKE '"+QString(DIR)+"/%/%' GROUP BY Cluster ORDER BY CSize ASC LIMIT 1;")
#define SQL_QUERY_COPY_CLUSTER_TO_TEMP_TABLE(CLUSTER)   QString("INSERT INTO temp_table (Source, Hash, Size) SELECT Source, Hash, Size FROM index_table WHERE Cluster='"+QString(CLUSTER)+"';")
#define SQL_QUERY_INSERT_INDEX_TABLE(CLUSTER, SOURCE, TARGET, HASH, SIZE)\
                                                        QString("INSERT INTO index_table (Cluster, Source, Target, Hash, Size) VALUES ('"+QString(CLUSTER)+"', '"+QString(SOURCE)+"', '"+QString(TARGET)+"', '"+QString(HASH)+"', "+QString(SIZE)+");")
#define SQL_QUERY_LOOK_FOR_DELETE_RECURSIVE(DIR)        QString("SELECT Cluster,Target FROM index_table WHERE Source LIKE '"+QString(DIR)+"/%' AND Source NOT IN (SELECT Source FROM temp_table WHERE Source LIKE '"+QString(DIR)+"/%';")
#define SQL_QUERY_LOOK_FOR_CHANGE_RECURSIVE(DIR)        QString("SELECT Cluster,Target FROM index_table WHERE Source LIKE '"+QString(DIR)+"/%' AND Source IN (SELECT Source FROM temp_table) AND Hash NOT IN (SELECT Hash FROM temp_table);")
#define SQL_QUERY_DELETE_SMALLER_CLUSTER_RECURSIVE(DIR) QString("SELECT Cluster,SUM(Size) AS CSize FROM index_table WHERE Source LIKE '"+QString(DIR)+"/%' GROUP BY Cluster ORDER BY CSize ASC LIMIT 1;")

//In test, unix system is >30x faster than windows to make an cluster
#ifndef _WIN32//On unix platform
    #define MAX_FALSE_POSITIVE_IN_COMPRESION 900//Greater value incrase accruacy but impact the performance (the algo can be better to avoid this #TODO)
#else//On windows platform
    #define MAX_FALSE_POSITIVE_IN_COMPRESION 100//Greater value incrase accruacy but impact the performance (the algo can be better to avoid this #TODO)
#endif

struct t_TempTable
{
    QString     source;
    QByteArray  hash;
    quint64     size;
};

struct t_IndexTable
{
    QString     cluster;
    QString     source;
    QString     target;
    QString     hash;
    quint64     size;
};

struct t_clusterInfo
{
    QFileInfo tarFile;
    QString   clusterId;
    QString   targetSiaName;
};

class DataBase : public QObject
{
    Q_OBJECT
public:
    explicit DataBase(QObject *parent = 0);
    ~DataBase(void);
    bool load(void);
    void unload(void);
    bool setupDataBase(void);
    void getDirList(QStringList *dirList);
    void buildTemporaryTable(const QString currentDir, const QStringList *fileList);
    void syncDataBase(const QString currentDir);
    QByteArray getFileHash(const QString str_file);
    void setSyncData(const t_SyncData *syncData);
    t_SyncData getSyncData(void) const;
private:
    void deleteProcedure(const QString currentDir);
    void changeProcedure(const QString currentDir);
    void appendProcedure(const QString currentDir);
    t_clusterInfo buildCluster(const QString currentDir);
    void resetTemporaryTable(void);
    QLinkedList<t_IndexTable> lookForDeletedFiles(const QString dir);
    QLinkedList<t_IndexTable> lookForChangedFiles(const QString dir);
    void deleteCluster(const QString cluster);
    void deleteSmallerCluster(const QString dir);
    void syncTables(void);
    int getFileCountInTempTable(void);
    void buildClusterFilesList(const QString currentDir, QLinkedList<t_IndexTable*> *outDataList, QStringList *outStrList);
    t_clusterInfo makeClusterFile(const QString currentDir, QLinkedList<t_IndexTable*> *inDataList, QStringList *inStrList);

    SIACom         *m_siaCom;
    QSqlDatabase    m_sqlDb;
    t_SyncData      m_syncData;
    ArchiveBuilder *m_archiveBuilder;
};

#endif // DATABASE_H
