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
char * print_pretty_size(unsigned long int size);

int main (int argc, char ** argv) {
    char conf_file[1024] = {0};
	char target_size[64] = {0};
	char target_folder[1024] = {0};
    int dry_run = 1;
    int opt = 0;

    snprintf(conf_file, 1024, "%s", DEF_CONFIG_FILE);
    while ((opt = getopt(argc, argv, "hrc:s:f:")) != -1) {
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
			case 's':
                snprintf(target_size, 1024, "%s", optarg);
                break;
			case 'f':
                snprintf(target_folder, 1024, "%s", optarg);
                break;
        }
    }

    if (strlen(conf_file) > 0) {
        memset(&cfg, 0, sizeof(struct global_config));
        if (read_cfg_file(conf_file)) {
            return 0;
        }
    }
    
    if (strlen(target_size) > 0) {
        cfg.size = get_size_from_pretty_str(target_size);
    }
    
    if (strlen(target_folder) > 0) {
        strncpy(cfg.path, target_folder, sizeof(cfg.path));
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
			double res = i;
			unsigned int pers = (res/count)*100;
			printf ("Sorting files %d%% in %d of %d (cycle %d)...\r", j, count, i);
            if ((fi+j)->ctime > (fi+j+1)->ctime) {
                memcpy(&tmp, (fi+j), sizeof(struct file_info));
                memcpy(fi+j, fi+j+1, sizeof(struct file_info));
                memcpy(fi+j+1, &tmp, sizeof(struct file_info));
            }
        }
    }

	printf("\n");

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
	char target_s[128] = {0};
    strcpy(new_s, print_pretty_size(new_size));
    strcpy(target_s, print_pretty_size(total_size));
    printf ("Target size: %s (%lu b) of %s (%lu b); New size is %s (%lu b). Need to remove %d files\n",
            print_pretty_size(cfg.size),
            cfg.size,
            target_s,
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
            if (*line == '#') {
                // skip comments
                continue;
            }
            line[strlen(line)-1] = '\0';
            p = strstr(line, "=");
            if (p) {
                strncpy(param, line, p-line);
                //printf("Param \"%s\" with value \"%s\"\n", param, p+1);
                if (strcmp(CFG_SIZE, param) == 0) {
                    strncpy(cfg.orig_size, p+1, sizeof(cfg.orig_size));
                    unsigned long int multi = 1;
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
                    //printf("Value of multiplier is \"%c\" = %lu\n", ch, multi);
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

char * print_pretty_size(unsigned long int size) {
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

unsigned long int get_size_from_pretty_str(const char *size) {
    unsigned long int multi = 1;
    switch (*(size+strlen(size)-1)) {
        case 't':
        case 'T':
            multi *= 1000;
        case 'g':
        case 'G':
            multi *= 1000;
        case 'm':
        case 'M':
            multi *= 1000;
        case 'k':
        case 'K':
            multi *= 1000;
            break;
        default:
            multi = 1;
    }
    //printf("value of multiplier is \"%c\" = %lu\n", ch, multi);
    return = atoi(size) * multi;
}

void print_help(char * name) {
    printf("FS Observer v%s\n", VERSION_NUMBER);
    printf("Usage: %s [-h] [-r] [-c <config file>] [-s <target size>] [-f <target folder>]\n", name);
    printf("\t-c\tspecify config file location.");
    printf("\t-r\trun with removing files. Without this option files for remove just will be printed in console.\n");
    printf("\t-s\tfolder size quota to limit. It is possible to use human readable letter (e.g. K,M,G,T)\n");
    printf("\t-f\tfolder path to limit\n");
    printf("\t-h\tPrint help and exit\n");
}

