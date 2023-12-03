#include <stddef.h>
#include <stdint.h>

#ifndef MOUNT_WFS_H_
#define MOUNT_WFS_H_

#define MAX_FILE_NAME_LEN 32
#define WFS_MAGIC 0xdeadbeef

#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFBLK 0060000
#define S_IFREG 0100000
#define S_IFIFO 0010000
#define S_IFLNK 0120000
#define S_IFSOCK 0140000

#define MAX_LOG_ENTRIES 100 // Define the maximum number of log entries

struct wfs_sb
{
    uint32_t magic;
    uint32_t head;
};

struct wfs_inode
{
    unsigned int inode_number;
    unsigned int deleted; // 1 if deleted, 0 otherwise
    unsigned int mode;    // type. S_IFDIR if the inode represents a directory or S_IFREG if it's for a file
    unsigned int uid;     // user id
    unsigned int gid;     // group id
    unsigned int flags;   // flags
    unsigned int size;    // size in bytes
    unsigned int atime;   // last access time
    unsigned int mtime;   // last modify time
    unsigned int ctime;   // inode change time (the last time any field of inode is modified)
    unsigned int links;   // number of hard links to this file (this can always be set to 1)
};
// store in data for log entry
struct wfs_dentry
{
    char name[MAX_FILE_NAME_LEN];
    unsigned long inode_number;
};

struct wfs_log_entry
{
    struct wfs_inode inode;
    char data[]; // if it is dir store wfs_dentry
};
struct wfs_inode *get_inode(unsigned inode_number);
unsigned long inode_for_name(char *name, struct wfs_log_entry *log_entry);
struct wfs_inode *path_to_inode(const char *path);

struct wfs_private
{
    int fd;
    void *disk;
    unsigned long len;
    size_t head;
};

#endif
