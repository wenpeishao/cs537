#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "psched.h"

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
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;

  acquire(&tickslock);
  ticks0 = ticks;

  // Set the wakeup time for the process.
  myproc()->wakeup_ticks = ticks0 + n;

  // Sleep until the wakeup time is reached.
  while (ticks < myproc()->wakeup_ticks)
  {
    if (myproc()->killed)
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

int sys_nice(void)
{
  int n;
  // Get the argument 'n' passed to the system call.
  if (argint(0, &n) < 0)
    return -1;
  // Check if the value of 'n' is out of bounds.
  if (n < 0 || n > 20)
    return -1;

  struct proc *curproc = myproc();
  // Acquire the lock for the process table, if necessary, to ensure atomicity.
  // acquire(&ptable.lock);
  int prev_nice = curproc->nice;
  curproc->nice = n;
  // Release the lock.
  // release(&ptable.lock);

  return prev_nice;
}

int sys_getschedstate(void)
{
  struct pschedinfo *result;
  if (argptr(0, (void *)&result, sizeof(struct pschedinfo)) < 0)
  {
    return -1;
  }
  if (result == 0)
  {
    return -1;
  }

  // acquire(&ptable.lock);

  // for(int i = 0; i < NPROC; i++) {
  //   struct proc *p = &ptable.proc[i];
  //   pinfo->inuse[i] = (p->state != UNUSED);
  //   pinfo->priority[i] = p->priority;
  //   pinfo->nice[i] = p->nice;
  //   pinfo->pid[i] = p->pid;
  //   pinfo->ticks[i] = p->ticks;
  // }

  // // Release the lock.
  // release(&ptable.lock);

  getschedstate(result);
  // proc_gather_pstat(result);

  return 0;
}
