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

