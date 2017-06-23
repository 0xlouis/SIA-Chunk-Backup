#ifndef CONFIG_H
#define CONFIG_H

#include "apptypeutils.h"
#include <QObject>
#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>

#define CONFIG_FILE_NAME QString("SIACBackup.ini")
#define CONFIG_FILE_PATH QString(QCoreApplication::applicationDirPath()+"/"+CONFIG_FILE_NAME)

#define KEY_BACKUP_MODE     "general/backup_mode"
#define KEY_DATA_BASE_NAME  "general/database_name"
#define KEY_DATA_BASE_PATH  "general/database_dir"
#define KEY_CLUSTER_SIZE    "general/cluster_size"
#define KEY_USE_COMPRESSION "general/use_compression"
#define KEY_USE_ENCRYPTION  "general/use_encryption"
#define KEY_AVOID_FRAG      "general/avoid_frag"
#define KEY_IP_ADDRESS      "sia/ip_address"
#define KEY_PORT            "sia/port"

#define BM_SEPARTE_BY_DIR  QString("SEPARTE_BY_DIR")
#define BM_RECURSIVE       QString("RECURSIVE")

enum class BackupMode : int
{
    SEPARTE_BY_DIR,
    RECURSIVE
};

struct t_GeneralConfig
{
    BackupMode  backupMode;
    QString     dbName;
    QString     dbDirPath;
    quint64     clusterSize;
    QString     tempDirPath;
    bool        useCompression;
    bool        useEncryption;
    bool        avoidFrag;
};

struct t_SiaConfig
{
    QString ipAddress;
    QString port;
};

class Config : public QObject
{
    Q_OBJECT
public:
    explicit Config(QObject *parent = 0);
    void createDefault(void);
    bool load(void);
    static BackupMode getBackupMode(void);
    static QString getDatabaseName(void);
    static QString getDatabasePath(void);
    static quint64 getClusterSize(void);
    static bool getUseCompression(void);
    static bool getUseEncryption(void);
    static bool getAvoidFrag(void);
    static QString getSiaIpAdrress(void);
    static QString getSiaPort(void);
private:
    static t_GeneralConfig  m_configData;
    static t_SiaConfig      m_siaConfig;
};

#endif // CONFIG_H
