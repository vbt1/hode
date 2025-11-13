
#ifndef FS_H__
#define FS_H__

#include <stdio.h>
extern "C" {
#include "gfs_wrap.h"
}

struct FileSystem {

	const char *_dataPath;
	const char *_savePath;
#if 0
	int _filesCount;
	char **_filesList;
#endif
	FileSystem(const char *dataPath, const char *savePath);
	~FileSystem();

	GFS_FILE *openAssetFile(const char *filename);
	GFS_FILE *openSaveFile(const char *filename, bool write);
	int closeFile(GFS_FILE *);

	void addFilePath(const char *path);
	void listFiles(const char *dir);
};

#endif // FS_H__
