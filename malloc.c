#include "malloc.h"

#ifndef CHUNK_DEFAULT_SIZE
#define CHUNK_DEFAULT_SIZE (32*1024)
#endif

#ifndef ALLOCATION_ALIGNMENT
#define ALLOCATION_ALIGNMENT 4
#endif

typedef struct chunkinfo chunkinfo_t;
struct chunkinfo {
    memsize_t size;
    chunkinfo_t *next;
};

chunkinfo_t *freep = 0;
chunkinfo_t *usedp = 0;

#ifdef _WIN32
#include <windows.h>

#define INV_HANDLE(x) (((x) == NULL) || ((x) == INVALID_HANDLE_VALUE))

chunkinfo_t *create_chunk(memsize_t size)
{
    memsize_t size_bytes = size > CHUNK_DEFAULT_SIZE ? size : CHUNK_DEFAULT_SIZE;
    Region *r = VirtualAllocEx(
        GetCurrentProcess(),      /* Allocate in current process address space */
        NULL,                     /* Unknown position */
        size_bytes,               /* Bytes to allocate */
        MEM_COMMIT | MEM_RESERVE, /* Reserve and commit allocated page */
        PAGE_READWRITE            /* Permissions ( Read/Write )*/
    );

    if(INV_HANDLE(x)) {
        return NULL;
    }

    chunk->size = size_bytes;
    chunk->next = 0;
    return chunk;
}

void destroy_chunk(chunkinfo_t *chunk)
{
    if (INV_HANDLE(chunk)) return;

    /* TODO: VirtualFreeEx return a boolean value that indicate error maybe do something on error?*/
    // BOOL free_result =
    VirtualFreeEx(
            GetCurrentProcess(),        /* Deallocate from current process address space */
            (LPVOID)chunk,                  /* Address to deallocate */
            0,                          /* Bytes to deallocate ( Unknown, deallocate entire page ) */
            MEM_RELEASE                 /* Release the page ( And implicitly decommit it ) */
            );
}

#endif

#ifdef __linux__
#include <sys/mman.h>

chunkinfo_t *create_chunk(memsize_t size)
{
    memsize_t chunk_size = size > CHUNK_DEFAULT_SIZE ? size : CHUNK_DEFAULT_SIZE;
    chunkinfo_t *chunk = mmap(0, sizeof(*chunk)+chunk_size, PROT_READ|PROT_WRITE, 
            MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    chunk->size = chunk_size;
    chunk->next = 0;
    return chunk;
}

void destroy_chunk(chunkinfo_t *chunk)
{
    munmap(chunk, chunk->size + sizeof(*chunk));
}
#endif

chunkinfo_t *split_chunk(chunkinfo_t *chunk, memsize_t size)
{
    memsize_t requested_size = sizeof(chunkinfo_t) + size;
    if(chunk == 0 || requested_size >= chunk->size) {
        return 0;
    }

    chunkinfo_t *result = (chunkinfo_t *)((void*)chunk + chunk->size - size);

    result->size = size;
    chunk->size -= requested_size;

    // Linking the chunks is not required
    // result->next = chunk->next;
    // chunk->next  = result;

    return result;
}

void add_to_used_chunks(chunkinfo_t *chunk)
{
    if(usedp) {
        chunk->next = usedp;
    }
    usedp = chunk;
}

void add_to_free_chunks(chunkinfo_t *chunk)
{
    if(!freep) {
        freep = chunk;
        return;
    }

    chunkinfo_t *curr_free = freep;
    chunkinfo_t *next_free = curr_free->next;
    int new_chunk_added = 0;
    while(curr_free != 0) {
        if(chunk > curr_free) {
            if(next_free && chunk > next_free) {
                curr_free = next_free;
                next_free = next_free->next;
                continue;
            }

            curr_free->next = chunk;
            chunk->next = next_free;
            new_chunk_added = 1;
            break;
        }
        curr_free = next_free;
        if(next_free) next_free = next_free->next;
    }

    if(!new_chunk_added) {
        chunk->next = freep;
        freep = chunk;
    }

    while((void *)chunk + sizeof(chunkinfo_t) + chunk->size == chunk->next) {
        chunk->size += chunk->next->size + sizeof(chunkinfo_t);
        chunk->next = chunk->next->next;
    }
}

/*
 * TODO: memalloc is has some flaws
 * - The cost of using memalloc if the allocation is less than sizeof(chunkinfo_t) would be inefficient
 * - The performance may become better if doubly link list is used rather than just a single link list
 * - No threading safety
 */
void *memalloc(memsize_t size)
{
    if(freep == 0) {
        freep = create_chunk(size);
    }
    if(size < ALLOCATION_ALIGNMENT) size = ALLOCATION_ALIGNMENT;
    size = size + ALLOCATION_ALIGNMENT - (size % ALLOCATION_ALIGNMENT);

    chunkinfo_t *prev_free = 0;
    chunkinfo_t *curr_free = freep;
    while(curr_free != 0) {
        if(curr_free->size > size + sizeof(chunkinfo_t)) { // INFO: If the requested size is still less than current free
            chunkinfo_t *result = split_chunk(curr_free, size);
            if(!result) continue;
            add_to_used_chunks(result);
            return (void *)(result + 1);
        } else if(curr_free->size == size) { // INFO: If it's the same size
            if(prev_free) {
                prev_free->next = curr_free->next;
            } else {
                freep = curr_free->next;
            }
            add_to_used_chunks(curr_free);
            return (void *)(curr_free + 1);
        }
        curr_free = curr_free->next;
        prev_free = curr_free;
    }

    chunkinfo_t *new_chunk = create_chunk(size);
    if(new_chunk) {
        if(new_chunk->size == size) { // INFO: If a really big size is requested
            add_to_used_chunks(new_chunk);
            return (void *)(new_chunk + 1);
        } else {
            new_chunk->next = freep;
            freep = new_chunk;
            chunkinfo_t *result = split_chunk(new_chunk, size);
            if(!result) return 0; // ERROR: Something is not right
            add_to_used_chunks(result);
            return (void *)(result + 1);
        }
    }

    return 0;
}

void memfree(void *ptr)
{
    chunkinfo_t *chunk = (chunkinfo_t *)ptr - 1;

    chunkinfo_t *prev_used = 0;
    chunkinfo_t *curr_used = usedp;
    while(curr_used != 0) {
        if(curr_used == chunk) {
            chunkinfo_t *curr_used_next = curr_used->next;
            curr_used->next = 0;
            add_to_free_chunks(curr_used);
            if(prev_used) {
                prev_used->next = curr_used_next;
            } else {
                usedp = curr_used_next;
            }
            break;
        }
        prev_used = curr_used;
        curr_used = curr_used->next;
    }

}
