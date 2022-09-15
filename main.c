/* 
 * This is the file of autoremove files based on quota and creation time
 *
 */

#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "ctype.h"
#include "time.h"
#include <dirent.h>
#include <sys/stat.h>

#include "fs_observer.h"

struct global_config cfg;

#define DEF_CONFIG_FILE "fs_obs.conf"

int read_cfg_file(char * fl);
void print_help(char * name);
char * print_pretty_size(int size);

int main (int argc, char ** argv) {
    char conf_file[1024] = {0};
    int dry_run = 1;
    int opt = 0;

    snprintf(conf_file, 1024, "%s", DEF_CONFIG_FILE);
    while ((opt = getopt(argc, argv, "hrc:")) != -1) {
        switch(opt) {
            case 'h':
            case '?':
                print_help(argv[0]);
                return 0;
            case 'r':
                dry_run = 0;
                break;
            case 'c':
                snprintf(conf_file, 1024, "%s", optarg);
                break;
        }
    }

    memset(&cfg, 0, sizeof(struct global_config));
    if (read_cfg_file(conf_file)) {
        return 0;
    }

    if (cfg.size == 0) {
        printf("Search folder size does not specified. Exiting.\n");
        return 0;
    }
    if (strlen(cfg.path) == 0) {
        printf("Search folder does not specified. Exiting.\n");
        return 0;
    }
    DIR *d;
    struct dirent *dir;
    char location[1024] = {0};
    struct stat st;
    unsigned long int total_size = 0;
    unsigned int count = 0;
    struct file_info * fi = NULL;

    d = opendir(cfg.path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0 ||
                    dir->d_type == DT_DIR)
                continue;
            count++;
            fi = realloc(fi, sizeof(struct file_info)*count);
            if (!fi) {
                printf("Cannot allocate memory\n");
                break;
            }
            snprintf(location, 1024,"%s/%s", cfg.path, dir->d_name);
            stat(location, &st);
            (fi+count-1)->size = st.st_size;
            (fi+count-1)->ctime = st.st_mtime;
            strcpy((fi+count-1)->path, location);
            //printf("%s [%d]: %d\n", dir->d_name, st.st_mtime, st.st_size);
            total_size += st.st_size;
        }
        closedir(d);
    }
    printf("Total size of %d files: %s (%lu bytes)\n", count, print_pretty_size(total_size), total_size );
    if (total_size < cfg.size) {
        printf ("Quota is not reached (given %s). Nothing to do\n", cfg.orig_size);
        return 0;
    }

    struct file_info tmp;
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < count-1; j++) {
            if ((fi+j)->ctime > (fi+j+1)->ctime) {
                memcpy(&tmp, (fi+j), sizeof(struct file_info));
                memcpy(fi+j, fi+j+1, sizeof(struct file_info));
                memcpy(fi+j+1, &tmp, sizeof(struct file_info));
            }
        }
    }

    /*
    for (int i = 0; i < count; i++) {
        printf("[%d]%s: %d\n", i, (fi+i)->path, (fi+i)->ctime);
    }
    */

    unsigned long int new_size = total_size;
    int rem_count = 0;
    for (int i = 0; i < count; i++) {
        if ((new_size -= (fi+i)->size) < cfg.size) {
            rem_count = i;
            break;
        }
    }
    printf ("-----------------------\n");
    char new_s[128] = {0};
    strcpy(new_s, print_pretty_size(new_size));
    printf ("New size: %s (%lud bytes); Need to remove %d files\n",
            print_pretty_size(total_size),
            total_size, 
            new_s,
            new_size, rem_count+1);

    if (dry_run) {
        printf("Running in dry-run mode. Following files plan to be removed:\n");
    }
    for (int i = 0; i <= rem_count; i++) {
        //remove files!!!!
        if (dry_run) {
            printf("[%d] %s: %s (%lu bytes)\n", i, (fi+i)->path, 
                    print_pretty_size((fi+i)->size), (fi+i)->size);
        } else {
            printf("Delete: %s\n", (fi+i)->path);
            unlink((fi+i)->path);
        }
    }
    printf("Done!\n");

    return 0;
}

int read_cfg_file(char * fl) {
    FILE * fp = fopen(fl, "r");
    if (!fp) {
        printf("Can't open config file!\n");
        return -1;
    }
    char line[1024] = {0};
    char param[1024] = {0};
    char value[1024] = {0};
    char * p = 0;
    while (! feof(fp)) {
        if (fgets(line, sizeof(line), fp)) {
            if ((p = strstr(line, "#")) != 0) {
                // skip comments
                *p = '\0';
            }
            line[strlen(line)-1] = '\0';
            p = strstr(line, "=");
            if (p) {
                strncpy(param, line, p-line);
                //printf("Param \"%s\" with value \"%s\"\n", param, p+1);
                if (strcmp(CFG_SIZE, param) == 0) {
                    strncpy(cfg.orig_size, p+1, sizeof(cfg.orig_size));
                    int multi = 1;
                    char * ch = &(line[strlen(line)-1]);
                    switch (line[strlen(line)-1]) {
                        case 'T':
                            multi *= 1000;
                        case 'G':
                            multi *= 1000;
                        case 'M':
                            multi *= 1000;
                        case 'K':
                            multi *= 1000;
                            break;
                        default:
                            multi = 1;
                    }
                    //printf("Value of multiplier is \"%c\" = %d\n", ch, multi);
                    *p = '\0';
                    cfg.size = atoi(p+1) * multi;
                } else
                if (strcmp(CFG_FOLDER, param) == 0) {
                    if(line[strlen(line)-1] == '/')
                        line[strlen(line)-1] = '\0';
                    strncpy(cfg.path, p+1, strlen(line)-(p-line)-1);
                } else {
                    printf("Unknown param \"%s\" with value \"%s\"\n", param, p+1);
                }
            }
        }
    }
    fclose(fp);
    return 0;
}

char * print_pretty_size(int size) {
    char suff = ' ';
    static char buff[128] = {0};
    float h_rsize = size;
    int cycles = 0;
    while (h_rsize > 1000) {
        cycles++;
        h_rsize/=1000;
    }
    switch (cycles) {
        case 1:
            suff = 'K';
            break;
        case 2:
            suff = 'M';
            break;
        case 3:
            suff = 'G';
            break;
        case 4:
            suff = 'T';
            break;
    }
    snprintf(buff, 128, "%2.2f%c", h_rsize, suff);
    //printf("result: %s\n", buff);
    return buff;
}

void print_help(char * name) {
    printf("FS Observer v%s\n", VERSION_NUMBER);
    printf("Usage: %s [-h] [-r] [-c <config_file>]\n", name);
    printf("\t-c specify config file location. Defailt name is \"%s\" in current folder\n", DEF_CONFIG_FILE);
    printf("\t-r Run with removing files. Without this option files for remove just will be printed in console.\n");
    printf("\t-h Print help and exit\n");
}

