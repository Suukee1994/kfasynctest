#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "kf_sys_fileio.h"

#if defined(_MSC_VER)
#define _fseek64 _fseeki64
#define _ftell64 _ftelli64
#elif defined(__MINGW32__)
#define _fseek64 fseeko64
#define _ftell64 ftello64
#else
#define _POSIX_C_SOURCE 200809L
#define _fseek64 fseeko
#define _ftell64 ftello
#endif

KFILE KF_SYS_CALL KFileCreate(KF_SYS_FILE_ACCESSMODE accessMode, KF_SYS_FILE_OPENMODE openMode, const char* file)
{
    if (file == NULL || *file == 0)
        return NULL;

#ifdef _MSC_VER
    wchar_t win_file_path[MAX_PATH * 2] = {0};
    MultiByteToWideChar(CP_UTF8, 0, file, -1, win_file_path, _countof(win_file_path));

    if (openMode == OPEN_MODE_DELETE_IF_EXIST) {
        SetFileAttributesW(win_file_path, FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(win_file_path);
    }

    HANDLE hFile = CreateFileW(win_file_path,
                               accessMode == FILE_ACCESS_READ ? GENERIC_READ :
                               (accessMode == FILE_ACCESS_WRITE ? GENERIC_WRITE : GENERIC_READ|GENERIC_WRITE),
                               accessMode == FILE_ACCESS_READ ? FILE_SHARE_READ : 0, NULL,
                               (openMode == OPEN_MODE_FAIL_IF_EXIST || openMode == OPEN_MODE_DELETE_IF_EXIST) ? CREATE_NEW :
                               (openMode == OPEN_MODE_FAIL_IF_NOT_EXIST ? OPEN_EXISTING : OPEN_ALWAYS),
                               0, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        hFile = NULL;
#else
    if (openMode == OPEN_MODE_DELETE_IF_EXIST)
        remove(file);
    
    FILE* hFile = NULL;
    switch (openMode) {
        case OPEN_MODE_FAIL_IF_EXIST:
            hFile = fopen(file, "rb");
            if (hFile == NULL) {
                hFile = fopen(file, accessMode == FILE_ACCESS_WRITE ? "wb" : "wb+");
            }else{
                fclose(hFile);
                hFile = NULL;
            }
            break;
        case OPEN_MODE_FAIL_IF_NOT_EXIST:
            hFile = fopen(file, accessMode == FILE_ACCESS_READ ? "rb" :
                          (accessMode == FILE_ACCESS_WRITE ? "wb" : "wb+"));
            break;
        case OPEN_MODE_APPEND_IF_EXIST:
            if (accessMode != FILE_ACCESS_READ)
                hFile = fopen(file, accessMode == FILE_ACCESS_WRITE ? "ab" : "ab+");
            break;
        case OPEN_MODE_DELETE_IF_EXIST:
            hFile = fopen(file, "wb+");
            break;
    }
#endif
    return hFile;
}

void KF_SYS_CALL KFileClose(KFILE file)
{
    if (file != NULL)
#ifdef _MSC_VER
        CloseHandle((HANDLE)file);
#else
        fclose((FILE*)file);
#endif
}

int KF_SYS_CALL KFileRead(KFILE file, void* buf, int size)
{
    if (file == NULL)
        return -1;

#ifdef _MSC_VER
    DWORD rsize = 0;
    if (!ReadFile((HANDLE)file, buf, size, &rsize, NULL))
        return -1;
    return (int)rsize;
#else
    return (int)fread(buf, 1, size, (FILE*)file);
#endif
}

int KF_SYS_CALL KFileWrite(KFILE file, void* buf, int size)
{
    if (file == NULL)
        return -1;

#ifdef _MSC_VER
    DWORD wsize = 0;
    if (!WriteFile((HANDLE)file, buf, size, &wsize, NULL))
        return -1;
    return (int)wsize;
#else
    return (int)fwrite(buf, 1, size, (FILE*)file);
#endif
}

void KF_SYS_CALL KFileFlush(KFILE file)
{
    if (file != NULL)
#ifdef _MSC_VER
        FlushFileBuffers((HANDLE)file);
#else
        fflush((FILE*)file);
#endif
}

long long KF_SYS_CALL KFileSeek(KFILE file, long long offset, int whence)
{
    if (file == NULL)
        return -1;

#ifdef _MSC_VER
    LARGE_INTEGER pos;
    if (whence == SEEK_CUR && offset == 0) {
        pos.QuadPart = 0;
        if (!SetFilePointerEx((HANDLE)file, pos, &pos, FILE_CURRENT))
            return -1;
    }else{
        pos.QuadPart = offset;
        if (!SetFilePointerEx((HANDLE)file, pos, &pos, (DWORD)whence))
            return -1;
    }
    return pos.QuadPart;
#else
    if (whence == SEEK_CUR && offset == 0)
        return _ftell64((FILE*)file);
    
    if (_fseek64((FILE*)file, offset, whence) == -1)
        return -1;
    return _ftell64((FILE*)file);
#endif
}

long long KF_SYS_CALL KFileGetSize(KFILE file)
{
    if (file == NULL)
        return -1;

#ifdef _MSC_VER
    LARGE_INTEGER size;
    size.QuadPart = 0;
    if (!GetFileSizeEx((HANDLE)file, &size))
        return -1;
    return size.QuadPart;
#else
    long long old_pos = _ftell64((FILE*)file);
    _fseek64((FILE*)file, 0, SEEK_END);
    long long result = _ftell64((FILE*)file);
    _fseek64((FILE*)file, old_pos, SEEK_SET);
    return result;
#endif
}

char* KF_SYS_CALL KFileGetName(KFILE file)
{
    if (file == NULL)
        return NULL;

#ifdef _MSC_VER
    FILE_NAME_INFO* fni = (FILE_NAME_INFO*)malloc(MAX_PATH * 4);
    if (fni == NULL)
        return NULL;

    char* path = NULL;
    memset(fni, 0, MAX_PATH * 4);
    if (GetFileInformationByHandleEx((HANDLE)file, FileNameInfo, fni, MAX_PATH * 4)) {
        char utf8_file_path[MAX_PATH * 2] = {0};
        if (WideCharToMultiByte(CP_UTF8, 0, fni->FileName, -1, utf8_file_path, _countof(utf8_file_path), NULL, NULL) > 0)
            path = strdup(utf8_file_path);
    }
    free(fni);
    return path;
#elif __APPLE__
    return NULL;
#else
    return NULL;
#endif
}

void KF_SYS_CALL KFileDelete(const char* file)
{
    if (file == NULL)
        return;

#ifdef _MSC_VER
    wchar_t win_file_path[MAX_PATH * 2] = {0};
    MultiByteToWideChar(CP_UTF8, 0, file, -1, win_file_path, _countof(win_file_path));
    DeleteFileW(win_file_path);
#else
    remove(file);
#endif
}

void KF_SYS_CALL KFileMove(const char* oldFile, const char* newFile)
{
    if (oldFile == NULL || newFile == NULL)
        return;

#ifdef _MSC_VER
    wchar_t old_file_path[MAX_PATH * 2] = {0};
    wchar_t new_file_path[MAX_PATH * 2] = {0};
    MultiByteToWideChar(CP_UTF8, 0, oldFile, -1, old_file_path, _countof(old_file_path));
    MultiByteToWideChar(CP_UTF8, 0, newFile, -1, new_file_path, _countof(new_file_path));
    MoveFileW(old_file_path, new_file_path);
#else
    rename(oldFile, newFile);
#endif
}