# SIA-Chunk-Backup
Prepare and send data efficiently on SIA network.
Espically the SIA network work with chunk of 40MB (if your file is smaller it simply padded by SIA) so the basic idea of this software is to regroup your files in archive to match an upload of 40MB (Some feature such compression, encryption (later) can be enabled in the config).

Once upload finished, this software create an SQLITE file index (which contain the "link table" between archive name (on SIA) and file name (in your local folder)). Currently you can browse friendly in this index with : http://sqlitebrowser.org

# Install
- Build the source with Qt 5.9 (Work under GNU/Linux and Windows).
- At this point you have the executable "SIA_Chunk_Backup", execute them (without arguments) it should create config "SIACBackup.ini" in the same directory.
- Read/Edit the config.

# Usage
SIA_Chunk_Backup <source_dir> <target_dir>

source_dir : Is the directory path to backup.

target_dir : Is SIA target path to store the backup.

# Contrib
Since I'm not computer engineer, any help (upgrade, bugs correction) is welcome :p
