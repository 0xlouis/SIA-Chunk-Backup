#include "config.h"

t_GeneralConfig Config::m_configData;
t_SiaConfig     Config::m_siaConfig;

Config::Config(QObject *parent) : QObject(parent)
{
    //If the config file doesn't exist create the default
    if(QFile::exists(CONFIG_FILE_PATH))
        return;

    this->createDefault();
}

void Config::createDefault(void)
{
    QFile config(":/default/SIACBackup.ini");
    config.copy(CONFIG_FILE_PATH);
}

bool Config::load(void)
{
    QString backupMode;
    QSettings settings(CONFIG_FILE_PATH, QSettings::IniFormat);

    backupMode                          = settings.value(KEY_BACKUP_MODE, BM_SEPARTE_BY_DIR).toString();
    Config::m_configData.clusterSize    = settings.value(KEY_CLUSTER_SIZE, 40000000).toULongLong();
    Config::m_configData.dbDirPath      = settings.value(KEY_DATA_BASE_PATH, QString("./")).toString();
    Config::m_configData.dbName         = settings.value(KEY_DATA_BASE_NAME, QString("sia_backup.db")).toString();
    Config::m_configData.useCompression = settings.value(KEY_USE_COMPRESSION, false).toBool();
    Config::m_configData.useEncryption  = settings.value(KEY_USE_ENCRYPTION, false).toBool();
    Config::m_configData.avoidFrag      = settings.value(KEY_AVOID_FRAG, true).toBool();

    Config::m_configData.dbDirPath      = QFileInfo(Config::m_configData.dbDirPath).absoluteFilePath();
    Config::m_configData.tempDirPath    = QFileInfo(Config::m_configData.tempDirPath).absoluteFilePath();

    Config::m_siaConfig.ipAddress       = settings.value(KEY_IP_ADDRESS, QString("localhost")).toString();
    Config::m_siaConfig.port            = settings.value(KEY_PORT, QString("9980")).toString();

    if(backupMode == BM_SEPARTE_BY_DIR)
        Config::m_configData.backupMode = BackupMode::SEPARTE_BY_DIR;
    else if(backupMode == BM_RECURSIVE)
        Config::m_configData.backupMode = BackupMode::RECURSIVE;
    else
        return false;

    if(Config::m_configData.dbDirPath.isEmpty())
        return false;

    if(Config::m_configData.dbName.isEmpty())
        return false;

    if(Config::m_configData.tempDirPath.isEmpty())
        return false;

    if(Config::m_siaConfig.ipAddress.isEmpty())
        return false;

    if(Config::m_siaConfig.port.isEmpty())
        return false;

    return true;
}

BackupMode Config::getBackupMode(void)
{
    return Config::m_configData.backupMode;
}

QString Config::getDatabaseName(void)
{
    return Config::m_configData.dbName;
}

QString Config::getDatabasePath(void)
{
    return Config::m_configData.dbDirPath;
}

quint64 Config::getClusterSize(void)
{
    return Config::m_configData.clusterSize;
}

bool Config::getUseCompression(void)
{
    return Config::m_configData.useCompression;
}

bool Config::getUseEncryption(void)
{
    return Config::m_configData.useEncryption;
}

bool Config::getAvoidFrag(void)
{
    return Config::m_configData.avoidFrag;
}

QString Config::getSiaIpAdrress(void)
{
    return Config::m_siaConfig.ipAddress;
}

QString Config::getSiaPort(void)
{
    return Config::m_siaConfig.port;
}
