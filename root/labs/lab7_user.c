#include <u.h>
#include <libc.h>

int main(int argc, char *argv[]) {

  int i;
  void *sem = seminit(1);

  printf("Hello world, this is a user program %d\n", getpid());

  fork();
  printf("Forking: my pid is %d\n", getpid());
  fork();
  printf("Forking: my pid is %d\n", getpid());
  fork();
  printf("Forking: my pid is %d\n", getpid());

  if (getpid() == 1)
    return 0;



  while(1) {
    semdown(sem);
    printf("Msg from process %d", getpid());
    for (i = 0; i < 200000 + 100000 * getpid(); i++);
    printf(" continued...\n");
    semup(sem);
    for (i = 0; i < 200000 + 100000 * getpid(); i++);
  }
  return 0;
}