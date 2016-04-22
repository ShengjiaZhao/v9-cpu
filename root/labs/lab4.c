//
// Created by shengjia on 4/16/16.
//


#include <u.h>
#include <lab.h>
#include <lab_stdout.h>

struct proc proc[NPROC];
struct proc *u;          // current process
struct proc *init;
char *mem_free;          // memory free list
char *mem_top;           // current top of unused memory
uint mem_sz;             // size of physical memory
uint kreserved;          // start of kernel reserved memory heap
uint *kpdir;             // kernel page directory
uint ticks;
char *memdisk;
int nextpid;

void panic(char *s)
{
  asm(CLI);
  out(1,'p'); out(1,'a'); out(1,'n'); out(1,'i'); out(1,'c'); out(1,':'); out(1,' ');
  while (*s) out(1,*s++);
  out(1,'\n');
  asm(HALT);
}

// page allocator
char *kalloc()
{
  char *r; int e = splhi();
  if (r = mem_free)
    mem_free = *(char **)r;   // If there is an item in free page list, use it
  else if ((uint)(r = mem_top) < P2V+(mem_sz - FSSIZE))
    mem_top += PAGE;          // Otherwise increment mem_top to allocate a new page
  else panic("kalloc failure!");  //XXX need to sleep here!
  splx(e);
  return r;
}

// free a page
void kfree(char *v)
{
  int e = splhi();
  if ((uint)v % PAGE || v < (char *)(P2V+kreserved) || (uint)v >= P2V+(mem_sz - FSSIZE))
    panic("kfree");
  *(char **)v = mem_free;
  mem_free = v;
  splx(e);
}

// a forked child's very first scheduling will switch here
forkret()
{
  asm(POPA); asm(SUSP);
  asm(POPG);
  asm(POPF);
  asm(POPC);
  asm(POPB);
  asm(POPA);
  asm(RTI);
}

// Look in the process table for an UNUSED proc.  If found, change state to EMBRYO and initialize
// state required to run in the kernel.  Otherwise return 0.
struct proc *allocproc()
{
  struct proc *p; char *sp; int e = splhi();

  for (p = proc; p < &proc[NPROC]; p++)
    if (p->state == UNUSED) goto found;
  printf("Warning: allocproc failed because NPROC limit reached");
  splx(e);
  return 0;

  found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  splx(e);

  // allocate kernel stack leaving room for trap frame
  sp = (p->kstack = kalloc()) + PAGE - sizeof(struct trapframe);
  p->tf = (struct trapframe *)sp;

  // set up new context to start executing at forkret
  sp -= 8;
  *(uint *)sp = (uint)forkret;

  p->context = sp;
  return p;
}

// create PTE for a page to map va to pa with perm as status flags
// if the PT do not exist, create it.
void mappage(uint *pd, uint va, uint pa, int perm)
{
  uint *pde, *pte, *pt;
  if (*(pde = &pd[va >> 22]) & PTE_P)
    pt = P2V+(*pde & -PAGE);
  else
    *pde = (V2P+(uint)(pt = memset(kalloc(), 0, PAGE))) | PTE_P | PTE_W | PTE_U;
  pte = &pt[(va >> 12) & 0x3ff];
  if (*pte & PTE_P) {
    printf("*pte=0x%x pd=0x%x va=0x%x pa=0x%x perm=0x%x", *pte, pd, va, pa, perm);
    panic("remap");
  }
  *pte = pa | perm;
}

// hand-craft the first process
void init_start()
{
  char cmd[10], *argv[2];

  // no data/bss segment
  cmd[0] = '/'; cmd[1] = 'e'; cmd[2] = 't'; cmd[3] = 'c'; cmd[4] = '/';
  cmd[5] = 'i'; cmd[6] = 'n'; cmd[7] = 'i'; cmd[8] = 't'; cmd[9] = 0;

  argv[0] = cmd;
  argv[1] = 0;

  printf("Executing the first thread");
  while(1);
  //if (!init_fork()) init_exec(cmd, argv);
  //init_exit(0); // become the idle task
}

void userinit()
{
  char *mem;
  init = allocproc();
  init->pdir = memcpy(kalloc(), kpdir, PAGE);
  // copy the code of the process function to a new memory page
  mem = memcpy(memset(kalloc(), 0, PAGE), (char *)init_start, (uint)userinit - (uint)init_start);
  // map virtual address 0 of the process page table to physical address of the memory page
  mappage(init->pdir, 0, V2P+mem, PTE_P | PTE_W | PTE_U);

  init->sz = PAGE;
  init->tf->sp = PAGE;
  init->tf->fc = USER;
  init->tf->pc = 0;
  safestrcpy(init->name, "initcode", sizeof(init->name));
  init->cwd = namei("/");
  init->state = RUNNABLE;
}

void swtch(int *old, int new) // switch stacks
{
  asm(LEA,0); // a = sp
  asm(LBL,8); // b = old
  asm(SX,0);  // *b = a
  asm(LL,16); // a = new
  asm(SSP);   // sp = a
}

void scheduler()
{
  int n;

  for (n = 0; n < NPROC; n++) {  // XXX do me differently
    proc[n].next = &proc[(n+1)&(NPROC-1)];
    proc[n].prev = &proc[(n-1)&(NPROC-1)];
  }

  u = &proc[0];
  pdir(V2P+(uint)(u->pdir));
  u->state = RUNNING;
  swtch(&n, u->context);
  panic("scheduler returned!\n");
}

void sched() // XXX redesign this better
{
  int n; struct proc *p;
  p = u;

  for (n=0;n<NPROC;n++) {
    u = u->next;
    if (u == &proc[0]) continue;
    if (u->state == RUNNABLE) goto found;
  }
  u = &proc[0];

  found:
  u->state = RUNNING;
  if (p != u) {
    pdir(V2P+(uint)(u->pdir));
    swtch(&p->context, u->context);
  }
  //else printf("spin(%d)\n",u->pid);    XXX else do a wait for interrupt? (which will actually pend because interrupts are turned off here)
}

void trap(uint *sp, double g, double f, int c, int b, int a, int fc, uint *pc)
{
  uint va;
  switch (fc) {
    case FSYS: panic("FSYS from kernel");
    case FSYS + USER:
      panic("No implementation of syscall yet");
    case FTIMER:
    case FTIMER + USER:
      ticks++;
      wakeup(&ticks);

      // force process exit if it has been killed and is in user space
      if (u->killed && (fc & USER)) exit(-1);

      // force process to give up CPU on clock tick
      if (u->state != RUNNING) { printf("pid=%d state=%d\n", u->pid, u->state); panic("!\n"); }
      u->state = RUNNABLE;
      sched();

      if (u->killed && (fc & USER)) exit(-1);
      return;

  }
}

alltraps()
{
  // Push all the parameters into the stack to simulate a cdecl function call
  asm(PSHA);
  asm(PSHB);
  asm(PSHC);
  asm(PSHF);
  asm(PSHG);
  asm(LUSP); asm(PSHA);
  trap();                // registers passed back out by magic reference :^O
  asm(POPA); asm(SUSP);
  asm(POPG);
  asm(POPF);
  asm(POPC);
  asm(POPB);
  asm(POPA);
  asm(RTI);
}

mainc()
{
  kpdir[0] = 0;          // don't need low map anymore
  //consoleinit();         // console device
  ivec(alltraps);        // trap vector
  //binit();               // buffer cache
  //ideinit();             // disk
  stmr(128*1024);        // set timer
  userinit();            // first user process
  printf("Welcome!\n");
  scheduler();           // start running processes
}

main()
{
  int *ksp;                 // temp kernel stack pointer
  static char kstack[256];  // temp kernel stack
  static int endbss;        // last variable in bss segment

  // initialize memory allocation
  mem_top = kreserved = ((uint)&endbss + PAGE + 3) & -PAGE;
  mem_sz = msiz();

  // initialize kernel page table
  setupkvm();
  kpdir[0] = kpdir[(uint)USERTOP >> 22]; // need a 1:1 map of low physical memory for awhile

  // initialize kernel stack pointer
  ksp = ((uint)kstack + sizeof(kstack) - 8) & -8;
  asm(LL, 4);
  asm(SSP);

  // turn on paging
  pdir(kpdir);
  spage(1);
  kpdir = P2V+(uint)kpdir;
  mem_top = P2V+mem_top;

  // jump (via return) to high memory
  ksp = P2V+(((uint)kstack + sizeof(kstack) - 8) & -8);
  *ksp = P2V+(uint)mainc;
  asm(LL, 4);
  asm(SSP);
  asm(LEV);
  // Note that since all addressing in v9 is w.r.t. PC or SP,
  // moving PC and SP to high addresses automatically move all references to that address
  // therefore we do not need to explicitly relocate the code
}