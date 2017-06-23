#include "appchunkbackup.h"

AppChunkBackup::AppChunkBackup(int &argc, char **argv) : QCoreApplication(argc, argv)
{
    //Reply app info
    this->setApplicationName(APP_NAME);
    this->setApplicationVersion(APP_VERSION);
    this->setOrganizationName(ORGANIZATION_NAME);
    this->setOrganizationDomain(ORGANIZATION_DOMAIN);

    //Set the log pattern
    qSetMessagePattern(QT_MESSAGE_PATTERN);

    //Init app object
    m_config    = new Config(this);
    m_dataBase  = new DataBase(this);

    //Wait until the app is ready to continue
    QObject::connect(this, SIGNAL(ready()), this, SLOT(runBackup()));

    QObject::connect(this, SIGNAL(aboutToQuit()), this, SLOT(quitApp()));
}

AppChunkBackup::~AppChunkBackup(void)
{
    delete m_config;
    delete m_dataBase;
}

int AppChunkBackup::exec(void)
{
    //Display app info
    qInfo(APP_INFO.toUtf8());

    //If any of beyond functions returned non success value => fatal error
    qInfo("Starting...");

    //Load config from file
    if(this->loadConfigFile() != true)
        QMetaObject::invokeMethod(this, "quit", Qt::QueuedConnection);//Will propely close app when pool event is up

    //Load app argument
    else if(this->loadAppArguments() != true)
        QMetaObject::invokeMethod(this, "quit", Qt::QueuedConnection);//Will propely close app when pool event is up

    //Load the data base
    else if(this->loadDataBase() != true)
        QMetaObject::invokeMethod(this, "quit", Qt::QueuedConnection);//Will propely close app when pool event is up

    //If everything is ok, the app can enter in running state (after going in event loop)
    else
        QTimer::singleShot(1000, this, SLOT(running()));

    //Exec pool event
    return QCoreApplication::exec();
}

void AppChunkBackup::running(void)
{
    emit this->runBackup();
}

void AppChunkBackup::quitApp(void)
{
    qInfo("Closing...");
    m_dataBase->unload();
}

void AppChunkBackup::runBackup(void)
{
    QStringList *dirList  = new QStringList();
    QStringList *fileList = new QStringList();
    QString currentDir, baseDir;

    baseDir = m_dataBase->getSyncData().rootSrcPath;

    //Scan the source directorys
    qInfo("Building directorys list...");
    m_dataBase->getDirList(dirList);
    qInfo("Done (%d results)", dirList->count());

    if(Config::getBackupMode() == BackupMode::SEPARTE_BY_DIR)
    {
        qInfo("All future actions take in account the \"SEPARTE_BY_DIR\" backup mode.");

        //Scan all source directorys
        while(!dirList->isEmpty())
        {
            //Take the next directory
            currentDir = dirList->takeFirst();

            //Scan files in the current directory
            *fileList  = QDir(currentDir).entryList(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System | QDir::CaseSensitive, QDir::Name);

            //Build the temp table in database
            m_dataBase->buildTemporaryTable(currentDir, fileList);

            //Sync database
            m_dataBase->syncDataBase(currentDir);
        }

        delete dirList;
        delete fileList;
    }
    else if(Config::getBackupMode() == BackupMode::RECURSIVE)
    {
        qInfo("All future actions take in account the \"RECURSIVE\" backup mode.");

        //Scan all source directorys
        while(!dirList->isEmpty())
        {
            //Take the next directory
            currentDir = dirList->takeFirst();

            //Scan files in the current directory
            *fileList  = QDir(currentDir).entryList(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System | QDir::CaseSensitive, QDir::Name);

            //Build the temp table in database
            m_dataBase->buildTemporaryTable(currentDir, fileList);
        }

        delete dirList;
        delete fileList;

        //Sync database
        m_dataBase->syncDataBase(baseDir);
    }

    this->quit();
}

bool AppChunkBackup::loadConfigFile(void)
{
    if(m_config->load() != true)
        return false;

    return true;
}

bool AppChunkBackup::loadAppArguments(void)
{
    QStringList argList = this->arguments();
    QFileInfo file;
    t_SyncData syncData;

    //Delete the first argument (the executable name)
    argList.removeFirst();

    //If there is not enough arguments => Print usage and close app
    if(argList.count() < ARG_MIN_TO_FUNCTION)
    {
        qCritical("Not enough arguments !");
        this->printUsage();
        return false;
    }

    //Get the arguments
    syncData.rootSrcPath = argList.takeFirst();
    syncData.rootDstPath = argList.takeFirst();

    //### Check Arguments ###
    file.setFile(syncData.rootSrcPath);

    //If the source dos not exist or if source is not dir => close app
    if(!file.exists() || !file.isDir())
    {
        qCritical("The source directory does not exist !");
        return false;
    }

    //Use absolute file path
    syncData.rootSrcPath = file.absoluteFilePath();
    qDebug("Source changed to : "+ syncData.rootSrcPath.toUtf8());

    //Remove the directory sperator at the end of the source dir
    if(syncData.rootSrcPath.endsWith('/') || syncData.rootSrcPath.endsWith('\\'))
        syncData.rootSrcPath.chop(1);

    //Remove the directory sperator at the end of the SIA target name
    if(syncData.rootDstPath.endsWith('/') || syncData.rootDstPath.endsWith('\\'))
        syncData.rootDstPath.chop(1);

    //Commit the new data
    m_dataBase->setSyncData(&syncData);

    return true;
}

bool AppChunkBackup::loadDataBase(void)
{
    if(m_dataBase->load() != true)
    {
        qCritical("An error occure when loading database !");
        return false;
    }

    if(m_dataBase->setupDataBase() != true)
    {
        qCritical("An error occure when setting up the database !");
        return false;
    }

    return true;
}

void AppChunkBackup::printUsage(void)
{
    QString usage;
    usage.append(this->applicationName() + " <source_dir> <target_dir>\n");
    usage.append("source_dir : Is the directory path to backup.\n");
    usage.append("target_dir : Is SIA target path to store the backup.\n");

    qInfo("Usage :");
    qInfo(usage.toUtf8());
}
