#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "include/spaceutil.h"
#include "include/logutil.h"
#include "include/config.h"

#define BYTES_IN_MB (1024.0 * 1024.0)

extern struct applicationConfig appConfig;

DiskSpace getDiskSpace(const char *path)
{
    struct statvfs stat;
    DiskSpace disk_space = {0, 0, 0};

    if (statvfs(path, &stat) != 0)
    {
        perror("statvfs failed");
        return disk_space;
    }

    disk_space.total_space = stat.f_blocks * stat.f_frsize;
    disk_space.available_space = stat.f_bavail * stat.f_frsize;
    disk_space.used_space = disk_space.total_space - (stat.f_bfree * stat.f_frsize);
    return disk_space;
}

void formatDiskSpaceMB(const DiskSpace *ds,
                       char *totalStr,
                       char *usedStr,
                       char *availStr,
                       size_t size)
{
    snprintf(totalStr, size, "%.2f MB", ds->total_space / BYTES_IN_MB);
    snprintf(usedStr, size, "%.2f MB", ds->used_space / BYTES_IN_MB);
    snprintf(availStr, size, "%.2f MB", ds->available_space / BYTES_IN_MB);
}