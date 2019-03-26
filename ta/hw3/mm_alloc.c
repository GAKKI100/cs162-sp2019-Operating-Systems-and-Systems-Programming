/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines.
 */

#include "mm_alloc.h"
#include <stdlib.h>
#define BLOCK_SIZE sizeof(block_t)
block_t *first_block = NULL;

/* find block with size >= INPUT 
 * if last == NULL or don't find block with
 * size >= INPUT will return NULL
 */
block_t *find_block(block_t *last,size_t size){
    block_t *b = first_block;
    while(b && !(b->free && b->size >= size)){
        last = b;
        b = b->next;
    }
    return b;
}

/* extend a new block in the last block*/
block_t *extend_heap(block_t *last, size_t s){
    block_t *b;
    b = sbrk(0);
    if(sbrk(BLOCK_SIZE + s) == (void *) -1)
        return NULL;
    b->size = s;
    b->next = NULL;
    if(last)
        last->next = b;
    b->prev = last;
    b->free = 0;
    return b;
}

/* split a block into two and the first one with size s*/
void split_block(block_t *b, size_t s){
    block_t *new;
    new = (block_t *)(b->data + s);
    new->size = b->size - s - BLOCK_SIZE;
    new->next = b->next;
    new->prev = b;
    if(new->next){
        new->next->prev = new;
    }
    new->free = 1;
    b->size = s;
    b->next = new;
}

/* align the size */
size_t align8(size_t s){
    if(!(s & 0x7))
        return s;
    return((s >> 3)+ 1) << 3;
}


void *mm_malloc(size_t size) {
    /* YOUR CODE HERE */
    block_t *b;
    block_t *last;
    size_t s = align8(size);
    if(first_block){
        last = first_block;
        b = find_block(last, s);
        if(b){
            if((b->size - s) >= (BLOCK_SIZE + 8))
                split_block(b,s);
            b->free = 0;
        }else{
            b = extend_heap(last, s);
            if(!b)
                return NULL;
        }
    }else{
        b = extend_heap(NULL, s);
        if(!b)
            return NULL;
        first_block = b;
    }
    return b->data;
}


/* copy data from src to dst */
void copy_block(block_t *src, block_t *dst){
    for(size_t i = 0; (i * 8) < src->size && (i * 8) < dst->size; i++)
        dst->data[i] = src->data[i];
}

void *mm_realloc(void *ptr, size_t size) {
    /* YOUR CODE HERE */
    size_t s;
    block_t *b, *new;
    void *newp;
    if(!ptr){
        if(!size)
            return NULL;
        return mm_malloc(size);
    }else{
        if(!size){
            mm_free(ptr);
            return NULL; 
        }
    }
    if(!valid_addr(ptr)){
        return NULL;
    }
    s = align8(size);
    b = get_block(ptr);
    if(b->size >= s){
        if(b->size - s >= (BLOCK_SIZE + 8))
            split_block(b, s);
    }else{
        if(b->next && b->next->free
            && (b->size + BLOCK_SIZE + b->next->size) >= s){
            fusion(b);
            if(b->size - s >= (BLOCK_SIZE + 8))
                split_block(b, s);
        }else{
            newp = mm_malloc(s);
            if(!newp)
                return NULL;
            new = get_block(newp);
            copy_block(b, new);
            mm_free(ptr);
            return newp;
        }
    }
    return ptr;
}

/* ensure the address is bewteen address of the first
 * block and break
 */
int valid_addr(void *ptr){
    if(first_block){
        if((int)ptr > (int)first_block && (int)ptr < (int)sbrk(0)){
            return (char *)ptr == get_block(ptr)->data;
        }
    }
    return 0;
}

block_t *get_block(void* ptr){
    return (block_t *)(ptr -= BLOCK_SIZE);
}

/* if the next block is free, then fuse current block
 * with the next block 
 */
block_t *fusion(block_t *b){
    if(b->next && b->next->free){
        b->size += BLOCK_SIZE + b->next->size;
        b->next = b->next->next;
        if(b->next)
            b->next->prev = b;
    }
    return b;
}

void mm_free(void *ptr) {
    /* YOUR CODE HERE */
    block_t *b;
    if(!valid_addr(ptr) || !ptr)
        return;
    b = get_block(ptr);
    b->free = 1;
    if(b->prev && b->prev->free)
        b = fusion(b->prev);
    if(b->next)
        fusion(b);
    else{
        if(b->prev)
            b->prev->next = NULL;
        else
            first_block = NULL;
        brk(b);
    }
}
