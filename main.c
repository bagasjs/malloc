#include "malloc.h"
#include <stdio.h>

int main(void)
{
    void *a = memalloc(64);
    void *b = memalloc(64);
    void *c = memalloc(64);
    void *d = memalloc(16);

    memfree(a);
    memfree(b);
    memfree(c);
    memfree(d);
}
