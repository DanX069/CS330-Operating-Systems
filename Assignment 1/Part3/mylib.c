#include "mylib.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdbool.h>

void *head = NULL;

unsigned long align_size(unsigned long size) {
    if (size % 8 != 0) {
        size = size + (8 - size % 8);
    }
    if (size < 24) size = 24;
    return size;
}

void *memalloc(unsigned long size) {
    size = align_size(size + sizeof(unsigned long));
    
    void *prev = NULL;
    void *next = NULL;

    if (!head) {
        unsigned long total_size = 0;
		if (size%(4 * 1024 * 1024) == 0 ) {
			total_size = size;
		} else {
			total_size = (size / (4 * 1024 * 1024) + 1) * (4 * 1024 * 1024);
		} 
        head = mmap(NULL, total_size, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (head == MAP_FAILED) {
            return NULL;
        }
        *(unsigned long *)head = total_size;
        *(void **)(head + sizeof(unsigned long)) = NULL;
        *(void **)(head + 2 * sizeof(unsigned long)) = NULL;
    }

    void *current = head;
    while (current) {
        unsigned long available_size = *(unsigned long *)current;
        next = *(void **)(current + sizeof(unsigned long));
        
        if (available_size >= size) {
            unsigned long remaining = available_size - size;
            void *next_chunk = current + size;
			void * prev_of_curr = *((void**)(current + 2*sizeof(unsigned long)));
			if(prev_of_curr!=NULL)
			{
				*((void**)(prev_of_curr + sizeof(unsigned long))) = *((void**)(current + sizeof(unsigned long)));
			}

			void* next_of_curr = *((void**)(current + sizeof(unsigned long)));
			if(next_of_curr!=NULL)
			{
				*((void**)(next_of_curr + 2*sizeof(unsigned long))) = *((void**)(current + 2*sizeof(unsigned long)));
			}
			if(current == head)
			{
				head = next_of_curr;
			}				
            if (remaining >= 24) {
            	*(unsigned long *)current = size;
                *(unsigned long *)next_chunk = remaining;
                *(void **)(next_chunk + sizeof(unsigned long)) = head;
                *(void **)(next_chunk + 2 * sizeof(unsigned long)) = NULL;

				if(head != NULL)
				*(void **)(head + 2*sizeof(unsigned long)) = next_chunk;

				head = next_chunk;
            }
			
            return current + sizeof(unsigned long);
        }
		
		else{
			current = *(void**)(current + sizeof(unsigned long));
		}
	}
	


	if(current==NULL)
	{ 
		unsigned long total_size = 0;
		if ((size%(4 * 1024 * 1024)) == 0 ) {
			total_size = size;
		} else {
			total_size = (size / (4 * 1024 * 1024) + 1) * (4 * 1024 * 1024);
		} 
		void *new_block = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (new_block == MAP_FAILED) {
			return NULL;
		}
		*(unsigned long *)new_block = total_size;
		unsigned long available_size = total_size;
            unsigned long remaining = available_size - size;			
            if (remaining >= 24) {
				*(unsigned long*)(new_block) = size;
				void* next_chunk = new_block + size;
                *(unsigned long *)next_chunk = remaining;
                *(void **)(next_chunk + sizeof(unsigned long)) = head;
                *(void **)(next_chunk + 2 * sizeof(unsigned long)) = NULL;
				*(void **)(head + 2*sizeof(unsigned long)) = next_chunk;
				head = next_chunk;

			}
				return new_block + sizeof(unsigned long);


	}
    return NULL;
}

int memfree(void *ptr) {
    if (!ptr) return -1;
    ptr = ptr - sizeof(unsigned long);  

    unsigned long chunk_size = *(unsigned long *)ptr;
    void *chunk_end = ptr + chunk_size;

    void *prev_chunk = *((void**)(ptr + 2*sizeof(unsigned long)));
    void *next_chunk = *((void**)(ptr + sizeof(unsigned long)));

    unsigned long prev_size = prev_chunk ? *(unsigned long *)prev_chunk : 0;
    void *prev_end = prev_chunk ? prev_chunk + prev_size : NULL;
    unsigned long next_size = next_chunk ? *(unsigned long *)next_chunk : 0;
    void *next_start = next_chunk;

    bool prev_free = prev_chunk && prev_end == ptr;
    bool next_free = next_chunk && next_start == chunk_end;

    // Case 1: Both contiguous chunks are allocated
    if (!prev_free && !next_free) {
        *(void **)(ptr + sizeof(unsigned long)) = head;
        *(void **)(ptr + 2 * sizeof(unsigned long)) = NULL;
        if (head) {
            *(void **)(head + 2 * sizeof(unsigned long)) = ptr;
        }
        head = ptr;
    }
    
    // Case 2: Only the contiguous chunk on the right is free
    else if (!prev_free && next_free) {
        unsigned long new_size = chunk_size + next_size;
        *(unsigned long *)ptr = new_size;
        void *next_next_chunk = *(void **)(next_chunk + sizeof(unsigned long));

        *(void **)(ptr + sizeof(unsigned long)) = next_next_chunk;
        if (next_next_chunk) {
            *(void **)(next_next_chunk + 2 * sizeof(unsigned long)) = ptr;
        }
        
        if (head == next_chunk) {
            head = ptr;
        }
    }

    // Case 3: Only the contiguous chunk on the left is free
    else if (prev_free && !next_free) {
        unsigned long new_size = chunk_size + prev_size;
        *(unsigned long *)prev_chunk = new_size;

        *(void **)(prev_chunk + 2 * sizeof(unsigned long)) = NULL;
        *(void **)(prev_chunk + sizeof(unsigned long)) = head;

        if (head) {
            *(void **)(head + 2 * sizeof(unsigned long)) = prev_chunk;
        }
        head = prev_chunk;
    }

    // Case 4: Both contiguous chunks are free
    else if (prev_free && next_free) {
        unsigned long new_size = chunk_size + prev_size + next_size;
        *(unsigned long *)prev_chunk = new_size;

        void *next_next_chunk = *(void **)(next_chunk + sizeof(unsigned long));
        *(void **)(prev_chunk + sizeof(unsigned long)) = next_next_chunk;
        if (next_next_chunk) {
            *(void **)(next_next_chunk + 2 * sizeof(unsigned long)) = prev_chunk;
        }

        if (head == next_chunk) {
            head = prev_chunk;
        }
    }

    return 0;
}


