#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>

#define TRUE 1
#define FALSE 0

struct File {
    char permissions[11];
    int links;
    char* owner;
    char* group;
    long int size;
    char* date;
    char* name;
    time_t mod_time;
};

char* polishMonth(int month) {
    char* months[12] = {"sty", "lut", "mar", "kwi", "maj", "cze", "lip", "sie", "wrz", "paź", "lis", "gru"};
    return months[month];
}

char* convertTime(time_t time) {
    char *time_str = malloc(20);
    struct tm *time_info = localtime(&time);
    snprintf(time_str, 20, "%s %2d %02d:%02d", polishMonth(time_info->tm_mon), time_info->tm_mday, time_info->tm_hour, time_info->tm_min);
    return time_str;
}

char* humanReadableSize(long int size) {
    char *hr_size = malloc(10);
    if (size >= 1024 * 1024 * 1024) {
        snprintf(hr_size, 10, "%.1fG", size / (1024.0 * 1024.0 * 1024.0));
    } else if (size >= 1024 * 1024) {
        snprintf(hr_size, 10, "%.1fM", size / (1024.0 * 1024.0));
    } else if (size >= 1024) {
        snprintf(hr_size, 10, "%.1fK", size / 1024.0);
    } else {
        snprintf(hr_size, 10, "%ld", size);
    }
    return hr_size;
}

void set_file_permissions(struct stat *file_stat, struct File *file) {
    // File type
    switch (file_stat->st_mode & S_IFMT) {
        case S_IFBLK:  file->permissions[0] = 'b'; break;
        case S_IFCHR:  file->permissions[0] = 'c'; break; 
        case S_IFDIR:  file->permissions[0] = 'd'; break; 
        case S_IFIFO:  file->permissions[0] = 'p'; break; 
        case S_IFLNK:  file->permissions[0] = 'l'; break; 
        case S_IFREG:  file->permissions[0] = '-'; break; 
        case S_IFSOCK: file->permissions[0] = 's'; break; 
        default:       file->permissions[0] = '?'; break; 
    }

    file->permissions[1] = (file_stat->st_mode & S_IRUSR) ? 'r' : '-';
    file->permissions[2] = (file_stat->st_mode & S_IWUSR) ? 'w' : '-';
    file->permissions[3] = (file_stat->st_mode & S_IXUSR) ? 'x' : '-';

    file->permissions[4] = (file_stat->st_mode & S_IRGRP) ? 'r' : '-';
    file->permissions[5] = (file_stat->st_mode & S_IWGRP) ? 'w' : '-';
    file->permissions[6] = (file_stat->st_mode & S_IXGRP) ? 'x' : '-';

    file->permissions[7] = (file_stat->st_mode & S_IROTH) ? 'r' : '-';
    file->permissions[8] = (file_stat->st_mode & S_IWOTH) ? 'w' : '-';
    file->permissions[9] = (file_stat->st_mode & S_IXOTH) ? 'x' : '-';

    file->permissions[10] = '\0';
}

void printFile(struct File f, int lOption, int hOption, int sizeOption) {
    if (sizeOption) {
        printf("%s %ld\n", f.name, f.size);
    } else if (lOption) {
        char* size_str = hOption ? humanReadableSize(f.size) : NULL;
        printf("%s %d %-5s %-5s", f.permissions, f.links, f.owner, f.group); 
        
        hOption ? printf(" %5s", size_str) : printf(" %5ld", f.size);
    
        printf(" %s %s\n", f.date, f.name);        
        if (hOption) free(size_str);
    } else {
        printf("%s ", f.name);
    }
}

void getFileInfo(struct File *file, struct dirent *DirEnt, struct stat *st) {
    set_file_permissions(st, file);

    file->links = st->st_nlink;

    struct passwd *pw = getpwuid(st->st_uid);
    struct group *gr = getgrgid(st->st_gid);

    file->owner = strdup(pw->pw_name);
    file->group = strdup(gr->gr_name);
    file->size = st->st_size;
    file->date = convertTime(st->st_mtime);
    file->name = strdup(DirEnt->d_name);
    file->mod_time = st->st_mtime;
}

int compareByName(const void *a, const void *b) {
    struct File *fileA = (struct File *)a;
    struct File *fileB = (struct File *)b;
    return strcasecmp(fileA->name, fileB->name);
}

int compareByTime(const void *a, const void *b) {
    struct File *fileA = (struct File *)a;
    struct File *fileB = (struct File *)b;
    return (fileB->mod_time - fileA->mod_time);
}

void listDirectory(const char *dirName, int lFlag, int RFlag, int ignoreHidden, int hFlag, int tFlag, int sizeFlag) {
    DIR *pDIR;
    struct dirent *pDirEnt;
    struct stat st;
    pDIR = opendir(dirName);

    if (pDIR == NULL) {
        fprintf(stderr, "%s %d: opendir() failed (%s)\n", __FILE__, __LINE__, strerror(errno));
        return;
    }

    long int blockTotal = 0;
    int fileCount = 0;
    int fileArraySize = 10;
    struct File *files = malloc(fileArraySize * sizeof(struct File));

    // calculate total blocks and count files
    while ((pDirEnt = readdir(pDIR)) != NULL) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dirName, pDirEnt->d_name);

        if (stat(path, &st) == -1) {
            perror("stat");
            continue;
        }

        if (ignoreHidden && pDirEnt->d_name[0] == '.') {
            continue;
        }

        blockTotal += st.st_blocks;

        // reallocate if the file array is full
        if (fileCount >= fileArraySize) {
            fileArraySize *= 2;
            files = realloc(files, fileArraySize * sizeof(struct File));
        }

        struct File file;
        getFileInfo(&file, pDirEnt, &st);
        files[fileCount++] = file;
    }

    blockTotal /= 2; // blocks of 512 -> 1024

    if (lFlag) {
        printf("total %ld\n", blockTotal);
    }

    // sort the files array
    if (tFlag) {
        qsort(files, fileCount, sizeof(struct File), compareByTime);
    } else {
        qsort(files, fileCount, sizeof(struct File), compareByName);
    }

    // print all files
    for (int i = 0; i < fileCount; i++) {
        printFile(files[i], lFlag, hFlag, sizeFlag);
    }   

    printf("\n");

    if (RFlag) {
        for (int i = 0; i < fileCount; i++) {
            if (files[i].permissions[0] == 'd' && strcmp(files[i].name, ".") != 0 && strcmp(files[i].name, "..") != 0) {
                char path[1024];
                snprintf(path, sizeof(path), "%s/%s", dirName, files[i].name);
                printf("\n%s:\n", path);
                listDirectory(path, lFlag, RFlag, TRUE, hFlag, tFlag, sizeFlag);
            }
        }
    }

    free(files);
    closedir(pDIR);
}


int main(int argc, char *argv[]) {
    int ignoreHidden = TRUE;
    int lFlag = FALSE;
    int RFlag = FALSE;
    int hFlag = FALSE;
    int tFlag = FALSE;
    int sizeFlag = FALSE;
    const char *dirPath = ".";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            lFlag = TRUE;
        } else if (strcmp(argv[i], "-R") == 0) {
            RFlag = TRUE;
            ignoreHidden = FALSE;
        } else if (strcmp(argv[i], "-a") == 0) {
            ignoreHidden = FALSE;
        } else if (strcmp(argv[i], "-h") == 0) {
            hFlag = TRUE;
        } else if (strcmp(argv[i], "-t") == 0) {
            tFlag = TRUE;
        } else if (strcmp(argv[i], "-size") == 0) {
            sizeFlag = TRUE;
        } 
        else if (argv[i][0] == '-') {
            // multiple options
            for (int j = 1; argv[i][j] != '\0'; j++) {
                switch (argv[i][j]) {
                    case 'l': lFlag = TRUE; break;
                    case 'R': RFlag = TRUE; break;
                    case 'a': ignoreHidden = FALSE; break;
                    case 'h': hFlag = TRUE; break;
                    case 't': tFlag = TRUE; break;
                    default:
                        fprintf(stderr, "Unknown option: %c\n", argv[i][j]);
                        return 1;
                }
            }
        } else {
            dirPath = argv[i];
        }
    }

    listDirectory(dirPath, lFlag, RFlag, ignoreHidden, hFlag, tFlag, sizeFlag);

    return 0;
}