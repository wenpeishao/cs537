/**
 * This program mounts the filesystem to a mount point, which are specifed by the arguments.
 * The usage is:
 *
 * mount.wfs [FUSE options] disk_path mount_point
 *
 * You need to pass [FUSE options] along with the mount_point to fuse_main as argv.
 * You may assume -s is always passed to mount.wfs as a FUSE option to disable multi-threading.
 */
