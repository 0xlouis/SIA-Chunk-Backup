#ifndef ARCHIVEBUILDER_H
#define ARCHIVEBUILDER_H

#include "libarchive/archive.h"
#include "libarchive/archive_entry.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include <cmath>
#include <QObject>
#include <QProcess>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QDir>

#define MAX_ATTEMPT_PER_ARCHIVE 32//Perfect accruacy if you have less than a couple of millions of files in one archive

#define STREAM_BUFF_BYTE 8192

struct t_archiveInfo
{
    QFileInfo archiveFile;
    quint32   entryCount;
};

class ArchiveBuilder : public QProcess
{
public:
    ArchiveBuilder(QObject *parent = 0);
    ~ArchiveBuilder(void);
    QFileInfo createTar(const QString tarName, const QStringList *srcFiles);
    t_archiveInfo createTar(const QString tarName, const QStringList *srcFiles, const quint64 limit);
    QFileInfo createZIP(QString srcFile);
    void cleanMirrorDir(void);
    QString getTempDir(void);
    QString getMirrorDir(void);
private:
    QTemporaryDir   *m_tempDir;
    QTemporaryDir   *m_mirrorDir;
    QString         m_gzipPath;
};

#endif // ARCHIVEBUILDER_H
