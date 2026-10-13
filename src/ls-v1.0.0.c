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

/* display modes */
enum { MODE_DEFAULT = 0, MODE_LONG = 1, MODE_HORIZONTAL = 2 };
/* recursive flag for -R */
int recursive = 0;
/* ANSI colour codes */
#define COLOR_RESET   "\033[0m"
#define COLOR_BLUE    "\033[0;34m"   /* directories */
#define COLOR_GREEN   "\033[0;32m"   /* executables */
#define COLOR_RED     "\033[0;31m"   /* archives */
#define COLOR_PINK    "\033[0;35m"   /* symbolic links */
#define COLOR_REVERSE "\033[7m"      /* special files */

void do_ls(const char *dir, int display_mode);
void do_ls_long(const char *dir, char **names, int count);
int compare_names(const void *a, const void *b);
void mode_to_string(mode_t mode, char *str);
int read_entries(const char *dir, char ***names, int *count, int *maxlen);
void print_columns(const char *dir, char **names, int count, int maxlen);
int get_terminal_width(void);
void print_horizontal(const char *dir, char **names, int count, int maxlen);
int ends_with(const char *s, const char *suffix);
const char *get_color(mode_t mode, const char *name);
void print_name(const char *name, mode_t mode);
void print_cell(const char *dir, const char *name, int width);



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

int main(int argc, char *argv[])
{
    int opt;
    int display_mode = MODE_DEFAULT;     /* default: down-then-across columns */

 while ((opt = getopt(argc, argv, "lxR")) != -1)    {
        switch (opt)
        {
        case 'l':
            display_mode = MODE_LONG;
            break;
        case 'x':
            display_mode = MODE_HORIZONTAL;
            break;
        case 'R':
             recursive = 1;
             break;
        default:
            fprintf(stderr, "Usage: %s [-l | -x] [directory...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        do_ls(".", display_mode);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i], display_mode);
            puts("");
        }
    }
    return 0;
}


/* Print names left to right, wrapping when the line is full */

void do_ls(const char *dir, int display_mode)
{
    char **names = NULL;
    int count = 0, maxlen = 0;

    if (read_entries(dir, &names, &count, &maxlen) == -1)
        return;

    /* sort alphabetically BEFORE any display function is called */
    qsort(names, count, sizeof(char *), compare_names);

    /* header when in recursive mode */
    if (recursive)
        printf("%s:\n", dir);

    if (display_mode == MODE_LONG)
        do_ls_long(dir, names, count);
    else if (display_mode == MODE_HORIZONTAL)
        print_horizontal(dir, names, count, maxlen);
    else
        print_columns(dir, names, count, maxlen);

    /* recurse into subdirectories */
    if (recursive)
    {
        for (int i = 0; i < count; i++)
        {
            char path[PATH_MAX];
            struct stat st;

            snprintf(path, sizeof(path), "%s/%s", dir, names[i]);

            if (lstat(path, &st) == -1)
                continue;

            if (S_ISDIR(st.st_mode))
            {
                printf("\n");
                do_ls(path, display_mode);
            }
        }
    }

    for (int i = 0; i < count; i++)
        free(names[i]);
    free(names);
}
/* Comparison function for qsort: each argument is a pointer to a char* */
int compare_names(const void *a, const void *b)
{
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    return strcmp(s1, s2);
}

/* Long listing of the (already sorted) names array */
void do_ls_long(const char *dir, char **names, int count)
{
    struct stat st;
    char path[PATH_MAX];

    /* PASS 1: find the widest value in each column so output lines up */
    int w_link = 0, w_user = 0, w_group = 0, w_size = 0;
    long long total_blocks = 0;

    for (int i = 0; i < count; i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
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
    for (int i = 0; i < count; i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
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
               "");
        print_name(names[i], st.st_mode);

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
}

/* 1 if string s ends with suffix */
int ends_with(const char *s, const char *suffix)
{
    size_t ls = strlen(s), lx = strlen(suffix);
    return ls >= lx && strcmp(s + ls - lx, suffix) == 0;
}

/* Choose the colour for a file from its st_mode (obtained with lstat) */
const char *get_color(mode_t mode, const char *name)
{
    if (S_ISLNK(mode))
        return COLOR_PINK;
    if (S_ISDIR(mode))
        return COLOR_BLUE;
    if (S_ISCHR(mode) || S_ISBLK(mode) || S_ISFIFO(mode) || S_ISSOCK(mode))
        return COLOR_REVERSE;
    if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
        return COLOR_GREEN;
    if (ends_with(name, ".tar") || ends_with(name, ".gz") || ends_with(name, ".zip"))
        return COLOR_RED;
    return "";
}

/* Print one name in its colour, then reset the colour */
void print_name(const char *name, mode_t mode)
{
    const char *color = get_color(mode, name);
    if (color[0] != '\0')
        printf("%s%s%s", color, name, COLOR_RESET);
    else
        printf("%s", name);
}

/* Print a name in colour, then pad with spaces up to 'width' characters.
   Padding is done by hand because colour codes would break printf("%-*s"). */
void print_cell(const char *dir, const char *name, int width)
{
    char path[PATH_MAX];
    struct stat st;
    mode_t mode = 0;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (lstat(path, &st) == 0)
        mode = st.st_mode;

    print_name(name, mode);

    int pad = width - (int)strlen(name);
    for (int i = 0; i < pad; i++)
        putchar(' ');
}

/* Print names in columns, "down then across" */
void print_columns(const char *dir, char **names, int count, int maxlen)
{
    if (count == 0)
        return;

    int term_width = get_terminal_width();
    int col_width = maxlen + 2;
    int num_cols = term_width / col_width;
    if (num_cols < 1)
        num_cols = 1;
    int num_rows = (count + num_cols - 1) / num_cols;

    for (int row = 0; row < num_rows; row++)
    {
        for (int col = 0; col < num_cols; col++)
        {
            int idx = col * num_rows + row;
            if (idx >= count)
                break;
            print_cell(dir, names[idx], col_width);
        }
        printf("\n");
    }
}

/* Print names left to right, wrapping when the line is full */
void print_horizontal(const char *dir, char **names, int count, int maxlen)
{
    int term_width = get_terminal_width();
    int col_width = maxlen + 2;
    int cur_pos = 0;

    for (int i = 0; i < count; i++)
    {
        if (cur_pos > 0 && cur_pos + col_width > term_width)
        {
            printf("\n");
            cur_pos = 0;
        }
        print_cell(dir, names[i], col_width);
        cur_pos += col_width;
    }
    if (cur_pos > 0)
        printf("\n");
}
