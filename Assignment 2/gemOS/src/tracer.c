#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
#include<tracer.h>


// access_bit convention
// 1 -> read
// 2 -> write
// 4 -> execute


///////////////////////////////////////////////////////////////////////////
//// 		Start of Trace buffer functionality 		      /////
///////////////////////////////////////////////////////////////////////////

int is_valid_mem_range(unsigned long buff, u32 count, int access_bit) {
    u64 end = buff + count - 1; // end address of the buffer
    // int in_mms = 0, in_vm = 0;

    struct exec_context *current = get_current_ctx(); // Assuming there's a function to get the current context
    struct mm_segment *mms = current->mms;

    // Check against MM_SEG_CODE segment
    if (buff >= mms[MM_SEG_CODE].start && end < mms[MM_SEG_CODE].next_free) {
        if (access_bit == 1 || access_bit == 4) return 1; // valid for read-only // execute as wel
        else return 0;
    }

    // Check against MM_SEG_RODATA segment
    else if (buff >= mms[MM_SEG_RODATA].start && end < mms[MM_SEG_RODATA].next_free) {
        if (access_bit == 1) return 1; // valid for read-only
        else return 0;
    }

    // Check against MM_SEG_DATA segment
    else if (buff >= mms[MM_SEG_DATA].start && end < mms[MM_SEG_DATA].next_free) {
        if (access_bit == 1 || access_bit == 2) return 1; // valid for read and/or write
        else return 0;
    }

    // Check against MM_SEG_STACK segment
    else if (buff >= mms[MM_SEG_STACK].start && end < mms[MM_SEG_STACK].end) {
        if (access_bit == 1 || access_bit == 2) return 1; // valid for read and/or write
        else return 0;
    }

    // Check against VM areas
    struct vm_area *vm = current->vm_area;
    while (vm) {
        if (buff >= vm->vm_start && end < vm->vm_end) {
            if ((access_bit == 1) && (vm->access_flags & O_READ)) return 1; // valid for read
            if ((access_bit == 2) && (vm->access_flags & O_WRITE)) return 1; // valid for write
            break;
        }
        vm = vm->vm_next;
    }

    return 0; // Invalid memory (not found)
}




long trace_buffer_close(struct file *filep)
{
    if (!filep || !filep->trace_buffer) {
        return -EINVAL;  // Invalid file or trace buffer
    }

    struct trace_buffer_info *tb = filep->trace_buffer;

    // Free the allocated memory for the trace buffer's internal buffer
    if (tb->buffer) {
        os_page_free(USER_REG, tb->buffer);
    }

    // Free the trace buffer info structure


    // os_free(tb, sizeof(struct trace_buffer_info));

    os_page_free(USER_REG, tb);

    // Free the file operations structure
    if (filep->fops) {
        // os_free(filep->fops, sizeof(struct fileops));
        os_page_free(USER_REG, filep->fops);
    }

    // Free the file descriptor itself
    // os_free(filep, sizeof(struct file));
    os_page_free(USER_REG, filep);

    filep = NULL;

    return 0;  // Successfully closed the trace buffer
}





int trace_buffer_read(struct file *filep, char *buff, u32 count) {
    struct trace_buffer_info *tb = filep->trace_buffer;

    // Check if the trace buffer is valid and if read operations are allowed
    if (!tb || (tb->mode != O_RDWR && tb->mode != O_READ)) {
        return -EINVAL;
    }

    int valid = is_valid_mem_range((unsigned long)buff, count, 2);
        if (valid != 1) { // We need write access to the user buffer
        return -EBADMEM;
    }

    // Calculate the number of bytes available to read from the trace buffer

    if(TRACE_BUFFER_MAX_SIZE - tb->space < count) count = TRACE_BUFFER_MAX_SIZE - tb->space;
    // u32 bytes_available = (tb->write_offset >= tb->read_offset) ? 
    //                       tb->write_offset - tb->read_offset : 
    //                       TRACE_BUFFER_MAX_SIZE - tb->read_offset + tb->write_offset;

    // // Adjust the count if there are fewer bytes available than requested
    // if (bytes_available < count) {
    //     count = bytes_available;
    // }

    // If no bytes are available, return 0
    if (count == 0) {
        return 0;
    }

    // Read data from the trace buffer to the user-space buffer
    for (u32 i = 0; i < count; i++) {
        buff[i] = tb->buffer[(tb->read_offset + i) % TRACE_BUFFER_MAX_SIZE];
    }

    tb->read_offset = (tb->read_offset + count) % TRACE_BUFFER_MAX_SIZE;

    tb->space = tb->space + count;
    return count;
}






int trace_buffer_write(struct file *filep, char *buff, u32 count) {
    struct trace_buffer_info *tb = filep->trace_buffer;
    if (!tb || (tb->mode != O_RDWR && tb->mode != O_WRITE)) {
        return -EINVAL;
    }

    // Check if the buffer is within a valid memory range
    int valid = is_valid_mem_range((unsigned long)buff, count, 1);
    // printk("valid bit: %d\n", valid);
        if (valid != 1) { // We need read access to the user buffer
            // printk("156. not a valid memory range\n");
        return -EBADMEM;
    }

    // u32 bytes_to_write = (TRACE_BUFFER_MAX_SIZE + tb->read_offset - tb->write_offset - 1) % TRACE_BUFFER_MAX_SIZE;
    // if (bytes_to_write < count) {
    //     count = bytes_to_write;
    // }
    if(tb->space < count) count = tb->space;

    u32 first_chunk = TRACE_BUFFER_MAX_SIZE - tb->write_offset;
    if (first_chunk >= count) {
        for (u32 i = 0; i < count; i++) {
            tb->buffer[tb->write_offset + i] = buff[i];
        }
        tb->write_offset += count;
    } else {
        for (u32 i = 0; i < first_chunk; i++) {
            tb->buffer[tb->write_offset + i] = buff[i];
        }
        for (u32 i = 0; i < count - first_chunk; i++) {
            tb->buffer[i] = buff[first_chunk + i];
        }
        tb->write_offset = count - first_chunk;
    }
    tb->space=tb->space - count;
    return count;
}






int sys_create_trace_buffer(struct exec_context *current, int mode) {
    // 1. Check the mode
    // printk("Inside create_trace_buffer\n");
    if (mode != O_READ && mode != O_WRITE && mode != O_RDWR) {
        return -EINVAL;
    }

    // 2. Find a free file descriptor
    int fd;
    for (fd = 0; fd < MAX_OPEN_FILES; fd++) {
        if (!current->files[fd]) {
            break;
        }
    }
    if (fd == MAX_OPEN_FILES) {
        return -EINVAL;  // No free file descriptor available
    }

    // 3. Allocate and initialize a file object (struct file)
    // struct file *new_file = os_alloc(USER_REG);
    struct file *new_file = os_page_alloc(USER_REG);
    if (!new_file) {
        return -ENOMEM;
    }
    new_file->type = TRACE_BUFFER;  // Assuming TRACE_BUFFER is the type for trace buffer files
    new_file->mode = mode;
    new_file->offp = 0;
    new_file->ref_count = 1;
    new_file->inode = NULL;

    // 4. Allocate and initialize a trace buffer object (struct trace_buffer_info)
    // struct trace_buffer_info *tb = os_alloc(sizeof(struct trace_buffer_info));
    struct trace_buffer_info *tb = os_page_alloc(USER_REG);
    if (!tb) {
        os_page_free(USER_REG, new_file);
        return -ENOMEM;
    }
    tb->buffer = (char *)os_page_alloc(USER_REG);
    if (!tb->buffer) {
        // os_free(tb, sizeof(struct trace_buffer_info));
        os_page_free(USER_REG, tb);
        os_page_free(USER_REG, new_file);
        return -ENOMEM;
    }
    tb->read_offset = 0;
    tb->write_offset = 0;
    tb->mode = mode;
    tb->space=4096;
    new_file->trace_buffer = tb;

    // 5. Allocate and initialize file pointers object (struct fileops)
    // struct fileops *ops = os_alloc(sizeof(struct fileops));
    struct fileops *ops = os_page_alloc(USER_REG);
    if (!ops) {
        // os_free(tb->buffer, TRACE_BUFFER_MAX_SIZE);
        os_page_free(USER_REG, tb->buffer);
        // os_free(tb, sizeof(struct trace_buffer_info));
        os_page_free(USER_REG, tb);
        os_page_free(USER_REG, new_file);
        return -ENOMEM;
    }
    ops->read = trace_buffer_read;
    ops->write = trace_buffer_write;
    ops->lseek = NULL;  // lseek is not supported for trace buffer
    ops->close = trace_buffer_close;
    new_file->fops = ops;

    // Assign the new file to the found free file descriptor
    current->files[fd] = new_file;

    // 6. Return the allocated file descriptor number
    // printk("Created trace buffer\n");
    return fd;
}










///////////////////////////////////////////////////////////////////////////
//// 		Start of strace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////


int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4) {

int valid_syscalls[] = {
        SYSCALL_EXIT, SYSCALL_GETPID, SYSCALL_EXPAND, SYSCALL_SHRINK, SYSCALL_ALARM, 
        SYSCALL_SLEEP, SYSCALL_SIGNAL, SYSCALL_CLONE, SYSCALL_FORK, SYSCALL_STATS, 
        SYSCALL_CONFIGURE, SYSCALL_PHYS_INFO, SYSCALL_DUMP_PTT, SYSCALL_CFORK, SYSCALL_MMAP, 
        SYSCALL_MUNMAP, SYSCALL_MPROTECT, SYSCALL_PMAP, SYSCALL_VFORK, SYSCALL_GET_USER_P, 
        SYSCALL_GET_COW_F, SYSCALL_OPEN, SYSCALL_READ, SYSCALL_WRITE, SYSCALL_DUP, 
        SYSCALL_DUP2, SYSCALL_CLOSE, SYSCALL_LSEEK, SYSCALL_FTRACE, SYSCALL_TRACE_BUFFER, 
        SYSCALL_START_STRACE, SYSCALL_END_STRACE, SYSCALL_READ_STRACE, SYSCALL_STRACE, 
        SYSCALL_READ_FTRACE, SYSCALL_GETPPID
    };

    int is_valid = 0;  // Flag to check if syscall_num is valid
    for (int i = 0; i < sizeof(valid_syscalls) / sizeof(int); i++) {
        if (valid_syscalls[i] == syscall_num) {
            is_valid = 1;
            break;
        }
    }

    if (!is_valid) {
        return 0;  // Invalid syscall number
    }
    // printk("274: perform_tracing called\n");

    struct exec_context *current = get_current_ctx();
    if(syscall_num == SYSCALL_END_STRACE) {
        return 0;
    }

    // Check if tracing is enabled for the current process
    if (!current->st_md_base || !current->st_md_base->is_traced) {
        return 0;
    }
// printk("282: error check 1\n");
    // If in FILTERED_TRACING mode, check if the syscall is in the list to be traced
    if (current->st_md_base->tracing_mode == FILTERED_TRACING) {
        struct strace_info *info = current->st_md_base->next;
        int found = 0;
        while (info) {
            if (info->syscall_num == syscall_num) {
                found = 1;
                break;
            }
            info = info->next;
        }
        if (!found) {
            return 0;  // The syscall is not in the list to be traced in FILTERED_TRACING mode
        }
    }
// printk("298: filtered tracing condition\n");
    struct file *trace_file = current->files[current->st_md_base->strace_fd];
    if (!trace_file || trace_file->type != TRACE_BUFFER) {
        return 0;  // Invalid trace buffer
    }
// printk("303: test\n");
    char trace_data[40];  // Maximum size for 5 values (syscall_num + 4 params) * 8 bytes each = 40 bytes
    int data_len = 8;     // At least syscall_num will be stored

    *((u64 *)trace_data) = syscall_num;

    switch (syscall_num) {
        case SYSCALL_EXIT:
        case SYSCALL_GETPID:
        case SYSCALL_GETPPID:
        case SYSCALL_FORK:
        case SYSCALL_CFORK:
        case SYSCALL_VFORK:
        case SYSCALL_PHYS_INFO:
        case SYSCALL_STATS:
        case SYSCALL_GET_USER_P:
        case SYSCALL_GET_COW_F:
        case SYSCALL_END_STRACE:
            // Syscalls with 0 additional parameters
            break;

        case SYSCALL_SLEEP:
        case SYSCALL_PMAP:
        case SYSCALL_DUP:
        case SYSCALL_CLOSE:
        case SYSCALL_TRACE_BUFFER:
            // Syscalls with 1 additional parameter
            *((u64 *)(trace_data + data_len)) = param1;
            data_len += 8;
            break;

        case SYSCALL_SIGNAL:
        case SYSCALL_CLONE:
        case SYSCALL_MUNMAP:
        case SYSCALL_OPEN:
        case SYSCALL_DUP2:
        case SYSCALL_START_STRACE:
        case SYSCALL_STRACE:
            // Syscalls with 2 additional parameters
            *((u64 *)(trace_data + data_len)) = param1;
            data_len += 8;
            *((u64 *)(trace_data + data_len)) = param2;
            data_len += 8;
            break;

        case SYSCALL_READ:
        case SYSCALL_WRITE:
        case SYSCALL_LSEEK:
        case SYSCALL_READ_STRACE:
        case SYSCALL_READ_FTRACE:
        case SYSCALL_MPROTECT:
            // Syscalls with 3 additional parameters
            *((u64 *)(trace_data + data_len)) = param1;
            data_len += 8;
            *((u64 *)(trace_data + data_len)) = param2;
            data_len += 8;
            *((u64 *)(trace_data + data_len)) = param3;
            data_len += 8;
            break;

        case SYSCALL_MMAP:
        case SYSCALL_FTRACE:
            // Syscalls with 4 additional parameters
            *((u64 *)(trace_data + data_len)) = param1;
            data_len += 8;
            *((u64 *)(trace_data + data_len)) = param2;
            data_len += 8;
            *((u64 *)(trace_data + data_len)) = param3;
            data_len += 8;
            *((u64 *)(trace_data + data_len)) = param4;
            data_len += 8;
            break;

        default:
            // Unknown syscall number, handle appropriately
            return 0;
    }
// printk("380: before trace_buffer_write\n");
    // Write the data to the trace buffer
    struct trace_buffer_info *tb = trace_file->trace_buffer;
    if (!tb || (tb->mode != O_RDWR && tb->mode != O_WRITE)) {
        return -EINVAL;
    }

    if(tb->space < data_len) data_len = tb->space;

    u32 first_chunk = TRACE_BUFFER_MAX_SIZE - tb->write_offset;
    if (first_chunk >= data_len) {
        for (u32 i = 0; i < data_len; i++) {
            tb->buffer[tb->write_offset + i] = trace_data[i];
        }
        tb->write_offset += data_len;
    } else {
        for (u32 i = 0; i < first_chunk; i++) {
            tb->buffer[tb->write_offset + i] = trace_data[i];
        }
        for (u32 i = 0; i < data_len - first_chunk; i++) {
            tb->buffer[i] = trace_data[first_chunk + i];
        }
        tb->write_offset = data_len - first_chunk;
    }
    tb->space = tb->space - data_len;
    // printk("trace buffer write offset = %d\n", tb->write_offset);

    return 0;
}
    





int sys_strace(struct exec_context *current, int syscall_num, int action) {

int valid_syscalls[] = {
        SYSCALL_EXIT, SYSCALL_GETPID, SYSCALL_EXPAND, SYSCALL_SHRINK, SYSCALL_ALARM, 
        SYSCALL_SLEEP, SYSCALL_SIGNAL, SYSCALL_CLONE, SYSCALL_FORK, SYSCALL_STATS, 
        SYSCALL_CONFIGURE, SYSCALL_PHYS_INFO, SYSCALL_DUMP_PTT, SYSCALL_CFORK, SYSCALL_MMAP, 
        SYSCALL_MUNMAP, SYSCALL_MPROTECT, SYSCALL_PMAP, SYSCALL_VFORK, SYSCALL_GET_USER_P, 
        SYSCALL_GET_COW_F, SYSCALL_OPEN, SYSCALL_READ, SYSCALL_WRITE, SYSCALL_DUP, 
        SYSCALL_DUP2, SYSCALL_CLOSE, SYSCALL_LSEEK, SYSCALL_FTRACE, SYSCALL_TRACE_BUFFER, 
        SYSCALL_START_STRACE, SYSCALL_END_STRACE, SYSCALL_READ_STRACE, SYSCALL_STRACE, 
        SYSCALL_READ_FTRACE, SYSCALL_GETPPID
    };

    int is_valid = 0;  // Flag to check if syscall_num is valid
    for (int i = 0; i < sizeof(valid_syscalls) / sizeof(int); i++) {
        if (valid_syscalls[i] == syscall_num) {
            is_valid = 1;
            break;
        }
    }

    if (!is_valid) {
        return -EINVAL;  // Invalid syscall number
    }
    
    // Check if the strace_head is initialized
    if (!current->st_md_base) {
        // return -EINVAL; // Return error if not initialized
        // current->st_md_base = os_alloc(sizeof(struct strace_head));
        current->st_md_base = os_page_alloc(USER_REG);
        current->st_md_base->count = 0;
        current->st_md_base->is_traced = 0;
        current->st_md_base->strace_fd = -1;
        current->st_md_base->tracing_mode = -1;
        current->st_md_base->next = current->st_md_base->last = NULL;
    }

    struct strace_head *head = current->st_md_base;

    // Handle ADD_STRACE action
    if (action == ADD_STRACE) {
        // Check if the syscall is already being traced
        struct strace_info *temp = head->next;
        while (temp) {
            if (temp->syscall_num == syscall_num) {
                return -EINVAL; // Already being traced, so return success
            }
            temp = temp->next;
        }

        // If not being traced, add it to the list
        // struct strace_info *new_node = (struct strace_info *) os_alloc(sizeof(struct strace_info));
        struct strace_info *new_node = (struct strace_info *) os_page_alloc(USER_REG);
        if (!new_node) {
            return -EINVAL; // Return error if memory allocation fails
        }
        new_node->syscall_num = syscall_num;
        new_node->next = NULL;

        if (!head->next) {
            head->next = new_node;
            head->last = new_node;
        } else {
            head->last->next = new_node;
            head->last = new_node;
        }

        head->count++; // Increase the count of syscalls being traced

    } else if (action == REMOVE_STRACE) { // Handle REMOVE_STRACE action
        struct strace_info *prev = NULL;
        struct strace_info *temp = head->next;

        while (temp) {
            if (temp->syscall_num == syscall_num) {
                break;
            }
            prev = temp;
            temp = temp->next;
        }
        if(temp == NULL) {
            return -EINVAL;
        }

        if (prev) {
            prev->next = temp->next;
        } else {
            head->next = temp->next;
        }

        if (head->last == temp) {
            head->last = prev;
        }
        // os_free(temp, sizeof(struct strace_info)); // Free the memory of the node
        os_page_free(USER_REG, temp);
        head->count--; // Decrease the count of syscalls being traced
        return 0; // Return success
    } else {
        return -EINVAL; // Invalid action
    }

    return 0; // Return success
}


int sys_read_strace(struct file *filep, char *buff, u64 count) {
    // Check if the file pointer is valid and if it corresponds to a trace buffer
    if (!filep || filep->type != TRACE_BUFFER) {
        return -EINVAL;
    }

    // Get the current execution context
    struct exec_context *current = get_current_ctx();

    // Get the trace buffer information from the current execution context
    struct trace_buffer_info *trace_buffer = filep->trace_buffer;

    // Check if the trace buffer is valid
    if (!trace_buffer) {
        return -EINVAL;
    }

    u64 user_buffer_pos = 0;  // Position in the user buffer
    int i = 0;
    u64 bytes_read = 0;      // Bytes read from the trace buffer

    while (i < count && trace_buffer->read_offset < trace_buffer->write_offset) {
        // Read the system call number from the trace buffer
        bytes_read = trace_buffer_read(filep, buff + user_buffer_pos, sizeof(u64));
        u64 syscall_num = *((u64 *)(buff + user_buffer_pos));
        user_buffer_pos += bytes_read;
        // printk("syscall_num = %d, user_buff_pos = %d. i = %d, count = %d\n", syscall_num, user_buffer_pos);

        // Determine the number of arguments based on the syscall number
        int num_args = 0;
        switch (syscall_num) {
            case SYSCALL_EXIT:
            case SYSCALL_GETPID:
            case SYSCALL_GETPPID:
            case SYSCALL_FORK:
            case SYSCALL_CFORK:
            case SYSCALL_VFORK:
            case SYSCALL_PHYS_INFO:
            case SYSCALL_STATS:
            case SYSCALL_GET_USER_P:
            case SYSCALL_GET_COW_F:
            case SYSCALL_END_STRACE:
                num_args = 0;
                break;

            case SYSCALL_SLEEP:
            case SYSCALL_PMAP:
            case SYSCALL_DUP:
            case SYSCALL_CLOSE:
            case SYSCALL_TRACE_BUFFER:
                num_args = 1;
                break;

            case SYSCALL_SIGNAL:
            case SYSCALL_CLONE:
            case SYSCALL_MUNMAP:
            case SYSCALL_OPEN:
            case SYSCALL_DUP2:
            case SYSCALL_START_STRACE:
            case SYSCALL_STRACE:
                num_args = 2;
                break;

            case SYSCALL_READ:
            case SYSCALL_WRITE:
            case SYSCALL_LSEEK:
            case SYSCALL_READ_STRACE:
            case SYSCALL_READ_FTRACE:
            case SYSCALL_MPROTECT:
                num_args = 3;
                break;

            case SYSCALL_MMAP:
            case SYSCALL_FTRACE:
                num_args = 4;
                break;

            default:
                // Unknown syscall number, handle appropriately
                return -EINVAL;
        }

        // Read the arguments of the system call from the trace buffer
        for (int j = 0; j < num_args && trace_buffer->read_offset < trace_buffer->write_offset; j++) {
            bytes_read = trace_buffer_read(filep, buff + user_buffer_pos, sizeof(u64));
            user_buffer_pos += bytes_read;
        }
        i++;
    }

    // Return the number of bytes filled in the user buffer
    return user_buffer_pos;
}





int sys_start_strace(struct exec_context *current, int fd, int tracing_mode) {
    // Check if the file descriptor is valid and corresponds to a trace buffer
    if (fd < 0 || fd >= MAX_OPEN_FILES || !current->files[fd] || current->files[fd]->type != TRACE_BUFFER) {
        return -EINVAL;
    }

    // Check if the tracing mode is valid
    if (tracing_mode != FULL_TRACING && tracing_mode != FILTERED_TRACING) {
        return -EINVAL;
    }

    // If tracing is already started for this syscall, update the fd and tracing_mode
    if (current->st_md_base) {
        current->st_md_base->is_traced = 1;
        current->st_md_base->strace_fd = fd;
        current->st_md_base->tracing_mode = tracing_mode;
        return 0;  // Successfully updated tracing info
    }

    // If tracing is not started, allocate memory for strace_head
    if (!current->st_md_base) {
        // current->st_md_base = os_alloc(sizeof(struct strace_head));
        current->st_md_base = os_page_alloc(USER_REG);
        if (!current->st_md_base) {
            return -EINVAL;  // Memory allocation failed
        }
        current->st_md_base->count = 0;
        current->st_md_base->next = NULL;
        current->st_md_base->last = NULL;
    }

    // Initialize the strace_head structure
    current->st_md_base->is_traced = 1;
    current->st_md_base->strace_fd = fd;
    current->st_md_base->tracing_mode = tracing_mode;

    return 0;  // Successfully started tracing
}



int sys_end_strace(struct exec_context *current) {
    // Check if the st_md_base exists (i.e., if tracing was started for this process)
    if (!current->st_md_base) {
        return -EINVAL;  // Tracing was not started for this process
    }

    // Check if tracing is currently active for this process
    if (!current->st_md_base->is_traced) {
        return -EINVAL;  // Tracing is not active for this process
    }

    // Cleanup the list of system calls to be traced
    struct strace_info *current_info = current->st_md_base->next;
    struct strace_info *next_info;
    while (current_info) {
        next_info = current_info->next;
        // os_free(current_info, sizeof(struct strace_info));
        os_page_free(USER_REG, current_info);
        current_info = next_info;
    }

    // Reset the st_md_base structure
    current->st_md_base->count = 0;
    current->st_md_base->is_traced = 0;
    current->st_md_base->strace_fd = -1;
    current->st_md_base->tracing_mode = 0;  // Reset to an invalid mode
    current->st_md_base->next = NULL;
    current->st_md_base->last = NULL;

    // Note: We are not releasing the trace buffer as per the description

    return 0;  // Successfully ended tracing
}




///////////////////////////////////////////////////////////////////////////
//// 		Start of ftrace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////



long do_ftrace(struct exec_context *ctx, unsigned long faddr, long action, long nargs, int fd_trace_buffer)
{	
    // printk("Entering do_ftrace\n");
// printk("Test1\n");
   
    // Check if the ftrace metadata base is initialized
    if (ctx->ft_md_base == NULL) { 
        // printk("Initializing ftrace_head\n");

         // Allocate memory for the ftrace metadata base
        struct ftrace_head *ft_md_base = os_alloc(sizeof(struct ftrace_head));

        // If memory allocation fails, return error
        if (!ft_md_base)
            return -EINVAL;

        // Initialize the ftrace metadata base fields
        ft_md_base->count = 0;
        ft_md_base->next = NULL;
        ft_md_base->last = NULL;
        ctx->ft_md_base = ft_md_base;
    }

    struct ftrace_head *ft_md_base = ctx->ft_md_base;

    // printk("Test2\n");
    
    // Handle different actions based on the provided action parameter
    if (action == ADD_FTRACE) {
        // Check if the maximum number of ftrace functions is reached
        // printk("Action: ADD_FTRACE\n");
        if (ft_md_base->count == FTRACE_MAX)
            return -EINVAL;

        struct ftrace_info *ft_info = ft_md_base->next;

        // printk("Test3\n");

        // Check if the function address is already being traced
        while (ft_info != NULL) {
            if (ft_info->faddr == faddr)
                return -EINVAL;
            ft_info = ft_info->next;
        }

         // printk("Test4\n");
        
        // Allocate memory for the new ftrace info
        ft_info = os_alloc(sizeof(struct ftrace_info));
        // Initialize the ftrace info fields
        ft_info->faddr = faddr;
        ft_info->num_args = nargs;
        ft_info->fd = fd_trace_buffer;
       
       // Backup the original code at the function address
        for (int i = 0; i < 4; i++) {
           ft_info->code_backup[i] =  *(((u8*)(ft_info->faddr)) + i);
        }
        
        // Initialize the trace buffer offsets
        struct trace_buffer_info *trace_buffer = ctx->files[ft_info->fd]->trace_buffer;
        trace_buffer->write_offset = 0;
        trace_buffer->read_offset = 0;


        // Add the new ftrace info to the list of traced functions
        ft_info->next = NULL;
        if (ft_md_base->count == 0) {
            ft_md_base->next = ft_info;
        } else {
            ft_md_base->last->next = ft_info;
        }
        ft_md_base->last = ft_info;
        ft_md_base->count++;
        return 0;



    } else if (action == REMOVE_FTRACE) {
        // printk("Action: REMOVE_FTRACE\n");
        // Find the function to be removed from tracing
        struct ftrace_info *ft_info = ft_md_base->next;
        struct ftrace_info *prev = NULL;
        while (ft_info != NULL) {
            if (ft_info->faddr == faddr)
                break;
            prev = ft_info;
            ft_info = ft_info->next;
        }
        // If the function is not found, return error
        if (ft_info == NULL)
            return -EINVAL;
        
        // Restore the original code if it was replaced by the invalid opcode
        if (*(u8*)ft_info->faddr == INV_OPCODE) {
            for (int i = 0; i < 4; i++) {
                *(((u8*)(ft_info->faddr)) + i) = ft_info->code_backup[i];
            }
        }


        // Remove the ftrace info from the list of traced functions
        if (ft_info == ft_md_base->next) {
            ft_md_base->next = ft_info->next;
            if (ft_info == ft_md_base->last)
                ft_md_base->last = NULL;
        } else if (ft_info == ft_md_base->last) {
            ft_md_base->last = prev;
            prev->next = NULL;
        } else {
            prev->next = ft_info->next;
        }

        // Free the memory allocated for the ftrace info
        os_free(ft_info, sizeof(struct ftrace_info));
        ft_info = NULL;
        ft_md_base->count--;
        return 0;



    } else if (action == ENABLE_FTRACE) {
        // printk("Action: ENABLE_FTRACE\n");
        // Find the function to be enabled for tracing
        struct ftrace_info *ft_info = ft_md_base->next;
        while (ft_info != NULL) {
            if (ft_info->faddr == faddr)
                break;
            ft_info = ft_info->next;
        }

        // If the function is not found, return error
        if (ft_info == NULL)
            return -EINVAL;
        
        // Replace the original code with the invalid opcode to enable tracing
        for (int i = 0; i < 4; i++) {
            *((u8*)(ft_info->faddr) + i) = INV_OPCODE;
        }
        return 0;



    } else if (action == DISABLE_FTRACE) {
        // printk("Action: DISABLE_FTRACE\n");

        // Find the function to be disabled from tracing
        struct ftrace_info *ft_info = ft_md_base->next;
        while (ft_info != NULL) {
            if (ft_info->faddr == faddr)
                break;
            ft_info = ft_info->next;
        }

        // If the function is not found, return error
        if (ft_info == NULL)
            return -EINVAL;
        
        // Restore the original code to disable tracing
        for (int i = 0; i < 4; i++) {
            *((u8*)(ft_info->faddr) + i) = ft_info->code_backup[i];
        }
        return 0;



    } else if (action == ENABLE_BACKTRACE) {
        // printk("Action: ENABLE_BACKTRACE\n");

        // Find the function to be enabled for backtrace
        struct ftrace_info *ft_info = ft_md_base->next;
        while (ft_info != NULL) {
            if (ft_info->faddr == faddr)
                break;
            ft_info = ft_info->next;
        }

        // If the function is not found, return error
        if (ft_info == NULL)
            return -EINVAL;
        
        // Replace the original code with the invalid opcode and enable backtrace
        if (*(u8*)ft_info->faddr != INV_OPCODE) {
            for (int i = 0; i < 4; i++) {
                ft_info->code_backup[i] = *((u8*)(ft_info->faddr) + i);
                *((u8*)(ft_info->faddr) + i) = INV_OPCODE;
            }
        }
        ft_info->capture_backtrace = 1;
        return 0;



    } else if (action == DISABLE_BACKTRACE) {
        // printk("Action: DISABLE_BACKTRACE\n");

        // Find the function to be disabled from backtrace
        struct ftrace_info *ft_info = ft_md_base->next;
        while (ft_info != NULL) {
            if (ft_info->faddr == faddr)
                break;
            ft_info = ft_info->next;
        }

        // If the function is not found, return error
        if (ft_info == NULL)
            return -EINVAL;
        
        // Restore the original code if it was replaced by the invalid opcode and disable backtrace
        if (*(u8*)ft_info->faddr == INV_OPCODE) {
            for (int i = 0; i < 4; i++) {
                *((u8*)(ft_info->faddr) + i) = ft_info->code_backup[i];
            }
        }
        ft_info->capture_backtrace = 0;
        return 0;
    }

    // printk("Exiting do_ftrace with no action matched\n");
    return 0;  // Return success if no action matched
}





//Fault handler

long handle_ftrace_fault(struct user_regs *regs) {

    // printk("Entering handle_ftrace_fault\n");
    // Get the current execution context
   struct exec_context *current = get_current_ctx();  // Assuming a function to get the current context
   
   // printk("In handler");

    // Check if the ftrace metadata base is initialized and if there's any function to trace
    if (!current->ft_md_base || !current->ft_md_base->count) {
        // printk("ftrace_head not initialized or no function to trace\n");
        return -EINVAL;
    }
    
    // Get the head of the ftrace list
    struct ftrace_head *head = current->ft_md_base;
    struct ftrace_info *temp = head->next;


    // Search for the function in the list of traced functions
    while (temp) {
        if (temp->faddr == regs->entry_rip) {
            break;
        }
        temp = temp->next;
    }
    
    // If the function is not found in the list, return error
    if (!temp) {
        // printk("Function not found in the list\n");
        return -EINVAL;
    }
    
    // Get the trace buffer associated with the function
    struct trace_buffer_info *trace_buffer = current->files[temp->fd]->trace_buffer;

    // Check if the trace buffer is full
    if (trace_buffer->space == 0) {
        // printk("Trace buffer is full\n");
        return -EINVAL;
    }

    
    // printk("Saving function address and arguments to trace buffer\n");

    // Save function address to the trace buffer
    *(u64*)(trace_buffer->buffer + trace_buffer->write_offset) = temp->faddr;
    trace_buffer->write_offset = (trace_buffer->write_offset + 8) % TRACE_BUFFER_MAX_SIZE;
    trace_buffer->space -= 8;


    // Save the function arguments to the trace buffer
    u64 arg_reg[6] = {regs->rdi, regs->rsi, regs->rdx, regs->rcx, regs->r8, regs->r9};
    for (int i = 0; i < temp->num_args; i++) {
        *(u64*)(trace_buffer->buffer + trace_buffer->write_offset) = arg_reg[i];
        trace_buffer->write_offset = (trace_buffer->write_offset + 8) % TRACE_BUFFER_MAX_SIZE;
        trace_buffer->space -= 8;
    }


    // If backtrace capture is enabled, save the backtrace to the trace buffer
    if (temp->capture_backtrace) {
        // printk("Capturing backtrace\n");
        if (trace_buffer->space == 0) {
            return -EINVAL;
        }

        *(u64*)(trace_buffer->buffer + trace_buffer->write_offset) = regs->entry_rip;
        trace_buffer->write_offset = (trace_buffer->write_offset + 8) % TRACE_BUFFER_MAX_SIZE;
        trace_buffer->space -= 8;


        u64 return_address = *(u64*)(regs->entry_rsp);
        u64 prev_rbp = regs->rbp;


        // Capture the backtrace until the end address is reached
        while (return_address != END_ADDR) {
            if (trace_buffer->space == 0) {
                // printk("Trace buffer space exhausted during backtrace\n");
                return -EINVAL;

                // printk("Address being stored:%x\n", return_address);
 

            }
            *(u64*)(trace_buffer->buffer + trace_buffer->write_offset) = return_address;
            trace_buffer->write_offset = (trace_buffer->write_offset + 8) % TRACE_BUFFER_MAX_SIZE;
            trace_buffer->space -= 8;

            return_address = *(u64*)(prev_rbp + 8);
            prev_rbp = *(u64*)prev_rbp;
        }
    }


    // printk("Adding delimiter to trace buffer\n");

    // Add a delimiter to the trace buffer
    if (trace_buffer->space == 0) {
        return -EINVAL;
    }

    *(u64*)(trace_buffer->buffer + trace_buffer->write_offset) = END_ADDR;
    trace_buffer->write_offset = (trace_buffer->write_offset + 8) % TRACE_BUFFER_MAX_SIZE;
    trace_buffer->space -= 8;


    // Adjust the stack pointer and base pointer
    *(u64*)(regs->entry_rsp - 8) = regs->rbp;
    regs->entry_rsp -= 8;
    regs->rbp = regs->entry_rsp;

    // Adjust the instruction pointer to skip the replaced instructions
    regs->entry_rip += 4;

    // printk("Exiting handle_ftrace_fault with success\n");
    return 0;  //return success
}





int sys_read_ftrace(struct file *filep, char *buff, u64 count)
{	
    // printk("Entering sys_read_ftrace\n");
    
    // Get the trace buffer associated with the file
    struct trace_buffer_info *trace_buffer = filep->trace_buffer;
    int bytes_written = 0;
    

    // Continue reading until the specified count is reached or the buffer is full
    while(count--){

        // Check if the trace buffer has reached its maximum size
        if (trace_buffer->space == TRACE_BUFFER_MAX_SIZE) {
            // printk("Trace buffer space reached max size\n");
            break;
        }


        // printk("Reading function address\n");

        // Read the function address from the trace buffer and store it in the user buffer
        *(u64*)(buff + bytes_written) = *(u64*)(trace_buffer->buffer + trace_buffer->read_offset);
        trace_buffer->read_offset = (trace_buffer->read_offset + 8) % TRACE_BUFFER_MAX_SIZE;
        trace_buffer->space += 8;
        bytes_written += 8;


        // printk("Reading arguments\n");

        // Read the function arguments from the trace buffer until the end address delimiter is reached
        while(*(u64*)(trace_buffer->buffer + trace_buffer->read_offset) != END_ADDR){
            *(u64*)(buff + bytes_written) = *(u64*)(trace_buffer->buffer + trace_buffer->read_offset);
            trace_buffer->read_offset = (trace_buffer->read_offset + 8) % TRACE_BUFFER_MAX_SIZE;
            trace_buffer->space += 8;
            bytes_written += 8;
        }


        // printk("Reading delimiter (not written to user buffer)\n");

        // Skip the delimiter in the trace buffer (it's not written to the user buffer)
        trace_buffer->read_offset = (trace_buffer->read_offset + 8) % TRACE_BUFFER_MAX_SIZE;
        trace_buffer->space += 8;
    }


    // printk("Exiting sys_read_ftrace with bytes_written: %d\n", bytes_written);
    return bytes_written; // Return the total number of bytes written to the user buffer
}







