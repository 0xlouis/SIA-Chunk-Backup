#ifndef APPCHUNKBACKUP_H
#define APPCHUNKBACKUP_H

#include "apptypeutils.h"
#include "config.h"
#include "database.h"
#include <QCoreApplication>
#include <QTimer>
#include <QFileInfo>

#define QT_MESSAGE_PATTERN QString("%{time dd/MM/yyyy-h:mm:ss} [%{if-debug}DEBUG %{file}:%{line}%{endif}%{if-info}INFO%{endif}%{if-warning}WARNING%{endif}%{if-critical}CRITICAL%{endif}%{if-fatal}FATAL%{endif}] - %{message}")

#define ARG_MIN_TO_FUNCTION 2

#define APP_NAME            QString("SIA Chunk Backup")
#define APP_VERSION         QString("V0.5 ALPHA")
#define APP_LICENCE         QString("GNU GENERAL PUBLIC LICENSE Version 3")
#define ORGANIZATION_NAME   QString("Orion")
#define ORGANIZATION_DOMAIN QString("http://github.com")
#define APP_INFO            QString(APP_NAME +"\t"+ APP_VERSION +"\n\t"+ APP_LICENCE)

class AppChunkBackup : public QCoreApplication
{
    Q_OBJECT
public:
    AppChunkBackup(int &argc, char **argv);
    ~AppChunkBackup(void);
    int  exec(void);
    void printUsage(void);
signals:
    void ready(void);
private slots:
    void running(void);
    void quitApp(void);
    void runBackup(void);
private:
    bool loadConfigFile(void);
    bool loadAppArguments(void);
    bool loadDataBase(void);

    Config      *m_config;
    DataBase    *m_dataBase;
};

#endif // APPCHUNKBACKUP_H
