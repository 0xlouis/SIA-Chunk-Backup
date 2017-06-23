#include "archivebuilder.h"

ArchiveBuilder::ArchiveBuilder(QObject *parent) : QProcess(parent)
{
    m_gzipPath = QFileInfo(QCoreApplication::applicationDirPath() +"/gzip.exe").absoluteFilePath();

    m_tempDir = new QTemporaryDir();
    m_tempDir->setAutoRemove(true);

    m_mirrorDir = new QTemporaryDir(this->getTempDir() +"/mirror");
    m_mirrorDir->setAutoRemove(true);
}

ArchiveBuilder::~ArchiveBuilder(void)
{
    delete m_mirrorDir;
    delete m_tempDir;
}

QFileInfo ArchiveBuilder::createTar(const QString tarName, const QStringList *srcFiles)
{
    QString                 dstFile;
    QString                 tarEntryFile;
    QFile                   file;
    struct archive          *archiveTar;
    struct archive_entry    *entry;
    int                     len;
    char                    *buff;

    dstFile     = QString(this->getTempDir()+"/"+tarName);
    buff        = new char[STREAM_BUFF_BYTE];

    //Build tar archive
    archiveTar  = archive_write_new();
    archive_write_add_filter_none(archiveTar);
    archive_write_set_format_gnutar(archiveTar);
    archive_write_open_filename(archiveTar, dstFile.toUtf8().data());

    foreach(QString srcFile, *srcFiles)
    {
        //Eleminate the redundant path
        tarEntryFile = srcFile.section(this->workingDirectory(), 1);
        tarEntryFile.remove(0, 1);

        //Write the tar entry
        entry = archive_entry_new();
        archive_entry_set_pathname(entry, tarEntryFile.toUtf8().data());
        archive_entry_set_size(entry, QFileInfo(srcFile).size());
        archive_entry_set_filetype(entry, AE_IFREG);//Regular file
        archive_entry_set_perm(entry, 0644);
        archive_write_header(archiveTar, entry);

        file.setFileName(srcFile);
        file.open(QIODevice::ReadOnly);

        //Copy the data in the tar archive
        len = file.read(buff, STREAM_BUFF_BYTE);
        while(len > 0)
        {
            archive_write_data(archiveTar, buff, len);
            len = file.read(buff, STREAM_BUFF_BYTE);
        }
        file.close();

        archive_entry_free(entry);
    }

    archive_write_close(archiveTar);
    archive_write_free(archiveTar);
    delete[] buff;

    return QFileInfo(dstFile);
}

t_archiveInfo ArchiveBuilder::createTar(const QString tarName, const QStringList *srcFiles, const quint64 limit)
{
    t_archiveInfo   archiveInfo;
    QFileInfo       archiveFileInfo;
    QStringList     *filesList;
    const quint32   MAX_FILES(srcFiles->count());
    quint32         currentHeight(0);
    quint32         minHeight(1);//One file per archive (at least)
    quint32         maxHeight(MAX_FILES);
    quint32         mem[2] = {0, 1};
    quint64         archiveSize(0);
    quint32         attempt(MAX_ATTEMPT_PER_ARCHIVE);
    int             i(0);

    //If you are only one file, all the algo can be reduce to :
    if(maxHeight == 1)
    {
        archiveFileInfo         = this->createTar(tarName, srcFiles);
        archiveInfo.archiveFile = archiveFileInfo;
        archiveInfo.entryCount  = srcFiles->count();

        return archiveInfo;
    }

    filesList       = new QStringList();

    //Libarchive does not support archive editing
    //So this algo converge to the good size (according to limit value)
    do
    {
        //Solve the quantification error
        if(archiveSize < limit)//If the curve increase
            currentHeight   = ceil(((double)(maxHeight + minHeight)) / 2.0);//Progress with "dichotomy" logic
        else if(archiveSize > limit)//If the curve decrease
            currentHeight   = floor(((double)(maxHeight + minHeight)) / 2.0);//Progress with "dichotomy" logic

        //Delete the archive file if exist
        if(archiveFileInfo.exists())
            QFile::remove(archiveFileInfo.absoluteFilePath());

        //Build the new file list to build the archive
        filesList->clear();
        (*filesList)    << srcFiles->mid(0, currentHeight);

        //Build the archive
        archiveFileInfo = this->createTar(tarName, filesList);
        archiveSize     = archiveFileInfo.size();

        //This condition blocs produce the counter-reaction (converge to the result, derivate sign opposite to the "dichotomy" above)
        if(archiveSize < limit)//If the curve increase
            minHeight = currentHeight;//Update the min files in the current "dichotomy" (closed loop)
        else if(archiveSize > limit)//If the curve decrease
            maxHeight = currentHeight;//Update the max files in the current "dichotomy" (closed loop)
        else
            break;//If the perfect size was found => stop this algo and release the result

        //If the result cannot be perfect (mainly the case), then the current file list change alternatively between two values (one greater than the limit and the other smaller)
        //This last bloc above trigger when this appening and result the correct value (in this case the smaller)
        if((archiveSize < limit) && (abs(maxHeight-minHeight) <= 1))
            mem[i++] = currentHeight;//Store one correct value

        //Avoid overflow
        if(i >= 2)
            i = 0;

        //If the same best soultion was found twice time => keep it and return the result
        if(mem[0] == mem[1])
            break;

    }while(--attempt);

    archiveInfo.archiveFile = archiveFileInfo;
    archiveInfo.entryCount  = filesList->count();

    delete filesList;

    return archiveInfo;
}

QFileInfo ArchiveBuilder::createZIP(QString srcFile)
{
    srcFile = QFileInfo(srcFile).absoluteFilePath();

    if(srcFile.isEmpty())
        return QFileInfo();

#ifndef _WIN32
    QString                 zipFileDir;
    QString                 zipFilePath;
    QString                 fileName;
    QFile                   file;
    struct archive          *archiveZip;
    struct archive_entry    *entry;
    int                     len;
    char                    *buff;

    //Build the shorter file path before create archive
    fileName     = srcFile.section(this->workingDirectory(), 1);

    if(fileName.startsWith("/") || fileName.startsWith("\\"))
        fileName.remove(0, 1);

    zipFileDir   = QFileInfo(m_mirrorDir->path() +"/"+ fileName).absolutePath();
    zipFilePath  = QFileInfo(m_mirrorDir->path() +"/"+ fileName).absoluteFilePath() +".zip";

    QDir(this->getMirrorDir()).mkpath(zipFileDir);

    buff         = new char[STREAM_BUFF_BYTE];

    //Build tar archive
    archiveZip  = archive_write_new();
    archive_write_add_filter_none(archiveZip);
    archive_write_set_format_zip(archiveZip);
    archive_write_open_filename(archiveZip, zipFilePath.toUtf8().data());

    //Write the tar entry
    entry = archive_entry_new();
    archive_entry_set_pathname(entry, fileName.toUtf8().data());
    archive_entry_set_size(entry, QFileInfo(srcFile).size());
    archive_entry_set_filetype(entry, AE_IFREG);//Regular file
    archive_entry_set_perm(entry, 0644);
    archive_write_header(archiveZip, entry);

    file.setFileName(srcFile);
    file.open(QIODevice::ReadOnly);

    //Copy the data in the tar archive
    len = file.read(buff, STREAM_BUFF_BYTE);
    while(len > 0)
    {
        archive_write_data(archiveZip, buff, len);
        len = file.read(buff, STREAM_BUFF_BYTE);
    }
    file.close();

    archive_entry_free(entry);

    archive_write_close(archiveZip);
    archive_write_free(archiveZip);
    delete[] buff;

    return QFileInfo(zipFilePath);
#else
    QString     zipFileDir;
    QString     zipFilePath;
    QString     fileName;

    //Build the shorter file path before create archive
    fileName     = srcFile.section(this->workingDirectory(), 1);

    if(fileName.startsWith("/") || fileName.startsWith("\\"))
        fileName.remove(0, 1);

    zipFileDir   = QFileInfo(m_mirrorDir->path() +"/"+ fileName).absolutePath();
    zipFilePath  = QFileInfo(m_mirrorDir->path() +"/"+ fileName).absoluteFilePath();

    QDir(this->getMirrorDir()).mkpath(zipFileDir);

    QFile::copy(srcFile, zipFilePath);

    this->start(m_gzipPath, QStringList() << zipFilePath, QIODevice::ReadOnly);
    this->waitForStarted(-1);
    this->waitForFinished(-1);

    zipFilePath += ".gz";

    return QFileInfo(zipFilePath);
#endif
}

void ArchiveBuilder::cleanMirrorDir(void)
{
    delete m_mirrorDir;
    m_mirrorDir = new QTemporaryDir(this->getTempDir() +"/mirror");
}

QString ArchiveBuilder::getTempDir(void)
{
    return m_tempDir->path();
}

QString ArchiveBuilder::getMirrorDir(void)
{
    return m_mirrorDir->path();
}
