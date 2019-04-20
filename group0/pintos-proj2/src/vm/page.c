#include <hash.h>
#include <string.h>
#include "lib/kernel/hash.h"

#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "filesys/file.h"


static unsigned spte_hash_func(const struct hash_elem *, void *);
static bool spte_less_func(const struct hash_elem *, const struct hash_elem *, void *);
static void spte_destroy_func(struct hash_elem *, void *);
static bool vm_load_page_from_filesys(struct supplemental_page_table_entry *, void *);



struct supplemental_page_table *
vm_supt_create(void){
    struct supplemental_page_table *supt = 
      (struct supplemental_page_table *)malloc(sizeof(struct supplemental_page_table));
    hash_init(&supt->page_map, spte_hash_func, spte_less_func, NULL);
    return supt;
}

void
vm_supt_destroy(struct supplemental_page_table *supt){
    ASSERT(supt != NULL);
    
    hash_destroy(&supt->page_map, spte_destroy_func);
    free(supt);
}

/* Install a page (specified by the starting address 'upage') which
 * is currently on the frame, in the supplemental page table
 * Return true if successful, and false otherwith
 * (In case of failure, a proper handing is required later -- process.c)
 */
bool
vm_supt_set_page(struct supplemental_page_table *supt, void *upage){
    struct supplemental_page_table_entry *spte;
    spte = (struct supplemental_page_table_entry *)malloc(sizeof(struct supplemental_page_table_entry));
    
    spte->upage = upage;
    spte->status = ON_FRAME;
    spte->swap_index = -1;
    
    struct hash_elem *prev_elem;
    prev_elem = hash_insert(&supt->page_map, &spte->elem);
    if(prev_elem == NULL){
        //successfullu insert into the supplemental page table
        return true;
    }else{
        //failed. there is already an entry
        free(spte);
        return false;
    }
}


/* Install a page (specified by the starting address 'upage')
 * on the supplemental page table. The page is of type ALL_ZERO,
 * indicates that all the bytes is (lazily) zero
 */
bool
vm_supt_install_zeropage(struct supplemental_page_table *supt, void *upage){
    struct supplemental_page_table_entry *spte;
    spte = (struct supplemental_page_table_entry *)malloc(sizeof(struct supplemental_page_table_entry));

    spte->upage = upage;
    spte->status = ALL_ZERO;
   
    struct hash_elem *prev_elem;
    prev_elem = hash_insert(&supt->page_map, &spte->elem);
    if(prev_elem == NULL) return true;
    
    //TODO there is already an entry
    PANIC("Duplicated SUPT entry for zeropage");
    return false;
}


bool
vm_supt_set_swap(struct supplemental_page_table *supt, void *page, swap_index_t swap_index){
    struct supplemental_page_table_entry *spte;
    spte = vm_supt_lookup(supt, page);
    
    if(spte == NULL) return false;
    
    spte->status = ON_SWAP;
    spte->swap_index = swap_index;
    return true;
}

/* Install a page (specified by the starting address 'upage')
 * on the supplemental page table, of type FROM_FILESYS.
 */
bool
vm_supt_install_filesys(struct supplemental_page_table *supt, void *upage, 
struct file *file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable){
     struct supplemental_page_table_entry *spte;
     spte = (struct supplemental_page_table_entry *)malloc(sizeof(struct supplemental_page_table_entry));
     
     spte->upage = upage;
     spte->status = FROM_FILESYS;
     spte->file = file;
     spte->file_offset = offset;
     spte->read_bytes = read_bytes;
     spte->zero_bytes = zero_bytes;
     spte->writable = writable;
     
     struct hash_elem *prev_elem;
     prev_elem = hash_insert(&supt->page_map, &spte->elem);
     if(prev_elem == NULL) return true;
     
     //TODO there is already an entry
     PANIC("Duplicated SUPT entry for filesys-page");
     return false;
}

struct supplemental_page_table_entry *
vm_supt_lookup(struct supplemental_page_table *supt, void *page){
    //create a temprorary object, just for looking up the hash table.
    struct supplemental_page_table_entry spte_temp;
    spte_temp.upage = page;

    struct hash_elem *elem = hash_find(&supt->page_map, &spte_temp.elem);
    if(elem == NULL)
        return NULL;
    return hash_entry(elem, struct supplemental_page_table_entry, elem);
}

bool
vm_supt_has_entry(struct supplemental_page_table *supt, void *page){
    /* Find the SUPT entry. If not found, it's an unmanaged page.*/
    struct supplemental_page_table_entry *spte = vm_supt_lookup(supt, page);
    if(spte == NULL)
        return false;
    return true; 
}


bool
vm_load_page(struct supplemental_page_table *supt, uint32_t *pagedir, void *upage){
    /* see also userprog/exception.c */
    
    // 1. chech if the memomry reference is valid
    struct supplemental_page_table_entry *spte;
    spte = vm_supt_lookup(supt, upage);
    if(spte == NULL)
        return false;
    
    //2. Obtain a frame to store the page
    void *frame_page = vm_frame_allocate(PAL_USER, upage);
    if(frame_page == NULL){
        return false;
    }
    
    //3. Fetch the data into the frame
    bool writable = true;
    switch(spte->status){
        case ALL_ZERO:
            memset(frame_page, 0, PGSIZE);
            break;
        case ON_FRAME:
            /*nothing to do*/
            break;
        case ON_SWAP:
            vm_swap_in(spte->swap_index, frame_page);
            break;
        case FROM_FILESYS:
            if(vm_load_page_from_filesys(spte, frame_page) == false){
                vm_frame_free(frame_page);
                return false;
            }
            writable = spte->writable;
            break;
        default:
            PANIC("unreachable state");
    }

    //4. Point the page table entry for the faulting virtual address to the physical page.
    if(!pagedir_set_page(pagedir, upage, frame_page, writable)){ 
        vm_frame_free(frame_page);
        return false;
    }
    spte->status = ON_FRAME;
    pagedir_set_dirty(pagedir, frame_page, false);

    //unpin frame
    vm_frame_unpin(frame_page);    
    return true;
}



static bool
vm_load_page_from_filesys(struct supplemental_page_table_entry *spte, void *kpage){
    file_seek(spte->file, spte->file_offset);
    
    //read bytes from the file
    int n_read = file_read(spte->file, kpage, spte->read_bytes);
    if(n_read != (int)spte->read_bytes)
        return false;
    
    //remain bytes are just zero
    ASSERT(spte->read_bytes + spte->zero_bytes == PGSIZE);
    memset(kpage + n_read, 0, spte->zero_bytes);
    return true;
}

/***********Helper**********/
//hash function required for [frame_map]. Use 'kaddr' as key
static unsigned
spte_hash_func(const struct hash_elem *elem, void *aux UNUSED){
    struct supplemental_page_table_entry *entry = 
      hash_entry(elem, struct supplemental_page_table_entry, elem);
    return hash_int((int)entry->upage);
}

static bool
spte_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
    struct supplemental_page_table_entry *a_entry = hash_entry(a, struct supplemental_page_table_entry, elem);
    struct supplemental_page_table_entry *b_entry = hash_entry(b, struct supplemental_page_table_entry, elem);
    return a_entry->upage < b_entry->upage;
}

static void
spte_destroy_func(struct hash_elem *elem, void *aux UNUSED){
    struct supplemental_page_table_entry *entry = hash_entry(elem, struct supplemental_page_table_entry, elem);
    free(entry);
}
