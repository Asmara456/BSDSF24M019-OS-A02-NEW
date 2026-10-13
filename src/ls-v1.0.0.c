/*
* Programming Assignment 02: lsv1.1.0
* Version 1.1.0 adds the -l (long listing) option
* Usage:
*       $ lsv1.1.0
*       $ lsv1.1.0 -l
*       $ lsv1.1.0 -l /home /etc
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <limits.h>
#include <sys/ioctl.h>

extern int errno;

void do_ls(const char *dir, int long_flag);
void do_ls_long(const char *dir);
void mode_to_string(mode_t mode, char *str);
int read_entries(const char *dir, char ***names, int *count, int *maxlen);
void print_columns(char **names, int count, int maxlen);
int get_terminal_width(void);

int main(int argc, char *argv[])
{
    int opt;
    int long_flag = 0;

    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        switch (opt)
        {
        case 'l':
            long_flag = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l] [directory...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        do_ls(".", long_flag);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i], long_flag);
            puts("");
        }
    }
    return 0;
}


/* Convert st_mode into a string like "drwxr-xr-x" */
void mode_to_string(mode_t mode, char *str)
{
    /* 1. file type character */
    if (S_ISREG(mode))       str[0] = '-';
    else if (S_ISDIR(mode))  str[0] = 'd';
    else if (S_ISLNK(mode))  str[0] = 'l';
    else if (S_ISCHR(mode))  str[0] = 'c';
    else if (S_ISBLK(mode))  str[0] = 'b';
    else if (S_ISFIFO(mode)) str[0] = 'p';
    else if (S_ISSOCK(mode)) str[0] = 's';
    else                     str[0] = '?';

    /* 2. owner permissions */
    str[1] = (mode & S_IRUSR) ? 'r' : '-';
    str[2] = (mode & S_IWUSR) ? 'w' : '-';
    str[3] = (mode & S_IXUSR) ? 'x' : '-';
    /* 3. group permissions */
    str[4] = (mode & S_IRGRP) ? 'r' : '-';
    str[5] = (mode & S_IWGRP) ? 'w' : '-';
    str[6] = (mode & S_IXGRP) ? 'x' : '-';
    /* 4. other permissions */
    str[7] = (mode & S_IROTH) ? 'r' : '-';
    str[8] = (mode & S_IWOTH) ? 'w' : '-';
    str[9] = (mode & S_IXOTH) ? 'x' : '-';

    /* 5. special bits: setuid, setgid, sticky */
    if (mode & S_ISUID)
        str[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID)
        str[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX)
        str[9] = (mode & S_IXOTH) ? 't' : 'T';

    str[10] = '\0';
}

void do_ls_long(const char *dir)
{
    struct dirent *entry;
    struct stat st;
    char path[PATH_MAX];

    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    /* PASS 1: find the widest value in each column so output lines up */
    int w_link = 0, w_user = 0, w_group = 0, w_size = 0;
    long long total_blocks = 0;

    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (lstat(path, &st) == -1)
        {
            perror(path);
            continue;
        }
        total_blocks += st.st_blocks;

        char buf[64];
        int len = snprintf(buf, sizeof(buf), "%lu", (unsigned long)st.st_nlink);
        if (len > w_link) w_link = len;

        struct passwd *pw = getpwuid(st.st_uid);
        if (pw) len = strlen(pw->pw_name);
        else    len = snprintf(buf, sizeof(buf), "%u", st.st_uid);
        if (len > w_user) w_user = len;

        struct group *gr = getgrgid(st.st_gid);
        if (gr) len = strlen(gr->gr_name);
        else    len = snprintf(buf, sizeof(buf), "%u", st.st_gid);
        if (len > w_group) w_group = len;

        len = snprintf(buf, sizeof(buf), "%lld", (long long)st.st_size);
        if (len > w_size) w_size = len;
    }

    /* st_blocks counts 512-byte blocks; ls prints 1024-byte blocks */
    printf("total %lld\n", total_blocks / 2);

    /* PASS 2: print every entry */
    rewinddir(dp);
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (lstat(path, &st) == -1)
            continue;

        char perms[11];
        mode_to_string(st.st_mode, perms);

        struct passwd *pw = getpwuid(st.st_uid);
        struct group *gr = getgrgid(st.st_gid);
        char uname[32], gname[32];
        if (pw) snprintf(uname, sizeof(uname), "%s", pw->pw_name);
        else    snprintf(uname, sizeof(uname), "%u", st.st_uid);
        if (gr) snprintf(gname, sizeof(gname), "%s", gr->gr_name);
        else    snprintf(gname, sizeof(gname), "%u", st.st_gid);

        /* ctime() returns "Wed Jun 30 21:49:08 1993\n" */
        time_t mtime = st.st_mtime;
        char *t = ctime(&mtime);
        char timebuf[32];
        time_t now = time(NULL);
        if (now - mtime > 15552000 || mtime > now)     /* older than ~6 months */
            snprintf(timebuf, sizeof(timebuf), "%.6s  %.4s", t + 4, t + 20);
        else
            snprintf(timebuf, sizeof(timebuf), "%.12s", t + 4);

        printf("%s %*lu %-*s %-*s %*lld %s %s",
               perms,
               w_link, (unsigned long)st.st_nlink,
               w_user, uname,
               w_group, gname,
               w_size, (long long)st.st_size,
               timebuf,
               entry->d_name);

        /* symbolic links: show "-> target" */
        if (S_ISLNK(st.st_mode))
        {
            char target[PATH_MAX];
            ssize_t n = readlink(path, target, sizeof(target) - 1);
            if (n != -1)
            {
                target[n] = '\0';
                printf(" -> %s", target);
            }
        }
        printf("\n");
    }

    closedir(dp);
}

void do_ls(const char *dir, int long_flag)
{
    if (long_flag)
    {
        do_ls_long(dir);
        return;
    }

    char **names = NULL;
    int count = 0, maxlen = 0;

    if (read_entries(dir, &names, &count, &maxlen) == -1)
        return;

    print_columns(names, count, maxlen);

    for (int i = 0; i < count; i++)
        free(names[i]);
    free(names);
}

/* Read all non-hidden names into a dynamic array; track the longest name */
int read_entries(const char *dir, char ***names, int *count, int *maxlen)
{
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return -1;
    }

    int capacity = 16;
    char **list = malloc(capacity * sizeof(char *));
    if (list == NULL)
    {
        perror("malloc");
        closedir(dp);
        return -1;
    }

    int n = 0;
    struct dirent *entry;
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        if (n == capacity)                 /* array full: double it */
        {
            capacity *= 2;
            char **tmp = realloc(list, capacity * sizeof(char *));
            if (tmp == NULL)
            {
                perror("realloc");
                break;
            }
            list = tmp;
        }

        list[n] = strdup(entry->d_name);
        if (list[n] == NULL)
        {
            perror("strdup");
            break;
        }
        int len = strlen(list[n]);
        if (len > *maxlen)
            *maxlen = len;
        n++;
    }
    if (errno != 0)
        perror("readdir failed");

    closedir(dp);
    *names = list;
    *count = n;
    return 0;
}

/* Terminal width in characters; falls back to 80 if ioctl fails */
int get_terminal_width(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
        return 80;
    return ws.ws_col;
}

/* Print names in columns, "down then across" */
void print_columns(char **names, int count, int maxlen)
{
    if (count == 0)
        return;

    int term_width = get_terminal_width();
    int col_width = maxlen + 2;                  /* longest name + 2 spaces */
    int num_cols = term_width / col_width;
    if (num_cols < 1)
        num_cols = 1;
    int num_rows = (count + num_cols - 1) / num_cols;   /* ceiling division */

    for (int row = 0; row < num_rows; row++)
    {
        for (int col = 0; col < num_cols; col++)
        {
            int idx = col * num_rows + row;      /* down the column first */
            if (idx >= count)
                break;
            printf("%-*s", col_width, names[idx]);
        }
        printf("\n");
    }
}
