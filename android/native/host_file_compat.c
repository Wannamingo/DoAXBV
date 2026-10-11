#include "host_file_compat.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct RecompHostFile { int fd; DIR *directory; char path[PATH_MAX]; };
static _Thread_local DWORD last_error;
static BOOL fail(void)
{
    switch (errno) {
    case ENOENT: last_error = ERROR_FILE_NOT_FOUND; break;
    case ENOTDIR: last_error = ERROR_PATH_NOT_FOUND; break;
    case EEXIST: last_error = ERROR_ALREADY_EXISTS; break;
    case EACCES: case EPERM: last_error = ERROR_ACCESS_DENIED; break;
    case EBUSY: last_error = ERROR_SHARING_VIOLATION; break;
    default: last_error = 1; break;
    }
    return 0;
}
DWORD GetLastError(void) { return last_error; }

/* Guest paths use case-insensitive Windows/Xbox separators. Resolve each
   existing component against the Android filesystem; new leaf names retain
   their requested spelling. No process working-directory change is needed. */
static BOOL native_path(const char *source, char result[PATH_MAX])
{
    if (!source || strlen(source) >= PATH_MAX) { errno = ENAMETOOLONG; return fail(); }
    char input[PATH_MAX];
    strcpy(input, source);
    for (char *p = input; *p; ++p) if (*p == '\\') *p = '/';
    result[0] = 0;
    if (input[0] == '/') strcpy(result, "/");
    char *save = NULL;
    for (char *part = strtok_r(input, "/", &save); part; part = strtok_r(NULL, "/", &save)) {
        char name[256];
        if (strlen(part) >= sizeof name) { errno = ENAMETOOLONG; return fail(); }
        strcpy(name, part);
        DIR *dir = opendir(*result ? result : ".");
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir))) if (!strcasecmp(entry->d_name, part)) {
                strcpy(name, entry->d_name); break;
            }
            closedir(dir);
        }
        size_t length = strlen(result), count = strlen(name);
        if (length + count + 2 >= PATH_MAX) { errno = ENAMETOOLONG; return fail(); }
        if (length && result[length - 1] != '/') strcat(result, "/");
        strcat(result, name);
    }
    return 1;
}
static DWORD attributes(const struct stat *st)
{
    DWORD a = S_ISDIR(st->st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    if (!(st->st_mode & S_IWUSR)) a |= FILE_ATTRIBUTE_READONLY;
    return a;
}
static uint64_t timestamp(struct timespec value)
{
    return ((uint64_t)value.tv_sec + UINT64_C(11644473600)) * UINT64_C(10000000) + value.tv_nsec / 100;
}
static FILETIME filetime(struct timespec value)
{
    uint64_t t = timestamp(value);
    return (FILETIME){(DWORD)t, (DWORD)(t >> 32)};
}
HANDLE CreateFileA(const char *path, DWORD access, DWORD share, void *security, DWORD disposition, DWORD flags, HANDLE template)
{
    (void)share; (void)security; (void)flags; (void)template;
    char native[PATH_MAX];
    if (!native_path(path, native)) return INVALID_HANDLE_VALUE;
    int options = O_CLOEXEC;
    if (access & (GENERIC_WRITE | GENERIC_ALL)) options |= (access & (GENERIC_READ | GENERIC_ALL)) ? O_RDWR : O_WRONLY;
    else options |= O_RDONLY;
    switch (disposition) {
    case CREATE_NEW: options |= O_CREAT | O_EXCL; break;
    case CREATE_ALWAYS: options |= O_CREAT | O_TRUNC; break;
    case OPEN_ALWAYS: options |= O_CREAT; break;
    case TRUNCATE_EXISTING: options |= O_TRUNC; break;
    case OPEN_EXISTING: break;
    default: errno = EINVAL; fail(); return INVALID_HANDLE_VALUE;
    }
    int fd = open(native, options, 0600);
    if (fd < 0) { fail(); return INVALID_HANDLE_VALUE; }
    HANDLE h = calloc(1, sizeof *h);
    if (!h) { close(fd); errno = ENOMEM; fail(); return INVALID_HANDLE_VALUE; }
    h->fd = fd;
    strcpy(h->path, native);
    last_error = ERROR_SUCCESS;
    return h;
}
HANDLE CreateFileW(const wchar_t *path, DWORD access, DWORD share, void *security, DWORD disposition, DWORD flags, HANDLE template)
{
    char utf8[PATH_MAX]; size_t at = 0;
    for (; path && *path; ++path) {
        uint32_t c = (uint32_t)*path;
        if (at + 4 >= sizeof utf8 || c > 0x10ffffu || (c >= 0xd800u && c <= 0xdfffu)) {
            errno = EINVAL; fail(); return INVALID_HANDLE_VALUE;
        }
        if (c < 0x80u) utf8[at++] = (char)c;
        else if (c < 0x800u) { utf8[at++] = (char)(0xc0u | (c >> 6)); utf8[at++] = (char)(0x80u | (c & 63)); }
        else if (c < 0x10000u) { utf8[at++] = (char)(0xe0u | (c >> 12)); utf8[at++] = (char)(0x80u | ((c >> 6) & 63)); utf8[at++] = (char)(0x80u | (c & 63)); }
        else { utf8[at++] = (char)(0xf0u | (c >> 18)); utf8[at++] = (char)(0x80u | ((c >> 12) & 63)); utf8[at++] = (char)(0x80u | ((c >> 6) & 63)); utf8[at++] = (char)(0x80u | (c & 63)); }
    }
    utf8[at] = 0;
    return CreateFileA(utf8, access, share, security, disposition, flags, template);
}
BOOL CloseHandle(HANDLE h)
{
    if (!h || h == INVALID_HANDLE_VALUE) { errno = EBADF; return fail(); }
    int result = h->directory ? closedir(h->directory) : close(h->fd);
    free(h);
    return result == 0 ? 1 : fail();
}
BOOL ReadFile(HANDLE h, void *buffer, DWORD count, DWORD *got, void *overlapped)
{
    (void)overlapped;
    if (!h || h == INVALID_HANDLE_VALUE || !got) { errno = EBADF; return fail(); }
    ssize_t n; do { n = read(h->fd, buffer, count); } while (n < 0 && errno == EINTR);
    *got = n < 0 ? 0 : (DWORD)n;
    return n < 0 ? fail() : 1;
}
BOOL WriteFile(HANDLE h, const void *buffer, DWORD count, DWORD *got, void *overlapped)
{
    (void)overlapped;
    if (!h || h == INVALID_HANDLE_VALUE || !got) { errno = EBADF; return fail(); }
    ssize_t n; do { n = write(h->fd, buffer, count); } while (n < 0 && errno == EINTR);
    *got = n < 0 ? 0 : (DWORD)n;
    return n < 0 ? fail() : 1;
}
BOOL SetFilePointerEx(HANDLE h, LARGE_INTEGER distance, LARGE_INTEGER *position, DWORD origin)
{
    if (!h || h == INVALID_HANDLE_VALUE || origin != FILE_BEGIN) { errno = EINVAL; return fail(); }
    off_t n = lseek(h->fd, (off_t)distance.QuadPart, SEEK_SET);
    if (position) position->QuadPart = n;
    return n < 0 ? fail() : 1;
}
BOOL GetFileSizeEx(HANDLE h, LARGE_INTEGER *size)
{
    struct stat st;
    if (!h || h == INVALID_HANDLE_VALUE || !size) { errno = EBADF; return fail(); }
    if (fstat(h->fd, &st)) return fail();
    size->QuadPart = st.st_size; return 1;
}
BOOL SetEndOfFile(HANDLE h)
{
    if (!h || h == INVALID_HANDLE_VALUE) { errno = EBADF; return fail(); }
    off_t position = lseek(h->fd, 0, SEEK_CUR);
    return position >= 0 && ftruncate(h->fd, position) == 0 ? 1 : fail();
}
BOOL FlushFileBuffers(HANDLE h)
{
    if (!h || h == INVALID_HANDLE_VALUE) { errno = EBADF; return fail(); }
    return fsync(h->fd) == 0 ? 1 : fail();
}
DWORD GetFileAttributesA(const char *path)
{
    char native[PATH_MAX]; struct stat st;
    if (!native_path(path, native) || stat(native, &st)) { fail(); return INVALID_FILE_ATTRIBUTES; }
    return attributes(&st);
}
BOOL CreateDirectoryA(const char *path, void *security)
{
    (void)security; char native[PATH_MAX];
    return native_path(path, native) && mkdir(native, 0700) == 0 ? 1 : fail();
}
BOOL RemoveDirectoryA(const char *path)
{
    char native[PATH_MAX]; return native_path(path, native) && rmdir(native) == 0 ? 1 : fail();
}
BOOL DeleteFileA(const char *path)
{
    char native[PATH_MAX]; return native_path(path, native) && unlink(native) == 0 ? 1 : fail();
}
BOOL FindNextFileA(HANDLE h, WIN32_FIND_DATAA *data)
{
    if (!h || h == INVALID_HANDLE_VALUE || !h->directory || !data) { errno = EINVAL; return fail(); }
    struct dirent *entry;
    while ((entry = readdir(h->directory))) {
        char path[PATH_MAX]; struct stat st;
        if (strlen(h->path) + strlen(entry->d_name) + 2 >= sizeof path) continue;
        strcpy(path, h->path); strcat(path, "/"); strcat(path, entry->d_name);
        if (stat(path, &st)) continue;
        memset(data, 0, sizeof *data);
        strcpy(data->cFileName, entry->d_name);
        data->dwFileAttributes = attributes(&st);
        data->nFileSizeLow = (DWORD)st.st_size; data->nFileSizeHigh = (DWORD)((uint64_t)st.st_size >> 32);
        data->ftCreationTime = filetime(st.st_ctim);
        data->ftLastWriteTime = filetime(st.st_mtim); data->ftLastAccessTime = filetime(st.st_atim);
        return 1;
    }
    return 0;
}
HANDLE FindFirstFileA(const char *pattern, WIN32_FIND_DATAA *data)
{
    char native[PATH_MAX];
    if (!native_path(pattern, native)) return INVALID_HANDLE_VALUE;
    char *leaf = strrchr(native, '/');
    if (leaf) *leaf = 0; else strcpy(native, ".");
    DIR *directory = opendir(*native ? native : "/");
    if (!directory) { fail(); return INVALID_HANDLE_VALUE; }
    HANDLE h = calloc(1, sizeof *h);
    if (!h) { closedir(directory); errno = ENOMEM; fail(); return INVALID_HANDLE_VALUE; }
    h->directory = directory; h->fd = -1; strcpy(h->path, *native ? native : "/");
    if (!FindNextFileA(h, data)) { CloseHandle(h); return INVALID_HANDLE_VALUE; }
    return h;
}
BOOL FindClose(HANDLE h) { return CloseHandle(h); }
BOOL GetFileInformationByHandleEx(HANDLE h, int kind, void *out, DWORD size)
{
    struct stat st;
    if (!h || h == INVALID_HANDLE_VALUE || kind != FileBasicInfo || size < sizeof(FILE_BASIC_INFO)) { errno = EINVAL; return fail(); }
    if (fstat(h->fd, &st)) return fail();
    FILE_BASIC_INFO *info = out;
    info->CreationTime.QuadPart = timestamp(st.st_ctim); info->LastAccessTime.QuadPart = timestamp(st.st_atim);
    info->LastWriteTime.QuadPart = timestamp(st.st_mtim); info->ChangeTime.QuadPart = timestamp(st.st_ctim);
    info->FileAttributes = attributes(&st); return 1;
}
BOOL SetFileInformationByHandle(HANDLE h, int kind, const void *in, DWORD size)
{
    struct stat st;
    if (!h || h == INVALID_HANDLE_VALUE || kind != FileBasicInfo || size < sizeof(FILE_BASIC_INFO)) { errno = EINVAL; return fail(); }
    if (fstat(h->fd, &st)) return fail();
    const FILE_BASIC_INFO *info = in;
    if (info->FileAttributes) {
        mode_t mode = st.st_mode;
        if (info->FileAttributes & FILE_ATTRIBUTE_READONLY) mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
        else mode |= S_IWUSR;
        if (fchmod(h->fd, mode)) return fail();
    }
    struct timespec times[2] = {st.st_atim, st.st_mtim};
    const int64_t values[2] = {info->LastAccessTime.QuadPart, info->LastWriteTime.QuadPart};
    for (unsigned i = 0; i < 2; ++i) if (values[i] > 0) {
        times[i].tv_sec = values[i] / INT64_C(10000000) - INT64_C(11644473600);
        times[i].tv_nsec = (values[i] % INT64_C(10000000)) * 100;
    }
    return futimens(h->fd, times) == 0 ? 1 : fail();
}
HANDLE GetCurrentProcess(void) { return (HANDLE)(intptr_t)-2; }
BOOL TerminateProcess(HANDLE process, unsigned code) { (void)process; _exit((int)code); }
