#ifndef SPACEUTIL_H_
#define SPACEUTIL_H_

#include <stdio.h>

typedef struct DiskSpace
{
    unsigned long total_space;
    unsigned long used_space;
    unsigned long available_space;
} DiskSpace;

DiskSpace getDiskSpace(const char *path);

void formatDiskSpaceMB(const DiskSpace *ds,
                       char *totalStr,
                       char *usedStr,
                       char *availStr,
                       size_t size);

#endif
