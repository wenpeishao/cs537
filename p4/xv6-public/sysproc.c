#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "psched.h"
#include "spinlock.h"
int sys_fork(void)
{
  return fork();
}

int sys_exit(void)
{
  exit();
  return 0; // not reached
}

int sys_wait(void)
{
  return wait();
}

int sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int sys_getpid(void)
{
  return myproc()->pid;
}

int sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

int sys_sleep(void)
{
  int n;
  struct proc *p = myproc();

  if (argint(0, &n) < 0)
    return -1;

  acquire(&tickslock);
  p->wakeuptime = ticks + n; // Set the wake-up time
  release(&tickslock);

  acquire(&tickslock);
  while (ticks < p->wakeuptime)
  {
    if (p->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);

  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
// In this function, argint() is used to fetch the integer argument n.
// Then, the function checks if n is within the valid range (0 to 20).
// If it is, it updates the nice_value of the current process.
int sys_nice(void)
{
  int n;
  if (argint(0, &n) < 0)
    return -1;

  if (n < 0 || n > 20)
    return -1;

  struct proc *curproc = myproc();
  int prev_nice_value = curproc->nice_value;

  curproc->nice_value = n;

  return prev_nice_value;
}

int sys_getschedstate(void)
{
  struct pschedinfo *psi;
  if (argptr(0, (char **)&psi, sizeof(struct pschedinfo)) < 0)
    return -1;

  return getschedstate(psi);
}
