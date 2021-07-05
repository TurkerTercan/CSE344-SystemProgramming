#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

struct Node {
    char* name;
    struct Node *next;
    struct Node *child;
};

int to_upper(char c) {
    if(('a' <= c) && (c <= 'z'))
        return 'A' + (c - 'a');
    return c;
}

int strcmp(const char *first, const char *second) {
    while(*first) {
        if (*first != * second)
            break;
        first++;
        second++;
    }
    return *(const unsigned char*)first - *(const unsigned char*)second;
}

unsigned int mystrlen(const char *s) {
    unsigned int count = 0;
    while(*s != '\0') {
        count++;
        s++;
    }
    return count;
}

char* strcat(char* dest, const char* source) {
    char* ptr = dest + mystrlen(dest);
    while (*source != '\0') {
        *ptr++ = *source++;
    }
    *ptr = '\0';
    return dest;
}

char* strcpy(char* dest, const char* source) {
    if (dest == NULL)
        return NULL;
    char *ptr = dest;
    while(*source != '\0') {
        *dest = *source;
        dest++;
        source++;
    }
    *dest = '\0';
    return ptr;
}


int regexFunction(const char* filename, const char* targetName) {
    int i = 0;
    int j = 0;
    int k = 0;

    while (to_upper(filename[i]) == to_upper(targetName[k + j])) {
        if (filename[i] == '\0' || targetName[k + j] == '\0')
            return 1;
        if (filename[i + 1] == '+') {
            while (to_upper(targetName[k + j + 1]) == to_upper(targetName[k + j])) {
                j++;
            }
            i++;
        }
        i++;
        k++;
    }
    return 0;
}

void add_to_tree(struct Node* head, char* location) {
    int i = mystrlen(head->name) + 1;
    struct Node *first = head;
    while(i < mystrlen(location)) {
        int j = i;
        while (j < mystrlen(location) && location[j] != '/'){
            j++;
        }

        char *temp = (char*) malloc((j - i + 1) * sizeof(char));

        for (int k = 0; k < j - i; ++k) {
            temp[k] = location[i + k];
        }
        temp[j - i] = '\0';

        if (first->next == NULL) {
            first->next = (struct Node*) malloc(sizeof(*head));
            first->next->name = (char*) malloc((mystrlen(temp) + 1) * sizeof(char));
            first->next->child = NULL;
            first->next->next = NULL;
            strcpy(first->next->name, temp);
            first = first->next;
        } else {
            first = first->next;
            while(first->child != NULL && strcmp(first->name, temp) != 0) {
                first = first->child;
            }
            if (strcmp(first->name, temp) != 0) {
                first->child = (struct Node*) malloc(sizeof(*head));
                first->child->name = (char*) malloc((mystrlen(temp) + 1) * sizeof(char));
                first->child->child = NULL;
                first->child->next = NULL;
                strcpy(first->child->name, temp);
                first = first->child;
            }
        }
        free(temp);
        i = j;
        i++;
    }
}

void print_tree(struct Node* head, int row) {
    if (head == NULL)
        return;
    if (row == 0) {
        write(1, head->name, mystrlen(head->name));
        write(1, "\n", 1);
        if (head->next != NULL)
            print_tree(head->next, row + 1);
    } else {
        write(1, "|", 1);
        for(int i = 0; i < row; i++)
            write(1, "--", 2);
        write(1, head->name, mystrlen(head->name));
        write(1, "\n", 1);
        if (head->next != NULL)
            print_tree(head->next, row + 1);
        if (head->child != NULL)
            print_tree(head->child, row);
    }
    free(head->name);
    free(head);
}

int openFile(
        int w_flag, char* w_arg,
        int f_flag, char* f_arg,
        int b_flag, int b_arg,
        int t_flag, char t_arg,
        int p_flag, char* p_arg,
        int l_flag, int l_arg,
        struct Node* head) {
    DIR *dir;
    struct dirent *entry;
    int result = 0;
    int result_sub = 0;

    dir = opendir(w_arg);
    if (dir == NULL) {
        perror("Unable to open directory: ");
        return 1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        int len = mystrlen(entry->d_name) + mystrlen(w_arg) + 2;
        char *location = (char *) malloc(len * sizeof(char));
        location[0] = '\0';
        strcat(location, w_arg);
        strcat(location, "/");
        strcat(location, entry->d_name);
        struct stat sb;
        if (stat(location, &sb) == -1) {
            perror("stat");
            return 1;
        }
        int f_temp;
        int b_temp;
        int t_temp;
        int p_temp;
        int l_temp;
        char permissions[9];
        if (f_flag == 1) {
            if ((regexFunction(f_arg, entry->d_name)) == 1)
                f_temp = 1;
            else
                f_temp = 0;
        } else
            f_temp = 1;
        if (b_flag == 1) {
            if (b_arg == sb.st_size)
                b_temp = 1;
            else
                b_temp = 0;
        } else {
            b_temp = 1;
        }
        if (t_flag == 1) {
            switch (sb.st_mode & S_IFMT) {
                case S_IFBLK:
                    if (t_arg == 'b')
                        t_temp = 1;
                    else
                        t_temp = 0;
                    break;
                case S_IFCHR:
                    if (t_arg == 'c')
                        t_temp = 1;
                    else
                        t_temp = 0;
                    break;
                case S_IFDIR:
                    if (t_arg == 'd')
                        t_temp = 1;
                    else
                        t_temp = 0;
                    break;
                case S_IFIFO:
                    if (t_arg == 'p')
                        t_temp = 1;
                    else
                        t_temp = 0;
                    break;
                case S_IFLNK:
                    if (t_arg == 'l')
                        t_temp = 1;
                    else
                        t_temp = 0;
                    break;
                case S_IFREG:
                    if (t_arg == 'f')
                        t_temp = 1;
                    else
                        t_temp = 0;
                    break;
                case S_IFSOCK:
                    if (t_arg == 's')
                        t_temp = 1;
                    else
                        t_temp = 0;
                    break;
                default:
                    t_temp = 0;
                    break;
            }
        } else
            t_temp = 1;
        if (p_flag == 1) {
            permissions[0] = (sb.st_mode & S_IRUSR) ? 'r' : '-';
            permissions[1] = (sb.st_mode & S_IWUSR) ? 'w' : '-';
            permissions[2] = (sb.st_mode & S_IXUSR) ? 'x' : '-';
            permissions[3] = (sb.st_mode & S_IRGRP) ? 'r' : '-';
            permissions[4] = (sb.st_mode & S_IWGRP) ? 'w' : '-';
            permissions[5] = (sb.st_mode & S_IXGRP) ? 'x' : '-';
            permissions[6] = (sb.st_mode & S_IROTH) ? 'r' : '-';
            permissions[7] = (sb.st_mode & S_IWOTH) ? 'w' : '-';
            permissions[8] = (sb.st_mode & S_IXOTH) ? 'x' : '-';
            if (strcmp(permissions, p_arg) == 0)
                p_temp = 1;
            else
                p_temp = 0;
        } else {
            p_temp = 1;
        }
        if (l_flag == 1) {
            if (sb.st_nlink == l_arg) {
                l_temp = 1;
            } else {
                l_temp = 0;
            }
        } else
            l_temp = 1;
        if (f_temp == 1 && b_temp == 1 && t_temp == 1 && p_temp == 1 && l_temp == 1) {
            add_to_tree(head, location);
            result = 1;
        }
        free(location);
    }

    rewinddir(dir);

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        if (entry->d_type == DT_DIR) {
            int len = mystrlen(entry->d_name) + mystrlen(w_arg) + 2;
            char* location = (char*) malloc(len * sizeof(char));
            location[0] = '\0';
            strcat(location, w_arg);
            strcat(location, "/");
            strcat(location, entry->d_name);
            int temp = 0;
            temp = openFile(w_flag, location, f_flag, f_arg, b_flag, b_arg, t_flag, t_arg, p_flag, p_arg, l_flag, l_arg, head);
            if (result_sub == 0 && temp == 1)
                result_sub = 1;
            free(location);
        }
    }
    closedir(dir);

    if (result == 1)
        return 1;
    if (result_sub == 1)
        return 1;
    return 0;

}

int main(int argc, char* argv[]) {
    int opt;
    int w_flag = 0;
    int f_flag = 0;
    int b_flag = 0;
    int t_flag = 0;
    int p_flag = 0;
    int l_flag = 0;

    char* targetDirectoryPath;
    char* targetFileName;
    int targetFileSize;
    char targetFileType;
    char* targetPermissions;
    int targetLinks;

    while((opt = getopt(argc, argv, "w:f:b:t:p:l:")) != - 1) {
        switch (opt) {
            case 'w':
                targetDirectoryPath = (char*) malloc((mystrlen(optarg) + 1) * sizeof(char));
                if (optarg != NULL)
                    strcpy(targetDirectoryPath, optarg);
                w_flag = 1;
                break;
            case 'f':
                targetFileName = (char*) malloc((mystrlen(optarg) + 1) * sizeof(char));
                if (optarg != NULL)
                    strcpy(targetFileName, optarg);
                f_flag = 1;
                break;
            case 'b':
                targetFileSize = atoi(optarg);
                b_flag = 1;
                break;
            case 't':
                targetFileType =  optarg[0];
                t_flag = 1;
                break;
            case 'p':
                targetPermissions = (char*) malloc((mystrlen(optarg) + 1) * sizeof(char));
                if (optarg != NULL)
                    strcpy(targetPermissions, optarg);
                p_flag = 1;
                break;
            case 'l':
                targetLinks = atoi(optarg);
                l_flag = 1;
                break;
            default:
                write(1, "Error: An unexpected argument occurred\n", 40);
                write(1, "Usage: ./myFind [-w targetLocation] [-f fileName] [-b fileSize] [-t fileType] [-p permissions] [-l links]\n", 107);
                return 1;
        }
    }
    if (optind > argc) {
        write(1, "Error: Excepted argument after options\n", 39);
        write(1, "Usage: ./myFind [-w targetLocation] [-f fileName] [-b fileSize] [-t fileType] [-p permissions] [-l links]\n", 107);
        if (p_flag == 1)
            free(targetPermissions);
        if (f_flag == 1)
            free(targetFileName);
        free(targetDirectoryPath);

        return 1;
    }
    if (argc > optind) {
        write(1, "Error: Too many arguments\n", 27);
        write(1, "Usage: ./myFind [-w targetLocation] [-f fileName] [-b fileSize] [-t fileType] [-p permissions] [-l links]\n", 107);
        if (p_flag == 1)
            free(targetPermissions);
        if (f_flag == 1)
            free(targetFileName);
        free(targetDirectoryPath);

        return 1;
    }

    if (w_flag == 0) {
        write(1, "-w is mandatory parameter\n", 26);
        write(1, "Usage: ./myFind [-w targetLocation] [-f fileName] [-b fileSize] [-t fileType] [-p permissions] [-l links]\n", 106);
        if (p_flag == 1)
            free(targetPermissions);
        if (f_flag == 1)
            free(targetFileName);
        free(targetDirectoryPath);

        return 1;
    }
    if (t_flag == 1 && (targetFileType != 'd' && targetFileType != 's' &&
    targetFileType != 'b' && targetFileType != 'c' && targetFileType != 'f' && targetFileType != 'p' && targetFileType != 'l')) {
        char str[19] = {'-', 't', ' ', targetFileType, ' ', 'i', 's', ' ', 'n', 'o', 't', ' ', 'v', 'a', 'l', 'i', 'd', '\n', '\0'};
        write(1, str, 19);
        write(1, "Use \"d\", \"s\", \"b\", \"c\", \"f\", \"p\", \"l\"\n", 38);
        if (p_flag == 1)
            free(targetPermissions);
        if (f_flag == 1)
            free(targetFileName);
        free(targetDirectoryPath);

        return 1;
    }
    if (p_flag == 1 && mystrlen(targetPermissions) != 9) {
        char *str = (char*) malloc((24 + mystrlen(targetPermissions) * sizeof(char)));
        str[0] = '\0';
        strcat(str, "Invalid permissions: ");
        strcat(str, targetPermissions);
        strcat(str, "\n");
        write(1, str, mystrlen(str) + 1);
        free(str);
        if (p_flag == 1)
            free(targetPermissions);
        if (f_flag == 1)
            free(targetFileName);
        free(targetDirectoryPath);

        return 1;
    }

    struct Node *head = (struct Node*) malloc(sizeof(*head));
    head->name = (char*) malloc((mystrlen(targetDirectoryPath) + 20) * sizeof(char));
    head->child = NULL;
    head->next = NULL;
    strcpy(head->name, targetDirectoryPath);

    int result = openFile(w_flag, targetDirectoryPath,
                    f_flag, targetFileName,
                    b_flag, targetFileSize,
                    t_flag, targetFileType,
                    p_flag, targetPermissions,
                    l_flag, targetLinks, head);

    if (p_flag == 1)
        free(targetPermissions);
    if (f_flag == 1)
        free(targetFileName);
    free(targetDirectoryPath);

    if (result == 0)
        write(2, "No File found!\n", 15);
    else {
        print_tree(head, 0);
    }

    return 0;
}
