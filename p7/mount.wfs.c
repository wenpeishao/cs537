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

struct wfs_log_entry *get_log_entry(unsigned int ino)
{
    // printf("number = %d\n", ino);
    char *ptr = NULL;
    ptr = (char *)((char *)disk + sizeof(struct wfs_sb));
    struct wfs_log_entry *lep = (struct wfs_log_entry *)ptr;
    struct wfs_log_entry *final = NULL;
    // begin with first entry
    for (; ptr < (char *)disk + sb->head; ptr += (sizeof(struct wfs_inode) + lep->inode.size))
    {
        lep = (struct wfs_log_entry *)ptr;
        if (lep->inode.inode_number == ino && lep->inode.deleted == 0)
        {
            final = lep;
        }
    }
    // not found
    return final;
}

struct wfs_inode *get_inode(unsigned inode_number)
{
    // Assuming log_entry is a global array of wfs_log_entry structures
    // and that each entry in this array corresponds to an inode in the filesystem

    // // Check if the inode number is within the valid range
    // if (inode_number >= MAX_INODES)
    // {
    //     return NULL; // Invalid inode number
    // } todo: delete

    // Access the inode from the log_entry array
    struct wfs_log_entry *entry = &log_entry[inode_number];

    // Check if the inode is marked as deleted or is otherwise invalid
    if (entry->inode.deleted)
    {
        return NULL; // Inode is deleted
    }

    return &entry->inode; // Return the inode structure
}

void add_directory_entry(struct wfs_inode *dir_inode, const char *entry_name, unsigned long inode_number)
{
    if (dir_inode == NULL || (dir_inode->mode & S_IFMT) != S_IFDIR)
    {
        return; // Not a directory or null inode
    }

    struct wfs_dentry *dentry = (struct wfs_dentry *)(log_entry[dir_inode->inode_number].data + dir_inode->size);
    strncpy(dentry->name, entry_name, MAX_FILE_NAME_LEN - 1);
    dentry->name[MAX_FILE_NAME_LEN - 1] = '\0'; // Ensure null termination
    dentry->inode_number = inode_number;

    // Update the size of the directory inode to reflect the new entry
    dir_inode->size += sizeof(struct wfs_dentry);
    // Update timestamps
    dir_inode->mtime = dir_inode->ctime = time(NULL);
}

unsigned long get_inode_number(char *name, struct wfs_log_entry *l)
{
    char *end_ptr = (char *)l + (sizeof(struct wfs_inode) + l->inode.size);
    char *cur = l->data; // begin at dentry
    struct wfs_dentry *d_ptr = (struct wfs_dentry *)cur;
    for (; cur < end_ptr; cur += sizeof(struct wfs_dentry))
    {
        d_ptr = (struct wfs_dentry *)cur;
        if (strcmp(name, d_ptr->name) == 0)
        {
            return d_ptr->inode_number;
        }
    }
    // not found
    return -1;
}

struct wfs_log_entry *path_to_logentry(const char *path)
{

    char *path_copy = strdup(path);
    if (path_copy == NULL)
    {
        perror("Failed to duplicate path string");
        return NULL;
    }
    // int slashes = count_slashes(path);

    unsigned long inodenum = 0;

    struct wfs_log_entry *logptr = NULL;

    // find root first
    logptr = get_log_entry(inodenum);

    puts(path);

    char *token = strtok(path_copy, "/");
    while (token != NULL)
    {
        if (logptr == NULL)
        {
            return NULL;
        }
        inodenum = get_inode_number(token, logptr);
        if (inodenum == -1)
        {
            return NULL;
        }

        logptr = get_log_entry(inodenum);

        token = strtok(NULL, "/");
    }
    free(path_copy);
    return logptr;
}

struct wfs_inode *path_to_inode(const char *path)
{
    // Handle special case of root directory
    if (strcmp(path, "/") == 0)
    {
        return get_inode(0); // Function to get the root inode
    }

    struct wfs_inode *current_inode = get_inode(0);
    if (current_inode == NULL)
    {
        return NULL; // Root inode not found
    }

    // Tokenize the path
    char *path_copy = strdup(path);
    char *token = strtok(path_copy, "/");

    while (token != NULL)
    {
        if ((current_inode->mode & S_IFMT) != S_IFDIR)
        {
            free(path_copy);
            return NULL; // Current inode is not a directory
        }

        // Find the inode number for the token
        unsigned long inode_number = inode_for_name(token, &log_entry[current_inode->inode_number]);
        if (inode_number == 0)
        {
            free(path_copy);
            return NULL; // Directory entry not found
        }

        current_inode = get_inode(inode_number);
        if (current_inode == NULL)
        {
            free(path_copy);
            return NULL; // Inode not found
        }

        token = strtok(NULL, "/"); // Get the next component of the path
    }

    free(path_copy);
    return current_inode;
}
unsigned long parent_inode_number(const char *path)
{
    if (strcmp(path, "/") == 0)
    {
        // Root directory has no parent, or it is its own parent
        return 0;
    }

    // Make a copy of the path to work with
    char *path_copy = strdup(path);
    if (path_copy == NULL)
    {
        // Handle memory allocation error
        return -1; // Indicate an error
    }

    // Find the last '/' character - this separates the parent path from the current directory/file
    char *last_slash = strrchr(path_copy, '/');
    if (last_slash == path_copy)
    {
        // The parent is the root directory
        free(path_copy);
        return 0;
    }

    // Terminate the path at the last '/' to get the parent's path
    *last_slash = '\0';

    // Get the inode of the parent directory
    struct wfs_inode *parent_inode = path_to_inode(path_copy);
    unsigned long parent_inode_number = (parent_inode != NULL) ? parent_inode->inode_number : 0;

    free(path_copy); // Free the allocated memory
    return parent_inode_number;
}

void add_inode_to_filesystem(struct wfs_inode *inode)
{
    if (inode == NULL)
    {
        return; // Null inode
    }

    // Assuming log_entry is a global array or similar structure
    // and head is the index for the next free entry in log_entry
    log_entry[sb->head].inode = *inode;

    // Update the head to point to the next free entry
    sb->head += sizeof(struct wfs_log_entry) + inode->size;

    // TODO: check if need to update filesystem metadata if necessary
    // (e.g., updating the number of inodes, free space, etc.)
}

static int my_getattr(const char *path, struct stat *stbuf)
{
    // Implementation of getattr function to retrieve file attributes
    // Fill stbuf structure with the attributes of the file/directory indicated by path
    // ...

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0)
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    }
    else
    {
        struct wfs_inode *inode = path_to_inode(path);
        if (inode == NULL)
        {
            return -ENOENT;
        }
        else
        {
            if (inode->mode == S_IFDIR)
            {
                stbuf->st_mode = S_IFDIR | 00777;
                stbuf->st_nlink = 1; // todo:check 1 or 2
            }
            else
            {
                stbuf->st_mode = S_IFREG | 0444;
                stbuf->st_nlink = 1;
                stbuf->st_size = inode->size;
            }
        }
    }
    return 0;
}
static int my_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    struct wfs_inode *inode = path_to_inode(path);
    if (inode == NULL)
    {
        return -ENOENT; // File not found
    }

    if ((inode->mode & S_IFMT) != S_IFREG)
    {
        return -EISDIR; // Reading a directory is not allowed
    }

    if (offset >= inode->size)
    {
        return 0; // Offset is beyond the end of the file
    }

    if (offset + size > inode->size)
    {
        size = inode->size - offset; // Adjust size to not exceed file length
    }

    // Access the data from the log entry corresponding to the inode
    struct wfs_log_entry *entry = &log_entry[inode->inode_number];

    // Copy the data from entry->data to buf
    memcpy(buf, entry->data + offset, size);

    return size;
}

static int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi)
{
    // Check if the path is the root directory
    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    struct wfs_inode *dir_inode = path_to_inode(path);
    if (dir_inode == NULL || (dir_inode->mode & S_IFMT) != S_IFDIR)
        return -ENOENT; // Not a directory or directory does not exist

    struct wfs_dentry *dentry = (struct wfs_dentry *)log_entry[dir_inode->inode_number].data;
    for (unsigned int i = 0; i < dir_inode->size / sizeof(struct wfs_dentry); ++i)
    {
        if (dentry[i].inode_number != 0)
        { // Skip empty entries
            filler(buf, dentry[i].name, NULL, 0);
        }
    }

    return 0;
}

int my_mkdir(const char *path, mode_t mode)
{
    // Check if the directory already exists
    struct wfs_inode *existing_inode = path_to_inode(path);
    if (existing_inode != NULL)
    {
        // The directory already exists
        return -EEXIST;
    }

    // Allocate a new inode for the directory
    struct wfs_inode *dir_inode = malloc(sizeof(struct wfs_inode));
    if (dir_inode == NULL)
    {
        // Failed to allocate inode
        return -ENOSPC;
    }

    // Initialize the inode for a directory
    dir_inode->mode = S_IFDIR | (mode & 0777);
    dir_inode->size = 0;
    dir_inode->links = 2; // todo: check 1 or 2
    dir_inode->atime = dir_inode->mtime = dir_inode->ctime = time(NULL);

    // Add '.' and '..' entries to the directory
    // You will need a function to add entries to a directory
    add_directory_entry(dir_inode, ".", dir_inode->inode_number);
    add_directory_entry(dir_inode, "..", parent_inode_number(path)); // Parent directory's inode number

    // Update the filesystem's structures to include the new directory
    add_inode_to_filesystem(dir_inode);

    return 0; // Success
}

int my_mknod(const char *path, mode_t mode, dev_t dev)
{
    // Check if the file already exists
    struct wfs_inode *existing_inode = path_to_inode(path);
    if (existing_inode != NULL)
    {
        // The file already exists
        return -EEXIST;
    }

    // Allocate a new inode
    struct wfs_inode *new_inode = malloc(sizeof(struct wfs_inode));
    if (new_inode == NULL)
    {
        // Failed to allocate inode
        return -ENOSPC;
    }

    // Initialize the inode for a regular file
    new_inode->mode = S_IFREG | (mode & 0777);                           // Regular file type with specified permissions
    new_inode->size = 0;                                                 // Initial size for the file
    new_inode->links = 1;                                                // Standard for files
    new_inode->atime = new_inode->mtime = new_inode->ctime = time(NULL); // Set timestamps
    new_inode->inode_number = get_next_inode_number();                   // Function to get the next available inode number
    new_inode->deleted = 0;                                              // File is not deleted

    // Add the inode to the filesystem's structures
    add_inode_to_filesystem(new_inode);

    return 0; // Success
}

int my_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    // Find the inode associated with the path
    struct wfs_inode *inode = path_to_inode(path);
    if (inode == NULL)
    {
        return -ENOENT; // File not found
    }

    if ((inode->mode & S_IFMT) != S_IFREG)
    {
        return -EISDIR; // Can't write to a directory
    }

    // Assuming that the file's data is stored directly in log_entry[inode->inode_number].data
    struct wfs_log_entry *entry = &log_entry[inode->inode_number];

    // Ensure the write does not exceed the filesystem limits
    // You need to handle reallocation if size+offset exceeds current allocation
    if (offset + size > inode->size)
    {
        return -EFBIG; // File too large
    }

    // Perform the write operation
    memcpy(entry->data + offset, buf, size);

    // Update the inode's size if the file has grown
    if (offset + size > inode->size)
    {
        inode->size = offset + size;
    }

    // Update the inode's modification time
    inode->mtime = time(NULL);

    return size; // Return the number of bytes written
}

int my_unlink(const char *path)
{
    // Find the inode associated with the path
    struct wfs_inode *inode = path_to_inode(path);
    if (inode == NULL)
    {
        return -ENOENT; // File not found
    }

    if ((inode->mode & S_IFMT) == S_IFDIR)
    {
        return -EISDIR; // Can't unlink a directory
    }

    // Mark the inode as deleted
    inode->deleted = 1;

    // If your filesystem uses a mechanism to free the space used by the file,
    // implement it here

    return 0; // Success
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
