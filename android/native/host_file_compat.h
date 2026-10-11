#ifndef DOAXBV_ANDROID_HOST_FILE_H
#define DOAXBV_ANDROID_HOST_FILE_H
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>
#include <strings.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct RecompHostFile *HANDLE;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef int64_t LONGLONG;
typedef int BOOL;
typedef unsigned char BOOLEAN;
typedef union LARGE_INTEGER { int64_t QuadPart; struct { uint32_t LowPart; int32_t HighPart; }; } LARGE_INTEGER;
typedef struct FILETIME { DWORD dwLowDateTime, dwHighDateTime; } FILETIME;
typedef struct FILE_BASIC_INFO {
    LARGE_INTEGER CreationTime, LastAccessTime, LastWriteTime, ChangeTime;
    DWORD FileAttributes;
} FILE_BASIC_INFO;
typedef struct WIN32_FIND_DATAA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD nFileSizeHigh, nFileSizeLow;
    char cFileName[256];
} WIN32_FIND_DATAA;
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define INVALID_FILE_ATTRIBUTES UINT32_MAX
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define GENERIC_EXECUTE 0x20000000u
#define GENERIC_ALL 0x10000000u
#define DELETE 0x00010000u
#define FILE_READ_ATTRIBUTES 0x80u
#define FILE_WRITE_ATTRIBUTES 0x100u
#define FILE_GENERIC_READ 0x00120089u
#define FILE_SHARE_READ 1u
#define FILE_SHARE_WRITE 2u
#define FILE_SHARE_DELETE 4u
#define FILE_ATTRIBUTE_READONLY 1u
#define FILE_ATTRIBUTE_DIRECTORY 0x10u
#define FILE_ATTRIBUTE_NORMAL 0x80u
#define FILE_FLAG_BACKUP_SEMANTICS 0x02000000u
#define CREATE_NEW 1u
#define CREATE_ALWAYS 2u
#define OPEN_EXISTING 3u
#define OPEN_ALWAYS 4u
#define TRUNCATE_EXISTING 5u
#define FILE_BEGIN 0u
#define ERROR_SUCCESS 0u
#define ERROR_FILE_NOT_FOUND 2u
#define ERROR_PATH_NOT_FOUND 3u
#define ERROR_ACCESS_DENIED 5u
#define ERROR_SHARING_VIOLATION 32u
#define ERROR_FILE_EXISTS 80u
#define ERROR_ALREADY_EXISTS 183u
#define TRUE 1
#define FALSE 0
#define FileBasicInfo 0
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
DWORD GetLastError(void);
HANDLE CreateFileA(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
HANDLE CreateFileW(const wchar_t *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
BOOL CloseHandle(HANDLE);
BOOL ReadFile(HANDLE, void *, DWORD, DWORD *, void *);
BOOL WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
BOOL SetFilePointerEx(HANDLE, LARGE_INTEGER, LARGE_INTEGER *, DWORD);
BOOL GetFileSizeEx(HANDLE, LARGE_INTEGER *);
BOOL SetEndOfFile(HANDLE);
BOOL FlushFileBuffers(HANDLE);
DWORD GetFileAttributesA(const char *);
BOOL CreateDirectoryA(const char *, void *);
BOOL RemoveDirectoryA(const char *);
BOOL DeleteFileA(const char *);
HANDLE FindFirstFileA(const char *, WIN32_FIND_DATAA *);
BOOL FindNextFileA(HANDLE, WIN32_FIND_DATAA *);
BOOL FindClose(HANDLE);
BOOL GetFileInformationByHandleEx(HANDLE, int, void *, DWORD);
BOOL SetFileInformationByHandle(HANDLE, int, const void *, DWORD);
HANDLE GetCurrentProcess(void);
BOOL TerminateProcess(HANDLE, unsigned);
#ifdef __cplusplus
}
#endif
#endif
