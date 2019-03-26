/*
 * mm_alloc.h
 *
 * A clone of the interface documented in "man 3 malloc".
 */

#pragma once

#include <stdlib.h>
#include <unistd.h>

typedef struct block{
    size_t size;        /*date size*/
    struct block *prev;      /* point to previous block */
    struct block *next;      /* point to next block*/
    int free;          /* is the block free */
    char data[0];  
}block_t;

void *mm_malloc(size_t size);
void *mm_realloc(void *ptr, size_t size);
void mm_free(void *ptr);
size_t align8(size_t);
void split_block(block_t *, size_t);
block_t *extend_heap(block_t *, size_t);
block_t *find_block(block_t *,size_t);
block_t *get_block(void *);
int valid_addr(void *);
block_t *fusion(block_t *);
void copy_block(block_t *src,block_t *dst);
