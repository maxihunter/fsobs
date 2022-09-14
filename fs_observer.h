#ifndef __FS_OBSERVER_H__
#define __FS_OBSERVER_H__

#include <time.h>

#define VERSION_NUMBER "1.00"

#define CFG_SIZE "size"
#define CFG_FOLDER "folder"

struct global_config {
    int nomber;
    char path[1024];
    char orig_size[128];
    int size; //size in bytes
    int sort_type;
};

struct file_info {
    char path[1024];
    long int size;
    time_t ctime;
};

#endif // __FS_OBSERVER_H__
