// Test preemption by forking off a child process that just spins forever.
// Let it run for a couple time slices, then kill it.

#include <inc/lib.h>

static void
spawn()
{
    int i = 0;
    while (1) {
        i--;
        cprintf("%d, ", i);
        sys_yield();
        i++;
    }
}

void
umain(int argc, char **argv)
{
    int children = 1;
    int r;
    while (children--) {
        if ((r = fork()) < 0)
            panic("really sucks");
        if (r == 0)
            spawn();
    }

    if (r == 0)
        panic("not reached");
}

