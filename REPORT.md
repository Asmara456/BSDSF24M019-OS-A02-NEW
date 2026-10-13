# Operating Systems - Programming Assignment 02 Report

## Feature 2: ls-v1.1.0 - Long Listing Format

### Q1. What is the crucial difference between stat() and lstat()? When is lstat() more appropriate in ls?

Both system calls fill a `struct stat` with a file's metadata (type, permissions, owner, size, timestamps). The difference is in how they treat a symbolic link:

- `stat()` follows the link and returns information about the file the link points to (the target).
- `lstat()` does not follow the link and returns information about the link itself.

If the path is not a symbolic link, both give the same result.

`lstat()` is the right choice for `ls -l`, because `ls` must show the link itself: its type (`l`), its own permissions, its own size (the length of the target path) and its own timestamp, followed by `name -> target`. If I used `stat()`, a symlink would be shown as its target's type and size, and I could not tell it was a link. A broken (dangling) link would also make `stat()` fail, whereas `lstat()` still works. `lstat()` is also needed later for recursion (-R), so that a symlink to a directory is not followed and does not cause an infinite loop.

### Q2. How can bitwise operators and macros extract file type and permissions from st_mode?

`st_mode` is an integer whose bits are divided into fields. The upper bits (masked by `S_IFMT`) store the file type, and the lower 12 bits store the permission bits and the special bits (setuid, setgid, sticky).

**File type:** the type is obtained by AND-ing with the mask `S_IFMT` and comparing with a constant, for example `(mode & S_IFMT) == S_IFDIR`. The macros `S_ISDIR(mode)`, `S_ISREG(mode)`, `S_ISLNK(mode)` and others do this for us. I use them to pick the first character of the listing (`d`, `-`, `l`, `c`, `b`, `p`, `s`).

**Permissions:** each permission is one bit. AND-ing `st_mode` with a mask isolates that bit: if the result is non-zero, the permission is set.

| Mask | Meaning |
|---|---|
| `S_IRUSR`, `S_IWUSR`, `S_IXUSR` | owner read, write, execute |
| `S_IRGRP`, `S_IWGRP`, `S_IXGRP` | group read, write, execute |
| `S_IROTH`, `S_IWOTH`, `S_IXOTH` | others read, write, execute |

For example, `(mode & S_IRUSR) ? 'r' : '-'` prints `r` if the owner has read permission, and `-` otherwise.

**Special bits:** `S_ISUID`, `S_ISGID` and `S_ISVTX` are tested the same way. If setuid is set, the owner execute position shows `s` (or `S` if execute is not set); setgid does the same in the group position; and the sticky bit shows `t` (or `T`) in the others position.


## Feature 3: ls-v1.2.0 - Column Display (Down Then Across)

### Q1. Explain the logic for printing items in a "down then across" columnar format. Why is a single loop through the filenames insufficient?

In "down then across" format the names are ordered down the first column, then continue at the top of the second column, and so on. I calculate the layout first:

1. Find the length of the longest filename (`maxlen`).
2. Column width = `maxlen + 2` (two spaces of spacing).
3. Number of columns = `terminal_width / column_width` (at least 1).
4. Number of rows = `ceil(total_files / num_columns)`.

Then I print row by row. For row `r` and column `c`, the item printed is `names[c * num_rows + r]`. Moving one column to the right means jumping `num_rows` positions ahead in the array, while moving down a row means moving 1 position ahead.

A single loop that prints `names[0], names[1], names[2]...` in order would print the names across each row (left to right), which is the order of the "across" format, not "down then across". The items that belong in the same row are not neighbours in the array, so the printing has to be organised by rows with an inner loop over columns, and the number of rows must be known before the first line is printed. That is why all the names must be read into an array first.

### Q2. What is the purpose of ioctl here? What are the limitations of a fixed 80-column fallback?

`ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)` asks the terminal driver for the current window size and fills a `struct winsize` (`ws_col` is the number of columns and `ws_row` is the number of rows). I use `ws_col` to calculate how many columns of filenames fit on one line.

If the program only used a fixed width of 80:
- On a wider terminal it would waste space and show fewer columns than could fit.
- On a narrower terminal (for example 60 columns) lines would be longer than the window, so the terminal would wrap them and the columns would be misaligned and hard to read.
- It would not adapt when the user resizes the window.

So ioctl makes the output fit the real window, and 80 is used only as a fallback when the size cannot be obtained (for example when ioctl fails).


## Feature 4: ls-v1.3.0 - Horizontal Column Display (-x)

### Q1. Compare the complexity of the "down then across" (vertical) logic and the "across" (horizontal) logic. Which needs more pre-calculation, and why?

The vertical ("down then across") layout is more complex and needs more pre-calculation. Before printing the first line, the program must know:
- the longest filename and the column width,
- the number of columns that fit (`terminal_width / column_width`),
- the number of rows (`ceil(total_files / num_columns)`).

Only then can it print row by row, using the index formula `names[col * num_rows + row]` for each cell, and it must stop when the index passes the end of the array. The number of rows depends on the total number of files, so the whole list must be known first.

The horizontal ("across") layout needs very little pre-calculation. It only needs the column width and the terminal width. It then goes through the array once, in order, keeping a counter of the current horizontal position. If the next name would not fit on the line, it prints a newline and resets the counter. There is no row count and no index arithmetic, and a single loop is enough.

### Q2. How did the program manage the different display modes (-l, -x, default)? How did it decide which function to call?

I used an integer variable `display_mode` with named constants from an `enum`: `MODE_DEFAULT`, `MODE_LONG` and `MODE_HORIZONTAL`. It starts as `MODE_DEFAULT`. The `getopt()` loop in `main()` sets it to `MODE_LONG` when it sees `-l` and to `MODE_HORIZONTAL` when it sees `-x`. If several options are given, the last one wins.

`main()` passes `display_mode` to `do_ls()`. In `do_ls()`:
- if the mode is `MODE_LONG`, it calls `do_ls_long()`;
- otherwise it reads all the names into an array, and then calls `print_horizontal()` if the mode is `MODE_HORIZONTAL`, or `print_columns()` (down then across) for the default.

Keeping one flag and one decision point means a new display mode can be added later by adding one constant and one branch, without restructuring the program.


## Feature 5: ls-v1.4.0 - Alphabetical Sort

### Q1. Why must all directory entries be read into memory before sorting? What are the drawbacks for directories with millions of files?

`readdir()` returns entries one at a time, in whatever order the file system stores them, and that order is not alphabetical. A sort can only put an item in the right place when it can compare it with all the others, and the last entry returned could belong at the very beginning. So the program cannot print entries as it reads them. It must collect every name first, sort the complete list, and only then display it.

For a directory with millions of files this approach has drawbacks:
- **Memory:** every name is stored (a string plus a pointer), so memory use grows with the number of files. Millions of entries can take hundreds of megabytes.
- **Delay:** nothing is printed until the whole directory has been read and sorted, so the user waits with no output.
- **Sorting time:** `qsort` takes O(n log n) comparisons, which is slow for very large n.
- **Reallocation:** the dynamic array has to be grown repeatedly with `realloc`, which copies data.
- **Stale results:** the directory can change while it is being read, so the sorted list may not match the directory afterwards.

Without sorting (the original version), names are printed as they are read, using constant memory.

### Q2. What is the purpose and signature of the comparison function required by qsort()? How does it work, and why must it take const void * arguments?

`qsort()` is a generic function. It does not know what type of data it sorts, only the base address, the number of elements and the size of each element. To decide the order it calls a function supplied by the programmer: the comparison function.

Signature:

    int compare_names(const void *a, const void *b);

The function receives pointers to two elements of the array and must return:
- a negative value if the first element should come before the second,
- zero if they are equal,
- a positive value if the first should come after the second.

In my program the array holds `char *` strings, so each argument points to a `char *`, which means it is really a `char **`. The function casts the `void *` to `const char **`, dereferences it once to get the string, and returns `strcmp(s1, s2)`:

    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    return strcmp(s1, s2);

The parameters must be `const void *` because `qsort()` has to work with any element type, and `void *` is the generic pointer type in C that can hold the address of any type. The `const` promises that the comparison will not modify the elements being compared. The parameter types must match what `qsort()` expects, so the function can be passed as a function pointer.

