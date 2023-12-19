#include <types.h>
#include <mmap.h>
#include <fork.h>
#include <v2p.h>
#include <page.h>

/* 
 * You may define macros and other helper functions here
 * You must not declare and use any static/global variables 
 * */

#define _4KB 4096
#define ALIGN(size) ((size + _4KB - 1) & ~(_4KB - 1))

// HELPER FUNCTIONS

void* os_alloc_vm_struct(int prot, unsigned long start, unsigned long end){
    struct vm_area* vm = os_alloc(sizeof(struct vm_area));
    if (!vm) {
        return NULL;
    }
    vm->access_flags = prot;
    vm->vm_start = start;
    vm->vm_end = end;
    vm->vm_next = NULL;

    return vm;
}

int merge_(struct vm_area* prev, struct vm_area* vm, struct vm_area* iter) {
    // merging check
    // HEURISTIC: if return value: 0: no merging done, 1: left-merge, 2: right-merge, 3: both-merge
    int return_val = 0; 
    if (prev != NULL && prev->vm_start != MMAP_AREA_START) {
        if (prev->access_flags == vm->access_flags && prev->vm_end == vm->vm_start) {
            // merge prev and vm
            prev->vm_end = vm->vm_end;
            prev->vm_next = vm->vm_next;
            vm->vm_next = NULL;
            os_free(vm, sizeof(struct vm_area));
            stats->num_vm_area -= 1;
            vm = prev;

            return_val += 1;
        }
    }
    if (iter != NULL) {
        if (vm->access_flags == iter->access_flags && vm->vm_end == iter->vm_start) {
            // merge vm and iter
            vm->vm_end = iter->vm_end;
            vm->vm_next = iter->vm_next;
            iter->vm_next = NULL;
            os_free(iter, sizeof(struct vm_area));
            stats->num_vm_area -= 1;

            return_val += 2;
        }
    }
    return return_val;
}

long find_first_and_mmap(struct exec_context* current, int prot, u64 length) {
    long return_address = -1;
    struct vm_area* prev = current->vm_area;
    struct vm_area* iter = prev->vm_next;
    struct vm_area* vm = NULL;
    
    // finding a suitable chunk
    if (iter == NULL) {
        // only the dummy_node present
        vm = os_alloc_vm_struct(prot, prev->vm_end, prev->vm_end + length);
        if (vm == NULL) { // failed allocation
            return -1;
        }

        prev->vm_next = vm;
        stats->num_vm_area += 1;

        return_address = vm->vm_start;
        // no merging required

        return return_address;
    } else {
        while(iter != NULL) {
            // checking for a suitable chunk
            if (prev->vm_end + length <= iter->vm_start) {
                vm = os_alloc_vm_struct(prot, prev->vm_end, prev->vm_end + length);
                if (vm == NULL) { // failed allocation
                    return -1;
                }
                return_address = vm->vm_start;
                // adding the node
                prev->vm_next = vm;
                vm->vm_next = iter;
                stats->num_vm_area += 1;
                merge_(prev, vm, iter);

                return return_address;
            } else {
                prev = iter;
                iter = iter->vm_next;
            }
        }

        // iter == NULL {no suitable chunk within}
        vm = os_alloc_vm_struct(prot, prev->vm_end, prev->vm_end + length);
        prev->vm_next = vm;
        stats->num_vm_area += 1;
        return_address = vm->vm_start;
        merge_(prev, vm, iter);

        return return_address;
    }
}

void free_pte(struct exec_context* current, struct vm_area* vm, u64 start_addr, u64 end_addr) {
    u64 virtual_address = start_addr;

    while(virtual_address < end_addr) {
        u64 PFN = current->pgd;
        u64* TABLE_POINTER_V = (u64*)(osmap(PFN));
        u64 temp = virtual_address;

        virtual_address = (virtual_address >> 12); // removing the offset
        int splits[4];
        for(int i = 3; i >= 0; i--) {
            splits[i] = ((1 << 9) - 1) & virtual_address;
            virtual_address = virtual_address >> 9;
        }

        int break_ = 0;

        for (int i = 0; i < 4; i++) {
            u64 entry = *(TABLE_POINTER_V + splits[i]);
            if ((entry & 0x1) == 0) {
                // the physical mapping is not present/intermediate tables not present
                // this is not a error (just the physical mapping is non-existent)
                break_ = 1;
                break;
            } else {
                PFN = (entry >> 12);
            }
            if (i < 3) {
                TABLE_POINTER_V = (u64*)(osmap(PFN));
            }
        }

        if (break_ == 0) {
            // this is when the above loop doesn't breaks
            // which means that currently PFN is that of the page-frame

            // updating the entry in pte_t (Level 4)
            *(TABLE_POINTER_V + splits[3]) = 0x0; // invalidate the entry of this page
            
            // free the page frame if possible
            if (get_pfn_refcount(PFN) == 1) {
                put_pfn(PFN);
                os_pfn_free(USER_REG, PFN);
            } else {
                put_pfn(PFN);
            }            

            // invalidate the TLB
            asm volatile("invlpg (%0)" ::"r" (temp) : "memory");
        }
        virtual_address = temp + _4KB;
    }
}

void update_pte(struct exec_context* current, struct vm_area* vm, u64 start_addr, u64 end_addr, int prot) {
    u64 virtual_address = start_addr;

    while(virtual_address < end_addr) {
        u64 PFN = current->pgd;
        u64* TABLE_POINTER_V = (u64*)(osmap(PFN));
        u64 temp = virtual_address;

        virtual_address = (virtual_address >> 12); // removing the offset
        int splits[4];
        for(int i = 3; i >= 0; i--) {
            splits[i] = ((1 << 9) - 1) & virtual_address;
            virtual_address = virtual_address >> 9;
        }

        int break_ = 0;

        for (int i = 0; i < 4; i++) {
            u64 entry = *(TABLE_POINTER_V + splits[i]);
            if ((entry & 0x1) == 0) {
                // the physical mapping is not present/intermediate tables not present
                // this is not a error (just the physical mapping is non-existent)
                break_ = 1;
                break;
            } else {
                PFN = (entry >> 12);
            }
            if (i < 3) {
                TABLE_POINTER_V = (u64*)(osmap(PFN));
            }
        }

        if (break_ == 0) {
            // this is when the above loop doesn't breaks
            // which means that currently PFN is that of the page-frame

            // updating the entry in pte_t (Level 4)
            u64 PFN_MEM = *(TABLE_POINTER_V + splits[3]) >> 12;
            if (get_pfn_refcount(PFN) == 1) {
                if (prot == PROT_READ) {
                    *(TABLE_POINTER_V + splits[3]) = (*(TABLE_POINTER_V + splits[3]) & ~(1 << 3)); 
                    // unset the bit at the 3rd position
                } else {
                    *(TABLE_POINTER_V + splits[3]) = (*(TABLE_POINTER_V + splits[3]) | (1 << 3)); 
                    // set the bit at the 4th position
                }
            }           

            // put_pfn(PFN);
            // os_pfn_free(USER_REG, PFN);

            // invalidate the TLB
            asm volatile("invlpg (%0)" ::"r" (temp) : "memory");
        }
        virtual_address = temp + _4KB;
    }
}

void make_pte(struct exec_context* parent, struct exec_context* child, u64 start_addr, u64 end_addr) {

    int splits[4];
    while (start_addr < end_addr) {
        u64 PFN = child->pgd;
        u64* TABLE_POINTER_V = (u64*)(osmap(PFN));

        u64 PFN_PARENT = parent->pgd;
        u64* TABLE_POINTER_V_PARENT = (u64*)(osmap(PFN_PARENT));

        u64 temp = start_addr;

        // splitting the address
        temp = (temp >> 12);
        for(int i = 3; i >= 0; i--) {
            splits[i] = ((1 << 9) - 1) & temp;
            temp = temp >> 9;
        }

        // making the PT
        for (int i = 0; i < 4; i++) {
            u64 entry = *(TABLE_POINTER_V + splits[i]);
            u64 entry_PARENT = *(TABLE_POINTER_V_PARENT + splits[i]);
            // in case of the PTE_T (we have to copy the PFN of the PageFrame 
            // and set write bit off and also increase the reference count of the PFN 
            // of the page frame to increase by one)
            if ((entry_PARENT & 0x1) != 0) {
                if (i == 3) {
                    // in case of the PTE_T (we have to copy the PFN of the PageFrame 
                    // and set write bit off and also increase the reference count of the PFN
                    // of the page frame to increase by one)
                    u64 PFN_MEM = (entry_PARENT >> 12);
                    *(TABLE_POINTER_V_PARENT + splits[i]) = (*(TABLE_POINTER_V_PARENT + splits[i]) & ~(1 << 3)); 
                    // read only to the page frame (updating it in PARENT)
                    // making the entry in CHILD
                    *(TABLE_POINTER_V + splits[i]) = *(TABLE_POINTER_V_PARENT + splits[i]);
                    get_pfn(PFN_MEM);   // ref count
                } else {
                    if ((entry & 0x1) == 0) {
                        // else allocation within OS space with read-write (generic) allocation
                        // for the page tables
                        PFN = os_pfn_alloc(OS_PT_REG);
                        entry = (PFN << 12) | 0x19;
                        *(TABLE_POINTER_V + splits[i]) = entry;
                    } else {
                        PFN = (entry >> 12);
                    }
                    PFN_PARENT = (entry_PARENT >> 12);
                    TABLE_POINTER_V = (u64*)(osmap(PFN));
                    TABLE_POINTER_V_PARENT = (u64*)(osmap(PFN_PARENT));
                }
            } else {
                // the parent page table is not allocated
                *(TABLE_POINTER_V + splits[i]) = 0x0;
                break;
            }
        }
        start_addr += _4KB;
    }
}
/**
 * mprotect System call Implementation.
 */
long vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{
    if (!current) {
        return -1;
    }
    if (addr < MMAP_AREA_START || addr > MMAP_AREA_END) {
        return -1;
    }
    if (prot != PROT_READ && prot != (PROT_READ | PROT_WRITE)) {
        return -1;
    }
    if (length <= 0) {
        return -1;
    }
    if (current->vm_area == NULL) {
        return -1;
    }

    struct vm_area* prev = current->vm_area;
    struct vm_area* iter = prev->vm_next;

    struct vm_area* vm = NULL;
    struct vm_area* new_vm = NULL;

    length = ALIGN(length);

    while(iter != NULL) {
        if (iter->vm_start >= addr && iter->vm_start <= addr + length) {
            // node that needs update found (as start_addr is between the updating address range)
            if (iter->vm_end <= addr + length) {
                // if the complete chunk is within (change the flag directly)

                // update PFN
                update_pte(current, iter, iter->vm_start, iter->vm_end, prot);             

                iter->access_flags = prot;

                // change in protection might potentially need merger
                // the return values to prevent infinite loop
                int merge_ret_val = merge_(prev, iter, iter->vm_next);

                if (merge_ret_val % 2) {
                    // either left-merging or both-merging
                    iter = prev;    // the prev->vm_next have been updated and hence 
                                    // subsequent iterations have no issues
                }
                // either no merging or right-merging (continue as normal)
            } else {
                // a part of the chunk needs change in the memory prots
                // creating a new node and adding it to the list
                vm = os_alloc_vm_struct(iter->access_flags, addr + length, iter->vm_end);
                if (vm == NULL) { // no allocation
                    return -1;
                }

                vm->vm_next = iter->vm_next;

                // update PFN
                update_pte(current, iter, iter->vm_start, addr + length, prot);

                iter->vm_end = addr + length;
                iter->access_flags = prot;
                iter->vm_next = vm;

                stats->num_vm_area += 1;

                merge_(prev, iter, vm); // potential merging for prev and iter
                // vm would not be merged ideally

                return 0;   // processing done
            }
        } else if (addr >= iter->vm_start && addr <= iter->vm_end) {
            // another condition (end_addr of the chunk between the address range)
            if (addr + length <= iter->vm_end) {
                
                // update PFN
                update_pte(current, iter, addr, addr + length, prot);

                // split into potentially three
                vm = os_alloc_vm_struct(prot, addr, addr + length);
                if (vm == NULL) { // no allocation
                    return -1;
                }

                if (addr + length == iter->vm_end) {
                    // split into two and not three
                    iter->vm_end = addr;
                    vm->vm_next = iter->vm_next;
                    iter->vm_next = vm;

                    stats->num_vm_area += 1;

                    merge_(iter, vm, vm->vm_next);  // ideally iter would not be merged here
                } else {
                    new_vm = os_alloc_vm_struct(iter->access_flags, addr + length, iter->vm_end);
                    if (new_vm == NULL) { // no allocation
                        return -1;
                    }
                    
                    new_vm->vm_next = iter->vm_next;
                    vm->vm_next = new_vm;
                    iter->vm_next = vm;
                    iter->vm_end = addr;

                    stats->num_vm_area += 2; // vm and new_vm
                    // no merging required if access_flag differs from prot(given), however in the other case
                    merge_(iter, vm, new_vm);
                }
            } else {
                // the mprotect (to be updated) region begins somewhere 
                // in the mid of the chunk and continues beyond it
                vm = os_alloc_vm_struct(prot, addr, iter->vm_end);
                if (vm == NULL) { // no allocation
                    return -1;
                }

                vm->vm_next = iter->vm_next;

                // update PFN
                update_pte(current, iter, addr, iter->vm_end, prot);

                iter->vm_next = vm;
                iter->vm_end = addr;

                stats->num_vm_area += 1;

                merge_(iter, vm, vm->vm_next);
            }
        }
        prev = iter;
        iter = iter->vm_next;
    }

    return 0;
}

/**
 * mmap system call implementation.
 */
long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{
    if (!current) {
        return -1;
    }
    if (length <= 0) {
        return -1;
    }
    if (addr != 0 && (addr <= MMAP_AREA_START || addr + length >= MMAP_AREA_END)) {
        return -1;          // invalid address range
    }
    if (flags != MAP_FIXED && flags != 0) {
        return -1;          // invalid flags
    }
    if (prot != PROT_READ && prot != (PROT_READ|PROT_WRITE)) {
        return -1;
    }
    if (length > 512*_4KB || length < 0) { // 2MB
        return -1;
    }

    length = ALIGN(length);     // aligning the length of memory chunk to a multiple of 4KB
    long return_address = -1;

    if (current->vm_area == NULL) {
        // first allocation
        struct vm_area* vm_head = os_alloc_vm_struct(0x0, MMAP_AREA_START, MMAP_AREA_START + _4KB);
        if (vm_head == NULL) { // failed allocation
            return -1;
        }
        current->vm_area = vm_head;
        stats->num_vm_area += 1;
    }

    if (flags == 0) { // Not MAP_FIXED
        if (addr == 0) { // first available chunk    
            return find_first_and_mmap(current, prot, length);  
        } else {
            // checking if address if already occupied
            int found = 0;
            struct vm_area* prev = current->vm_area;
            struct vm_area* iter = prev->vm_next;

            while(iter != NULL) { // finding if the hint-address is already in use
                if ((iter->vm_start <= addr && iter->vm_end > addr) ||
                    (iter->vm_start >= addr && iter->vm_start < addr + length)) {
                        found = 1;
                        break;
                } else {
                    prev = iter;
                    iter = iter->vm_next;
                }
            }

            if (found) { // addr requested is already occupied start from start
                return find_first_and_mmap(current, prot, length);
            } else { // memory available at the hint address
                struct vm_area* vm = os_alloc_vm_struct(prot, addr, addr + length);

                prev = current->vm_area;
                iter = prev->vm_next;

                while (iter != NULL) { // looking for point of addition in the sorted list
                    if (prev->vm_start <= vm->vm_start && iter->vm_start >= vm->vm_start) {
                        break;
                    } else {
                        prev = iter;
                        iter = iter->vm_next;
                    }
                }      
                return_address = vm->vm_start;

                // adding the node
                prev->vm_next = vm;
                vm->vm_next = iter;
                stats->num_vm_area += 1;

                merge_(prev, vm, iter);
                return return_address;                    
            }
        }
    } else { // flag = MAP_FIXED
        if (addr == 0) { // address needs to be passed
            return -1;
        }
        struct vm_area* prev = current->vm_area;
        struct vm_area* iter = prev->vm_next;

        while(iter != NULL) { // looking for point of addition in the sorted list
            if (addr >= prev->vm_start && addr <= iter->vm_start) {
                break;
            } else {
                prev = iter;
                iter = iter->vm_next;
            }
        }

        struct vm_area* vm = os_alloc_vm_struct(prot, addr, addr + length);

        if (iter == NULL) { // only the first dummy node in current->vm_area
            if (vm->vm_start < prev->vm_end) { // addresses within the dummy region not allowable
                return -1;
            } else {
                return_address = vm->vm_start;
                prev->vm_next = vm;
                vm->vm_next = iter;
                stats->num_vm_area += 1;
                merge_(prev, vm, iter);

                return return_address;
            }
        } else {
            if (vm->vm_start < prev->vm_end || vm->vm_end > iter->vm_start) {
                // not possible to be added in the hint address
                return -1;
            } else {
                return_address = vm->vm_start;
                // adding the node
                prev->vm_next = vm;
                vm->vm_next = iter;
                stats->num_vm_area += 1;
                merge_(prev, vm, iter);

                return return_address;
            }
        }
    }
    
    return -EINVAL;
}

/**
 * munmap system call implemenations
 */

long vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
    // All pages containing a part of the indicated range
    // are unmapped, and subsequent references to these pages will
    // generate SIGSEGV.  It is not an error if the indicated range does
    // not contain any mapped pages. [Source: man pages]
    
    if (!current) {
        return -1;
    }
    if (addr < MMAP_AREA_START || addr > MMAP_AREA_END) {
        return -1;
    }
    if (length < 0) {
        return -1;
    }
    if (addr + length > MMAP_AREA_END) {
        return -1;
    }
    if (current->vm_area == NULL) {
        return -1;
    }

    struct vm_area* prev = current->vm_area;
    struct vm_area* iter = prev->vm_next;

    struct vm_area* vm = NULL;
    struct vm_area* new_vm = NULL;

    length = ALIGN(length);

    while (iter != NULL) {
        if (iter->vm_start >= addr && iter->vm_start <= addr + length) {
            // if the chunk (w.r.t start_address) comes in overlap with the address range
            vm = iter;
            if (vm->vm_end <= addr + length) { // if the complete chunk can be removed from the list
                // removing the pointers and freeing the node in consideration

                // updating the page table entries, and freeing page frames if they exist
                free_pte(current, vm, vm->vm_start, vm->vm_end);

                prev->vm_next = vm->vm_next;
                iter = vm->vm_next;
                vm->vm_next = NULL;

                os_free(vm, sizeof(struct vm_area));

                stats->num_vm_area -= 1;

                iter = prev;    // otherwise the subsequent nodes might not get deleted
            } else {
                // only a part of the memory chunk needs to unmapped

                // updating the page table entries, and freeing page frames if they exist
                free_pte(current, vm, vm->vm_start, addr + length);

                // instead of creating a new node and deleting the existing one,
                // smarter method would be to resize the chunk itself
                iter->vm_start = addr + length;
            }
        } else if (addr >= iter->vm_start && addr <= iter->vm_end) {
            if (addr + length >= iter->vm_start && addr + length <= iter->vm_end) {
                
                // updating the page table entries, and freeing page frames if they exist
                free_pte(current, iter, addr, addr + length);

                new_vm = os_alloc_vm_struct(iter->access_flags, addr + length, iter->vm_end);
                new_vm->vm_next = iter->vm_next;
                iter->vm_end = addr;
                iter->vm_next = new_vm;

                stats->num_vm_area += 1;
            } else {
                // updating the page table entries, and freeing page frames if they exist
                free_pte(current, iter, addr, iter->vm_end);

                iter->vm_end = addr;
            }
        }
        prev = iter;
        iter = iter->vm_next;
    }
    return 0;
}



/**
 * Function will invoked whenever there is page fault for an address in the vm area region
 * created using mmap
 */

long vm_area_pagefault(struct exec_context *current, u64 addr, int error_code) {
    if (!current) {
        return -1;
    }

    struct vm_area* vm = current->vm_area;
    while(vm != NULL) {
        if (addr >= vm->vm_start && addr < vm->vm_end) {
            break;
        }
        vm = vm->vm_next;
    }

    if (!vm || (vm->access_flags == PROT_READ && error_code == 0x6)) {
        // either invalid addr (no such virtual address) OR
        // write-try on a read-only memory location
        return -1;
    }

    if (error_code == 0x7) {
        if (vm->access_flags == PROT_READ) {
            // invalid access
            return -1;
        } else {
            return handle_cow_fault(current, addr, vm->access_flags);
        }
    }

    // splits store the 9 bit offset in order: pgd_offset->[0], pud_offset->[1], pmd_offset->[2], pte_offset->[3]
    addr = (addr >> 12); // removing the offset
    int splits[4];
    for(int i = 3; i >= 0; i--) {
        splits[i] = ((1 << 9) - 1) & addr;
        addr = addr >> 9;
    }

    u64 PFN = current->pgd;     // PFN is currently the physical address of the CR3 (PTBR) register
    u64* TABLE_POINTER_V = (u64*)(osmap(PFN));  // TABLE_POINTER_V is the corresponding VA

    for (int i = 0; i < 4; i++) {
        u64 entry = *(TABLE_POINTER_V + splits[i]); // pgd_t, pud_t, pmd_t, pte_t
        if ((entry & 0x1) == 0) {
            // need to allocate next level table / page frame in case it is missing
            PFN = os_pfn_alloc((i == 3) ? USER_REG : OS_PT_REG);
            // updating the entry
            entry = (PFN << 12) | 0x19; // 11001 == 0x19
                                        // present bit(0) = 1; read/write bit(3) = 1; user/supervisor bit(4) = 1; 
            if (i == 3 && vm->access_flags != (PROT_READ|PROT_WRITE)) {
                entry = (PFN << 12) | 0x11; // 10001 == 0x11
                                            // present bit(0) = 1; read/write bit(3) = 0; user/supervisor bit(4) = 1; 
            }
            *(TABLE_POINTER_V + splits[i]) = entry;
        } else {
            PFN = (entry >> 12);
        }
        if (i < 3) {
            TABLE_POINTER_V = (u64*)(osmap(PFN));
        }
    }

    // asm volatile("invlpg (%0)" ::"r" (addr) : "memory");
    return 1;
}

/**
 * cfork system call implemenations
 * The parent returns the pid of child process. The return path of
 * the child process is handled separately through the calls at the 
 * end of this function (e.g., setup_child_context etc.)
 */

long do_cfork(){
    u32 pid;
    struct exec_context *new_ctx = get_new_ctx();
    struct exec_context *ctx = get_current_ctx();
     /* Do not modify above lines
     * 
     * */   
     /*--------------------- Your code [start]---------------*/
    
    // copying the contents of the parent exec_context to the child exec_context

    new_ctx->ppid = ctx->pid;   // child ppid = parent pid
    new_ctx->type = ctx->type;
    new_ctx->state = ctx->state;
    new_ctx->used_mem = ctx->used_mem;
    new_ctx->os_rsp = ctx->os_rsp;
    memcpy(new_ctx->name, ctx->name, CNAME_MAX);
    new_ctx->regs = ctx->regs;
    new_ctx->pending_signal_bitmap = ctx->pending_signal_bitmap;

    for(int i = 0; i < MAX_SIGNALS; i++)
        new_ctx->sighandlers[i] = ctx->sighandlers[i];
    new_ctx->ticks_to_sleep = ctx->ticks_to_sleep;
    new_ctx->alarm_config_time = ctx->alarm_config_time;
    for(int i = 0; i < MAX_OPEN_FILES; i++){
        new_ctx->files[i] = ctx->files[i];
        if (new_ctx->files[i] != NULL) {
            new_ctx->files[i]->ref_count += 1;
        }
    }
    new_ctx->ctx_threads = ctx->ctx_threads;
    // copying the MMS segment
    for (int i = 0; i < MAX_MM_SEGS; i++) {
        new_ctx->mms[i] = ctx->mms[i];
    }
    // copying the vm_area list
    struct vm_area* temp = ctx->vm_area;
    while(temp != NULL){
        struct vm_area* new_vm_area = os_alloc(sizeof(struct vm_area));
        new_vm_area->vm_start = temp->vm_start;
        new_vm_area->vm_end = temp->vm_end;
        new_vm_area->access_flags = temp->access_flags;
        new_vm_area->vm_next = NULL;
        if(new_ctx->vm_area == NULL){
            new_ctx->vm_area = new_vm_area;
        }else{
            struct vm_area* temp2 = new_ctx->vm_area;
            while(temp2->vm_next != NULL){
                temp2 = temp2->vm_next;
            }
            temp2->vm_next = new_vm_area;
        }
        temp = temp->vm_next;
    }

    // first level PGD table for child to be allocated
    new_ctx->pgd = os_pfn_alloc(OS_PT_REG);
    // copy the PTE (4 levels)
    
    // first for the MMS segments
    for(int i = 0; i < MAX_MM_SEGS; i++) {
        if (i != MM_SEG_STACK) {
            make_pte(ctx, new_ctx, ctx->mms[i].start, ctx->mms[i].next_free);
        } else {
            make_pte(ctx, new_ctx, ctx->mms[i].start, ctx->mms[i].end);
        }
    }
    struct vm_area* vm_p = new_ctx->vm_area;
    while(vm_p != NULL) {
        make_pte(ctx, new_ctx, vm_p->vm_start, vm_p->vm_end);
        vm_p = vm_p->vm_next;
    }

    pid = new_ctx->pid;
     /*--------------------- Your code [end] ----------------*/
    
     /*
     * The remaining part must not be changed
     */
    copy_os_pts(ctx->pgd, new_ctx->pgd);
    do_file_fork(new_ctx);
    setup_child_context(new_ctx);
    return pid;
}



/* Cow fault handling, for the entire user address space
 * For address belonging to memory segments (i.e., stack, data) 
 * it is called when there is a CoW violation in these areas. 
 *
 * For vm areas, your fault handler 'vm_area_pagefault'
 * should invoke this function
 * */

long handle_cow_fault(struct exec_context *current, u64 vaddr, int access_flags)
{
    if (!current) {
        return -1;
    }
    if (access_flags == PROT_READ) {
        return -1;
    }
    u64 PFN = current->pgd;
    u64* TABLE_POINTER_V = (u64*)(osmap(PFN));

    int splits[4];
    u64 temp = vaddr;
    temp = (temp >> 12);
    for(int i = 3; i >= 0; i--) {
        splits[i] = ((1 << 9) - 1) & temp;
        temp = temp >> 9;
    }

    for (int i = 0; i < 3; i++) {
        u64 entry = *(TABLE_POINTER_V + splits[i]);
        if ((entry & 0x1) == 0) {
            // should not happen
            return -1;
        }
        PFN = (entry >> 12);
        TABLE_POINTER_V = (u64*)(osmap(PFN));
    }
    // TABLE_POINTER_V is currently base address of the PTE/last level table
    PFN = (*(TABLE_POINTER_V + splits[3])) >> 12;
    // PFN is the page frame number

    if (get_pfn_refcount(PFN) == 1) {
        // single reference
        // references to this frame
        *(TABLE_POINTER_V + splits[3]) = *(TABLE_POINTER_V + splits[3]) | (1 << 3); // set the write bit   
        return 1;
    } else {
        // create a new page frame and update the entry for this page table
        // reduce the reference counter for the PFN
        u64 PFN_NEW = os_pfn_alloc(USER_REG);
        put_pfn(PFN);
        *(TABLE_POINTER_V + splits[3]) = (PFN_NEW << 12) | 0x19;

        // now we need to copy the page frame itself
        char* from_addr = (char*)osmap(PFN);
        char* to_addr = (char*)osmap(PFN_NEW);

        memcpy(to_addr, from_addr, _4KB);
    }

    return 1;
}