### Operating Systems (CS330) Assignments
This repository contains all of my assignment solutions for the Operating Systems **(CS330A)** course at IIT Kanpur, Odd Semester - 2023, instructed by [Prof. Debadatta Mishra](https://www.cse.iitk.ac.in/users/deba/). 

| __Assignment__ | __Task__ | __Subtasks__ |
|-------------|------------|--------------|
| Assignment-1 | Process, File and Memory Abstractions | - Chaining Unary Opertors (Binaries) <br> - Custom Directory Space User <br> - Custom implementation of `malloc()` and `free()` functions: `memalloc()` and `memfree()` |
|Assignment-2|Custom System Call in gemOS|- A new once-read-write wrap-over data structure (`trace_buffer`) used subsequently. <br> - Implementing system call tracing functionality using a custom `strace` syscall, in the trace_buffer. <br> - Implementing function call tracing functionality using a custom `ftrace` syscall, in the trace_buffer.|
|Assignment-3|Memory Manipulations| - Memory mapping support in gemOS (`mmap`, `munmap`, `mprotect`) system calls manipulating the virtual address space. <br> - Page Table Manipluations: Implementing lazy-allocation of physical memory. Handling `page_faults`. <br> - Custom `fork` implementation that supports `copy-on-write` functionality between processes.|


### Created by

| __Name__ | __Email__ |
|-------------|------------|
| Danish Mehmood | [danishm21@iitk.ac.in](mailto:danishm21@iitk.ac.in) |


#### References
[1] Debadatta Mishra. 2019. gemOS: Bridging the Gap between Architecture and
Operating System in Computer System Education. In Workshop on Computer
Architecture Education (WCAE’19), June 22, 2019, Phoenix, AZ, USA. ACM,
New York, NY, USA, 8 pages.

[2] gem5 simulator: The gem5 simulator is a modular platform for computer-system architecture research, encompassing system-level architecture as well as processor microarchitecture. It is primarily used to evaluate new hardware designs, system software changes, and compile-time and run-time system optimizations.
The main website can be found at http://www.gem5.org.
