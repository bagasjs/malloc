#ifndef MALLOC_H_
#define MALLOC_H_

#define MSTATIC_ASSERT(cond) _Static_assert(cond, #cond)

#ifndef NULL
#define NULL 0
#endif

typedef unsigned long int memsize_t;
typedef unsigned char membyte_t;

void *memalloc(memsize_t size);
void memfree(void *ptr);

#endif // MALLOC_H_
