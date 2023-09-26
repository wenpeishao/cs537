# XV6 `getlastcat` System Call Implementation

**Name:** Wenpei Shao
**CS Login:** wenpei
**Wisc ID:** 9083215211
**Email:** wshao33@wisc.edu

## Implementation Status:
All features implemented and tested successfully.

## Files Modified:
1. `defs.h`: Added the prototype for `getlastcat`.
2. `syscall.h`: Added the syscall number for `getlastcat`.
3. `syscall.c`: Added the syscall function to the syscall list.
4. `sysproc.c`: Implemented the `getlastcat` syscall.
6. `getlastcat.c`: Created a new user program to test the syscall.
7. `Makefile`: Added `getlastcat` to the `UPROGS` list.
8. `usys.S`: Added the syscall assembly code for getlastcat.
9. `sysfile.c`: Modify sys_open syscall funcation and add last_cat_filename extern char [] in order to record last time which file cat was call.
10. `user.h`: Added the prototype for `getlastcat` user program.