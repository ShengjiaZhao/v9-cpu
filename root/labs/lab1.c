//
// Created by shengjia on 4/15/16.
//

#include <u.h>
#include <lab.h>

cout(char c)
{
  out(1, c);
}

int consolewrite(struct inode *ip, char *buf, int n)
{
  int i, e;
  for (i = 0; i < n; i++) cout(buf[i]);
  return n;
}

void consoleinit()
{
  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read  = consoleread;
}

mainc()
{
  consoleinit();         // console device
  ivec(alltraps);        // trap vector
  stmr(128*1024);        // set timer
  while(1);
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

  // jump (via return) to high memory
  *ksp = (uint)mainc;
  asm(LEV);
}
