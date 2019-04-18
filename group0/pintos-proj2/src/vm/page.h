#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "vm/swap.h"
#include "filesys/off_t.h"

/*
 * Indicates a states of page
 */
enum page_status{
    ALL_ZERO,  //all zeros
    ON_FRAME,  //actively in memory
    ON_SWAP,    // swapped (on swap slot)
    FROM_FILESYS  //from filesystem (or executable)
};


/*
 * Supplemental page table. The scope is per-process
 */
struct supplemental_page_table{
    /* The hash table, page->spte*/
    struct hash page_map;
};

struct supplemental_page_table_entry{
    void *upage;    /* Virtual address of the page(the key)*/
    struct hash_elem elem;
    enum page_status status;
    //for ON_SWAP
    swap_index_t swap_index;  /* Stores the swap index if the page is 
                               * swapped out. Only effective when 
                               * status == ON_FRAME
                               */
    //for FROM_FILESYS
    struct file *file;
    off_t file_offset;
    uint32_t read_bytes, zero_bytes;
    bool writable;
};

/*
 * Methods for manipulating supplemental page tables
 */
struct supplemental_page_table *vm_supt_create(void);
void vm_supt_destroy(struct supplemental_page_table *);
bool vm_supt_set_page(struct supplemental_page_table *, void *);
bool vm_supt_install_zeropage(struct supplemental_page_table *, void *);
bool vm_supt_set_swap(struct supplemental_page_table *, void *, swap_index_t);
bool vm_supt_install_filesys(struct supplemental_page_table *, void *, 
struct file *, off_t, uint32_t, uint32_t, bool);


struct supplemental_page_table_entry *vm_supt_lookup(struct supplemental_page_table *, void *);
bool vm_supt_has_entry(struct supplemental_page_table *, void *);
bool vm_load_page(struct supplemental_page_table *, uint32_t *, void *);
#endif

