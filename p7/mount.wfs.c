/**
 * This program mounts the filesystem to a mount point, which are specifed by the arguments.
 * The usage is:
 *
 * mount.wfs [FUSE options] disk_path mount_point
 *
 * You need to pass [FUSE options] along with the mount_point to fuse_main as argv.
 * You may assume -s is always passed to mount.wfs as a FUSE option to disable multi-threading.
 */
#define FUSE_USE_VERSION 30
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <fuse.h>
#include "wfs.h"

struct wfs_sb *sb = NULL;
void *disk = NULL;
struct wfs_log_entry *log_entry;

struct wfs_log_entry *find_parent_log_path(const char *path)
{
    if (path == NULL)
    {
        return NULL;
    }

    // Handle root directory separately
    if (strcmp(path, "/") == 0 || strcmp(path, "") == 0)
    {
        return get_log_entry(0); // Assuming 0 is the root inode number
    }

    // Find the last occurrence of '/'
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL)
    {
        return NULL; // No slash found - invalid path
    }

    // Calculate the length of the substring before the last slash
    size_t length = last_slash - path;
    if (length == 0)
    {
        // Parent is the root directory
        return get_log_entry(0);
    }

    // Allocate memory for the new substring (parent path)
    char *parent_path = malloc(length + 1); // +1 for null terminator
    if (parent_path == NULL)
    {
        return NULL; // Memory allocation failed
    }

    // Copy the substring into the new string
    strncpy(parent_path, path, length);
    parent_path[length] = '\0'; // Null-terminate the string

    // Find the log entry for the parent path
    struct wfs_log_entry *logptr = path_to_logentry(parent_path);
    free(parent_path);

    return logptr;
}

unsigned int Max_InodeNum()
{
    char *currentPtr = (char *)((char *)disk + sizeof(struct wfs_sb));
    struct wfs_log_entry *currentLogEntry = (struct wfs_log_entry *)currentPtr;
    unsigned int maxInodeNum = 0;

    while (currentPtr < (char *)disk + sb->head)
    {
        currentLogEntry = (struct wfs_log_entry *)currentPtr;
        if (currentLogEntry->inode.inode_number > maxInodeNum && !currentLogEntry->inode.deleted)
        {
            maxInodeNum = currentLogEntry->inode.inode_number;
        }
        currentPtr += sizeof(struct wfs_inode) + currentLogEntry->inode.size;
    }
    return maxInodeNum;
}

struct wfs_log_entry *get_log_entry(unsigned int inodeNumber)
{
    char *currentPtr = (char *)((char *)disk + sizeof(struct wfs_sb));
    struct wfs_log_entry *currentLogEntry = (struct wfs_log_entry *)currentPtr;
    struct wfs_log_entry *targetLogEntry = NULL;

    while (currentPtr < (char *)disk + sb->head)
    {
        currentLogEntry = (struct wfs_log_entry *)currentPtr;
        if (currentLogEntry->inode.inode_number == inodeNumber && !currentLogEntry->inode.deleted)
        {
            targetLogEntry = currentLogEntry;
            break;
        }
        currentPtr += sizeof(struct wfs_inode) + currentLogEntry->inode.size;
    }
    return targetLogEntry;
}

struct wfs_log_entry *path_to_logentry(const char *path)
{
    char *pathCopy = strdup(path);
    if (!pathCopy)
    {
        perror("Failed to duplicate path string");
        return NULL;
    }

    unsigned long inodeNum = 0;
    struct wfs_log_entry *currentLogEntry = get_log_entry(inodeNum);
    char *token = strtok(pathCopy, "/");

    while (token)
    {
        if (!currentLogEntry)
        {
            free(pathCopy);
            return NULL;
        }
        inodeNum = get_inode_number(token, currentLogEntry);
        if (inodeNum == (unsigned long)-1)
        {
            free(pathCopy);
            return NULL;
        }

        currentLogEntry = get_log_entry(inodeNum);
        token = strtok(NULL, "/");
    }
    free(pathCopy);
    return currentLogEntry;
}

unsigned long get_inode_number(char *name, struct wfs_log_entry *logEntry)
{
    char *endPtr = (char *)logEntry + sizeof(struct wfs_inode) + logEntry->inode.size;
    for (char *currentPtr = logEntry->data; currentPtr < endPtr; currentPtr += sizeof(struct wfs_dentry))
    {
        struct wfs_dentry *dentry = (struct wfs_dentry *)currentPtr;
        if (strcmp(name, dentry->name) == 0)
        {
            return dentry->inode_number;
        }
    }
    return (unsigned long)-1;
}

static int my_getattr(const char *path, struct stat *stbuf)
{
    memset(stbuf, 0, sizeof(struct stat));

    struct wfs_log_entry *log_entry = path_to_logentry(path);
    if (log_entry == NULL)
    {
        return -ENOENT; // No such file or directory
    }
    stbuf->st_mode = log_entry->inode.mode;
    stbuf->st_nlink = log_entry->inode.links;
    stbuf->st_size = log_entry->inode.size;
    stbuf->st_uid = log_entry->inode.uid;
    stbuf->st_gid = log_entry->inode.gid;
    stbuf->st_mtime = log_entry->inode.mtime;
    return 0;
}

static int my_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int len;
    int ret = 0;
    struct wfs_log_entry *file_entry = path_to_logentry(path);

    len = file_entry->inode.size;

    if (file_entry == NULL)
    {
        // File not found or error
        return -ENOENT;
    }

    if (offset < len)
    {
        if (offset + size > len)
        {
            size = len - offset;
        }
        memcpy(buf, file_entry->data + offset, size);
        ret = size;
    }
    else
    {
        ret = 0; // Offset is past the end of the file, overflow
    }

    return ret;
}
static int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    struct wfs_log_entry *l = path_to_logentry(path);
    struct wfs_log_entry *parent = find_parent_log_path(path);

    if (l == NULL || parent == NULL)
    {
        return -ENOENT;
    }

    struct stat st;

    // Current directory (".")
    memset(&st, 0, sizeof(st));
    st.st_ino = l->inode.inode_number;
    st.st_mode = l->inode.mode;
    if (filler(buf, ".", &st, 0))
        return 0;

    // Parent directory ("..")
    memset(&st, 0, sizeof(st));
    st.st_ino = parent->inode.inode_number;
    st.st_mode = parent->inode.mode;
    if (filler(buf, "..", &st, 0))
        return 0;

    // Iterate over the directory's entries
    char *end_ptr = (char *)l + (sizeof(struct wfs_inode) + l->inode.size);
    for (char *cur = l->data; cur < end_ptr; cur += sizeof(struct wfs_dentry))
    {
        struct wfs_dentry *d_ptr = (struct wfs_dentry *)cur;

        if (get_log_entry(d_ptr->inode_number)->inode.deleted == 0)
        {
            memset(&st, 0, sizeof(st));
            st.st_ino = d_ptr->inode_number;
            st.st_mode = get_log_entry(d_ptr->inode_number)->inode.mode;

            if (filler(buf, d_ptr->name, &st, 0))
                break;
        }
    }

    return 0;
}

int my_mknod(const char *path, mode_t mode, dev_t dev)
{
    if (path == NULL)
    {
        return -EINVAL; // Invalid argument
    }
    const char *pos = strrchr(path, '/'); // Find the last '/'
    const char *fileName;
    if (pos != NULL)
    {
        fileName = pos + 1;
    }
    else // No '/' found
    {
        fileName = path;
    }

    struct wfs_log_entry *parent_log = find_parent_log_path(path);
    unsigned int newNum = Max_InodeNum() + 1;

    if (get_inode_number((char *)fileName, parent_log) != -1)
    {
        return -EEXIST;
    }
    // append new parent log
    struct wfs_log_entry *newLog = (struct wfs_log_entry *)((char *)disk + sb->head);

    memcpy((char *)newLog, (char *)parent_log, sizeof(struct wfs_log_entry) + parent_log->inode.size);

    struct wfs_dentry *new_dentry = (void *)((char *)newLog->data + newLog->inode.size);
    strcpy(new_dentry->name, fileName);
    new_dentry->inode_number = newNum;
    // todo:here
    //  update size and head
    newLog->inode.size = newLog->inode.size + sizeof(struct wfs_dentry);
    sb->head = sb->head + (uint32_t)(sizeof(struct wfs_inode) + newLog->inode.size);

    // append new log for new inode
    struct wfs_log_entry *new_file_entry = (struct wfs_log_entry *)((char *)disk + sb->head);
    new_file_entry->inode.inode_number = newNum;
    new_file_entry->inode.atime = time(NULL);
    new_file_entry->inode.mtime = time(NULL);
    new_file_entry->inode.ctime = time(NULL);
    new_file_entry->inode.deleted = 0;
    new_file_entry->inode.flags = 0;
    new_file_entry->inode.gid = getuid();
    new_file_entry->inode.uid = getuid();
    new_file_entry->inode.links = 1;
    new_file_entry->inode.mode = mode | S_IFREG;
    new_file_entry->inode.size = 0;

    sb->head = sb->head + (uint32_t)(sizeof(struct wfs_inode));

    return 0;
}
int my_mkdir(const char *path, mode_t mode)
{
    if (path == NULL)
    {
        return -EINVAL; // Invalid argument
    }

    // Find the last occurrence of '/'
    const char *last_slash = strrchr(path, '/');
    const char *file_name;
    if (last_slash != NULL)
    {
        // Return the substring after the last slash
        file_name = last_slash + 1;
    }
    else
    {
        // The path does not have a slash, use the whole path as the file name
        file_name = path;
    }

    struct wfs_log_entry *parent_log = find_parent_log_path(path);
    unsigned int new_inodenum = Max_InodeNum() + 1;

    if (get_inode_number((char *)file_name, parent_log) != -1)
    {
        return -EEXIST;
    }

    // append new parent log
    struct wfs_log_entry *new_parent_log = (struct wfs_log_entry *)((char *)disk + sb->head);
    memcpy((char *)new_parent_log, (char *)parent_log, sizeof(struct wfs_log_entry) + parent_log->inode.size);
    // *new_parent_log = *parent_log;

    struct wfs_dentry *new_dentry = (void *)((char *)new_parent_log->data + new_parent_log->inode.size);
    strcpy(new_dentry->name, file_name);
    new_dentry->inode_number = new_inodenum;

    // update size and head
    new_parent_log->inode.size = new_parent_log->inode.size + sizeof(struct wfs_dentry);
    sb->head = sb->head + (uint32_t)(sizeof(struct wfs_inode) + new_parent_log->inode.size);

    // append new log for new inode
    struct wfs_log_entry *new_file_entry = (struct wfs_log_entry *)((char *)disk + sb->head);
    new_file_entry->inode.inode_number = new_inodenum;
    new_file_entry->inode.atime = time(NULL);
    new_file_entry->inode.mtime = time(NULL);
    new_file_entry->inode.ctime = time(NULL);
    new_file_entry->inode.deleted = 0;
    new_file_entry->inode.flags = 0;
    new_file_entry->inode.gid = getuid();
    new_file_entry->inode.uid = getuid();
    new_file_entry->inode.links = 1;
    new_file_entry->inode.mode = S_IFDIR | mode;
    new_file_entry->inode.size = 0;

    sb->head = sb->head + (uint32_t)(sizeof(struct wfs_inode));
    return 0;
}

static int my_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    // append one new logentry of itself
    if (offset < 0)
    {
        // bad write
        return 0;
    }
    struct wfs_log_entry *old_log = path_to_logentry(path);
    if (old_log == NULL)
    {
        return -ENOENT;
    }

    int new_size = old_log->inode.size;

    if (offset + size > old_log->inode.size)
    {
        // update size
        new_size = offset + size;
    }
    struct wfs_log_entry *new_log = (struct wfs_log_entry *)((char *)disk + sb->head);
    memcpy((char *)new_log, (char *)old_log, sizeof(struct wfs_inode) + old_log->inode.size);
    new_log->inode.size = new_size;
    memcpy((char *)new_log->data + offset, (char *)buf, size);
    new_log->inode.mtime = time(NULL);

    sb->head = sb->head + (uint32_t)(sizeof(struct wfs_inode)) + new_log->inode.size;
    return size;
}

int my_unlink(const char *path)
{
    struct wfs_log_entry *file_entry = path_to_logentry(path);
    if (file_entry == NULL)
    {
        return -ENOENT; // File not found
    }

    if ((file_entry->inode.mode & S_IFMT) == S_IFDIR)
    {
        return -EISDIR; // Can't unlink a directory
    }

    // Mark the inode as deleted
    file_entry->inode.deleted = 1;
    return 0;
}

static struct fuse_operations my_operations = {
    .getattr = my_getattr,
    .mknod = my_mknod,
    .mkdir = my_mkdir,
    .read = my_read,
    .write = my_write,
    .readdir = my_readdir,
    .unlink = my_unlink,
};

int main(int argc, char *argv[])
{

    // Filter argc and argv here and then pass it to fuse_main
    if (argc < 3)
    {
        printf("Usage: mount.wfs [FUSE options] disk_path mount_point\n");
        return 1;
    }
    // should be like ./mount.wfs -f -s disk mnt or without -f
    int i;
    char *disk_arg = NULL;
    // Filter argc and argv here and then pass it to fuse_main

    // Iterate over arguments
    for (i = 1; i < argc; i++)
    {
        if (!(strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "-s") == 0))
        {
            disk_arg = argv[i]; // Save the disk argument
            // Remove 'disk' from argv
            memmove(&argv[i], &argv[i + 1], (argc - i - 1) * sizeof(char *));
            argc--;
            i--;
            break;
        }
    }

    // open disk
    int fd = open(disk_arg, O_RDWR | O_CREAT, 0666);
    if (fd < 0)
    {
        perror("Failed to open disk image");
        exit(1);
    }

    // get the file size
    struct stat s;
    if (fstat(fd, &s) == -1)
    {
        perror("Failed to get file size");
        close(fd);
        exit(1);
    }

    // mmap disk
    disk = mmap(NULL, s.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk == MAP_FAILED)
    {
        perror("Failed to map file");
        close(fd);
        exit(1);
    }

    sb = (struct wfs_sb *)disk;
    int fuse_return_value = fuse_main(argc, argv, &my_operations, NULL);

    if (munmap(disk, s.st_size) == -1)
    {
        perror("Failed to unmap memory");
    }

    close(fd);

    return fuse_return_value;
}
