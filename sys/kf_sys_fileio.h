#ifndef __KF_SYS_FILE_IO_H
#define __KF_SYS_FILE_IO_H

#ifndef KF_SYS_CALL
#ifdef _MSC_VER
#define KF_SYS_CALL __stdcall
#else
#define KF_SYS_CALL
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FILE_ACCESS_READ = 0,
    FILE_ACCESS_WRITE,
    FILE_ACCESS_READWRITE
} KF_SYS_FILE_ACCESSMODE;

typedef enum {
    OPEN_MODE_FAIL_IF_EXIST = 0,
    OPEN_MODE_FAIL_IF_NOT_EXIST,
    OPEN_MODE_APPEND_IF_EXIST,
    OPEN_MODE_DELETE_IF_EXIST
} KF_SYS_FILE_OPENMODE;

typedef void* KFILE;

KFILE KF_SYS_CALL KFileCreate(KF_SYS_FILE_ACCESSMODE accessMode, KF_SYS_FILE_OPENMODE openMode, const char* file);
void  KF_SYS_CALL KFileClose(KFILE file);

int  KF_SYS_CALL KFileRead(KFILE file, void* buf, int size);
int  KF_SYS_CALL KFileWrite(KFILE file, void* buf, int size);
void KF_SYS_CALL KFileFlush(KFILE file);

long long KF_SYS_CALL KFileSeek(KFILE file, long long offset, int whence);
long long KF_SYS_CALL KFileGetSize(KFILE file);

char* KF_SYS_CALL KFileGetName(KFILE file); //返回值用free释放，可能返回NULL

void KF_SYS_CALL KFileDelete(const char* file);
void KF_SYS_CALL KFileMove(const char* oldFile, const char* newFile);

#ifdef __cplusplus
}
#endif

#endif //__KF_SYS_FILE_IO_H