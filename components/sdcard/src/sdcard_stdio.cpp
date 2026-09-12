// Optional newlib syscall shim over FatFs, for consumers that want classic
// POSIX/stdio file access (open/read/lseek/fstat, or fopen/fread/fseek on
// top of them) instead of SdCard::read_file()'s whole-file-into-memory API.
// Ported from PicoDoom's src/sd_stdio.c, generalized: any engine that seeks
// around inside a file via libc (e.g. a WAD/archive format's directory
// table) needs this, not just Doom.
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

#include "ff.h"

#define PICO_TOOLSET_SDCARD_STDIO_MAX_FDS 12

namespace {

struct SdFile {
    FIL file;
    bool open;
};

SdFile g_files[PICO_TOOLSET_SDCARD_STDIO_MAX_FDS];

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
            if (f_open(&g_files[i].file, path, mode) != FR_OK)
                return -1;
            g_files[i].open = true;
            return 3 + i;
        }
    }
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
    if (f_read(&g_files[i].file, buf, (UINT)len, &got) != FR_OK)
        return -1;
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
    if (f_write(&g_files[i].file, buf, (UINT)len, &done) != FR_OK)
        return -1;
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
    if (target < 0)
        return -1;
    if (f_lseek(&g_files[i].file, (FSIZE_t)target) != FR_OK)
        return -1;
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
    st->st_mode = S_IFREG | 0444;
    st->st_size = (off_t)f_size(&g_files[i].file);
    return 0;
}

int _stat(const char* path, struct stat* st)
{
    if (!path)
        return -1;

    FILINFO info;
    if (f_stat(path, &info) != FR_OK)
        return -1;

    memset(st, 0, sizeof(*st));
    st->st_mode = (info.fattrib & AM_DIR) ? S_IFDIR : S_IFREG;
    st->st_size = (off_t)info.fsize;
    return 0;
}

int _isatty(int fd)
{
    return fd <= 2;
}

} // extern "C"
