// Optional newlib syscall shim over FatFs, for consumers that want classic
// POSIX/stdio file access (open/read/lseek/fstat, or fopen/fread/fseek on
// top of them) instead of SdCard::read_file()'s whole-file-into-memory API.
// Ported from a consumer project's src/sd_stdio.c, generalized: any engine
// that seeks
// around inside a file via libc (e.g. a WAD/archive format's directory
// table) needs this, not just one game engine.
//
// Compiled in only when PICO_TOOLSET_SDCARD_STDIO is ON (see this
// component's CMakeLists.txt) -- overriding newlib's global weak syscalls
// is a whole-program decision, not every SD card consumer wants it. Once
// linked, no function here needs calling directly: just call
// pico_toolset::SdCard::init() as usual, then use fopen()/fread()/etc.
// (declared in <cstdio>/<unistd.h>) normally. Console fds 0/1/2 pass through
// to pico_stdio unchanged, so printf()/stdio_usb keep working too.
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "pico/stdio.h"
#include "pico/time.h" // at_the_end_of_time, used by _read()'s console-fd path

#include "ff.h"

#define PICO_TOOLSET_SDCARD_STDIO_MAX_FDS 12

namespace {

struct SdFile {
    FIL file;
    bool open;
    BYTE mode; // FA_* flags it was opened with
};

SdFile g_files[PICO_TOOLSET_SDCARD_STDIO_MAX_FDS];

// FatFs result -> errno, so callers can tell "no such file" from "no card" from
// "read-only" (the shim used to fail with errno 0).
int errno_from_fresult(FRESULT res)
{
    switch (res) {
    case FR_OK: return 0;
    case FR_NO_FILE:
    case FR_NO_PATH: return ENOENT;
    case FR_INVALID_NAME:
    case FR_INVALID_PARAMETER: return EINVAL;
    case FR_DENIED:
    case FR_LOCKED: return EACCES;
    case FR_WRITE_PROTECTED: return EROFS;
    case FR_EXIST: return EEXIST;
    case FR_DISK_ERR:
    case FR_INT_ERR: return EIO;
    case FR_NOT_READY:
    case FR_NOT_ENABLED:
    case FR_INVALID_DRIVE: return ENODEV;
    case FR_NO_FILESYSTEM: return ENOTSUP;
    case FR_NOT_ENOUGH_CORE: return ENOMEM;
    case FR_TOO_MANY_OPEN_FILES: return EMFILE;
    case FR_INVALID_OBJECT: return EBADF;
    case FR_TIMEOUT: return ETIMEDOUT;
    default: return EIO;
    }
}

} // namespace

extern "C" {

int _open(const char* path, int flags, ...)
{
    if (!path || !path[0])
        return -1;

    BYTE mode = 0;
    int acc = flags & (O_RDONLY | O_WRONLY | O_RDWR);
    if (acc != O_WRONLY)
        mode |= FA_READ;
    if (acc != O_RDONLY)
        mode |= FA_WRITE;
    if (flags & O_CREAT) {
        if (flags & O_TRUNC)
            mode |= FA_CREATE_ALWAYS;
        else if (flags & O_APPEND)
            mode |= FA_OPEN_APPEND;
        else
            mode |= FA_OPEN_ALWAYS;
    }

    for (int i = 0; i < PICO_TOOLSET_SDCARD_STDIO_MAX_FDS; i++) {
        if (!g_files[i].open) {
            const FRESULT res = f_open(&g_files[i].file, path, mode);
            if (res != FR_OK) {
                errno = errno_from_fresult(res);
                return -1;
            }
            g_files[i].open = true;
            g_files[i].mode = mode;
            return 3 + i;
        }
    }
    errno = EMFILE;
    return -1;
}

int _close(int fd)
{
    if (fd <= 2)
        return 0;
    int i = fd - 3;
    if (i < 0 || i >= PICO_TOOLSET_SDCARD_STDIO_MAX_FDS || !g_files[i].open)
        return -1;
    FRESULT res = f_close(&g_files[i].file);
    g_files[i].open = false;
    if (res != FR_OK) errno = errno_from_fresult(res);
    return res == FR_OK ? 0 : -1;
}

int _read(int fd, char* buf, int len)
{
    if (fd == 0)
        return stdio_get_until(buf, len, at_the_end_of_time);
    int i = fd - 3;
    if (i < 0 || i >= PICO_TOOLSET_SDCARD_STDIO_MAX_FDS || !g_files[i].open)
        return -1;

    UINT got = 0;
    const FRESULT res = f_read(&g_files[i].file, buf, (UINT)len, &got);
    if (res != FR_OK) {
        errno = errno_from_fresult(res);
        return -1;
    }
    return (int)got;
}

int _write(int fd, const char* buf, int len)
{
    if (fd == 1 || fd == 2) {
        stdio_put_string(buf, len, false, true);
        return len;
    }
    int i = fd - 3;
    if (i < 0 || i >= PICO_TOOLSET_SDCARD_STDIO_MAX_FDS || !g_files[i].open)
        return -1;

    UINT done = 0;
    const FRESULT res = f_write(&g_files[i].file, buf, (UINT)len, &done);
    if (res != FR_OK) {
        errno = errno_from_fresult(res);
        return -1;
    }
    if (done < (UINT)len) errno = ENOSPC; // short write: the volume is full
    return (int)done;
}

off_t _lseek(int fd, off_t pos, int whence)
{
    int i = fd - 3;
    if (i < 0 || i >= PICO_TOOLSET_SDCARD_STDIO_MAX_FDS || !g_files[i].open)
        return -1;

    FSIZE_t base;
    switch (whence) {
    case SEEK_SET: base = 0; break;
    case SEEK_CUR: base = f_tell(&g_files[i].file); break;
    case SEEK_END: base = f_size(&g_files[i].file); break;
    default: return -1;
    }

    int64_t target = (int64_t)base + pos;
    if (target < 0) {
        errno = EINVAL;
        return -1;
    }
    const FRESULT res = f_lseek(&g_files[i].file, (FSIZE_t)target);
    if (res != FR_OK) {
        errno = errno_from_fresult(res);
        return -1;
    }
    return (off_t)target;
}

int _fstat(int fd, struct stat* st)
{
    if (fd <= 2)
        return -1;
    int i = fd - 3;
    if (i < 0 || i >= PICO_TOOLSET_SDCARD_STDIO_MAX_FDS || !g_files[i].open)
        return -1;

    memset(st, 0, sizeof(*st));
    // Permission bits follow how the file was opened, not a blanket 0444.
    st->st_mode = S_IFREG | ((g_files[i].mode & FA_WRITE) ? 0666 : 0444);
    st->st_size = (off_t)f_size(&g_files[i].file);
    return 0;
}

int _stat(const char* path, struct stat* st)
{
    if (!path)
        return -1;

    FILINFO info;
    const FRESULT res = f_stat(path, &info);
    if (res != FR_OK) {
        errno = errno_from_fresult(res);
        return -1;
    }

    memset(st, 0, sizeof(*st));
    // FAT's read-only attribute maps to the permission bits (0444 vs 0666).
    const mode_t perms = (info.fattrib & AM_RDO) ? 0444 : 0666;
    st->st_mode = (info.fattrib & AM_DIR) ? (S_IFDIR | (perms | 0111)) : (S_IFREG | perms);
    st->st_size = (off_t)info.fsize;
    return 0;
}

int _isatty(int fd)
{
    return fd <= 2;
}

} // extern "C"
