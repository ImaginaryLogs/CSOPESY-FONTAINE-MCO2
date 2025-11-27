# Memory Management & Demand Paging Architecture

## Overview
This document outlines the architecture for the Memory Management System (MMS) and Demand Paging implementation for the CSOPESY OS project. The system simulates a paged memory environment with backing store support, demand paging, and FIFO page replacement.

## Core Components

### 1. MemoryManager (Singleton)
The `MemoryManager` is a singleton class responsible for managing physical memory (RAM) and backing store operations.

**Data Structures:**
- `std::vector<uint8_t> ram_`: Physical memory simulation. Size = `max-overall-mem` bytes.
- `std::vector<bool> frame_map_`: Bitmap tracking free/allocated frames.
- `std::vector<bool> dirty_map_`: Tracks which frames have been modified.
- `std::deque<size_t> active_frames_`: FIFO queue for page replacement.
- `std::unordered_map<size_t, FrameOwner> frame_owners_`: Maps frame index to {pid, page_num}.

**Key Methods:**
- `request_page(pid, page_num, load_from_disk)`: Allocates a frame for the given page. Evicts a victim if memory is full.
- `read_physical(frame_idx, offset)`: Reads a `uint16_t` value from physical memory at the given frame and offset.
- `write_physical(frame_idx, offset, value)`: Writes a `uint16_t` value to physical memory. Marks frame as dirty.
- `save_frame_to_disk(pid, page_num, frame_idx)`: Persists frame contents to backing store.
- `load_frame_from_disk(pid, page_num, frame_idx)`: Restores frame contents from backing store.
- `get_ram_state()`: Returns frame allocation state for visualization (`process-smi`).
- `get_paged_in_count()`, `get_paged_out_count()`: Statistics for `vmstat`.

**Thread Safety:**
All methods are protected by a mutex (`mtx_`) to ensure safe concurrent access from multiple CPU workers.

### 2. Process Memory Management
Each `Process` instance maintains its own virtual memory abstraction.

**Data Structures:**
- `std::unordered_map<std::string, uint32_t> symbol_table_`: Maps variable names to virtual addresses.
- `std::vector<PageEntry> page_table_`: Page table with entries containing:
  - `frame_idx`: Physical frame number (when valid).
  - `valid`: Whether page is currently in physical memory.
  - `dirty`: Whether page has been modified (tracked by MemoryManager).
  - `on_disk`: Whether page is persisted in backing store.
- `size_t m_page_size`: Page size (same as `mem-per-frame`).
- `size_t current_brk_`: Current heap pointer for variable allocation.

**Key Methods:**
- `translate(v_addr)`: Converts virtual address to `{frame_idx, offset}`. Returns `std::nullopt` if page fault.
- `read_token_value(token)`: Reads variable from symbol table. Triggers page fault if symbol table page not in memory.
- `set_var_value(name, value)`: Writes variable to symbol table. Triggers page fault if needed. Allocates virtual address on first declaration.
- `update_page_table(page_num, frame_idx)`: Updates page table entry after page is loaded.
- `invalidate_page(page_num)`: Marks page as invalid when evicted.
- `initialize_memory(mem_size, page_size)`: Initializes memory structures during process admission.

### 3. Instruction Set
The process instruction set includes both computation and memory access instructions.

**Supported Instructions:**
- `PRINT <msg>`: Outputs message or variable value to process log.
- `DECLARE <var> [value]`: Declares a variable and optionally initializes it. Allocates 2 bytes in symbol table.
- `ADD <dst> <src1> <src2>`: Adds two values and stores result.
- `SUBTRACT <dst> <src1> <src2>`: Subtracts two values and stores result.
- `SLEEP <ticks>`: Relinquishes CPU for specified ticks (process enters WAITING state).
- `READ <var> <address>`: Reads `uint16_t` from arbitrary virtual address to variable. Supports hex (0x...) and decimal formats.
- `WRITE <address> <var>`: Writes variable value to arbitrary virtual address.

**Memory Access Instructions (READ/WRITE):**
- Both instructions use `Process::translate()` to convert virtual addresses to physical.
- Trigger page faults if the target page is not in memory.
- Support addresses up to 65536 (2^16) per spec limits.
- **Memory Violation Detection**: Addresses >= 65536 terminate the process with error message.

### 4. Backing Store
**Implementation:** File-based persistence in `backing_store/` directory.

**File Format:**
- One file per process: `backing_store/process_<pid>.swap`
- Each page occupies `mem-per-frame` bytes at offset `page_num * mem-per-frame`.
- Binary format: direct write/read of frame contents.

**Operations:**
- Created on-demand when first page is evicted.
- Dirty pages are written before eviction.
- Clean pages can be evicted without writing (will be re-loaded from their original source or zeroed if new).

### 5. Scheduler Integration
The `Scheduler` coordinates memory management with process execution.

**Long-Term Admission (`long_term_admission`):**
1. Dequeues processes from `job_queue_`.
2. Calculates memory requirement: random value between `min-mem-per-proc` and `max-mem-per-proc`.
3. Aligns memory size to frame boundaries.
4. Calls `Process::initialize_memory()` to set up page table.
5. Moves process to `READY` state and enqueues in `ready_queue_`.

**Page Fault Handling (`handle_page_fault`):**
1. Called when a process enters `BLOCKED_PAGE_FAULT` state.
2. Requests the faulting page from `MemoryManager::request_page()`.
3. Handles eviction if returned (updates victim process's page table).
4. Updates faulting process's page table with new frame.
5. Transitions process to `READY` state for retry.

**CPU Worker Interaction:**
- CPU workers detect `BLOCKED_PAGE_FAULT` state and yield the CPU.
- Scheduler's main loop calls `handle_page_fault()` for blocked processes.
- Process retries the same instruction after page fault is resolved.

## Interaction Flow

### Instruction Execution with Memory Access
1. `CPUWorker` calls `Process::execute_tick()`.
2. Process parses instruction (e.g., `ADD varA varB varC` or `READ varX 0x500`).
3. Process resolves variables to virtual addresses via `symbol_table_`.
4. For memory access, process calls `translate(v_addr)`.
5. **Hit**: Process uses returned `{frame_idx, offset}` to call `MemoryManager::read_physical()` or `write_physical()`.
6. **Miss (Page Fault)**:
   - `translate()` returns `std::nullopt`.
   - Process sets `faulting_page_` and state to `BLOCKED_PAGE_FAULT`.
   - Returns control to CPU worker without advancing PC.
7. Scheduler resolves page fault (see above).
8. Process retries the instruction in the next tick.

### Page Replacement (FIFO)
1. When memory is full, `MemoryManager::request_page()` calls `get_free_frame_or_evict()`.
2. FIFO victim selection: Pop oldest frame from `active_frames_` deque.
3. If victim frame is dirty, write to backing store.
4. Notify victim process to invalidate page table entry.
5. Load new page (from disk if `load_from_disk=true`, else zero-fill).
6. Update faulting process's page table.
7. Add new frame to end of FIFO queue.

### Memory Access Violations
When `READ` or `WRITE` encounters an address >= 65536:
1. Process logs error message with timestamp and invalid address.
2. Process state set to `FINISHED`.
3. Error printed to console for user visibility.
4. Format: `"Process <name> shut down due to memory access violation error that occurred at <HH:MM:SS>. 0x<addr> invalid."`

## Configuration (`config.txt`)

**Memory Parameters:**
- `max-overall-mem`: Total physical memory in bytes. Range: [2^6, 2^16], power of 2.
- `mem-per-frame`: Frame/page size in bytes. Range: [2^6, 2^16], power of 2.
- `min-mem-per-proc`: Minimum memory per process in bytes.
- `max-mem-per-proc`: Maximum memory per process in bytes.

**Derived Values:**
- Total frames: `max-overall-mem / mem-per-frame`
- Pages per process: `mem_size / mem-per-frame` (rounded up)

## CLI Commands

### Process Creation
- `screen -s <name>`: Creates process with random instructions.
- `screen -c <name> <mem_size> "<instructions>"`: Creates process with custom instructions (semicolon-separated).

### Memory Visualization
- `process-smi`: Shows memory usage summary per process (similar to nvidia-smi).
- `vmstat`: Shows detailed statistics:
  - Total/used/free memory
  - Idle/active/total CPU ticks
  - Pages paged in/out

### Process Inspection
- `screen -r <name>`: Attaches to process screen to view logs and status.
  - Shows memory violation error if process terminated due to invalid access.

## Design Decisions

### Why These Choices?
- **FIFO Replacement**: Simplest to implement, meets "bare minimum" requirement. No tracking of access patterns needed.
- **File-Based Backing Store**: Persistent across runs, easy to debug (can inspect .swap files).
- **uint16 Variables**: Tests multi-byte memory operations and page boundary scenarios.
- **Demand Paging**: Pages allocated only on first access, minimizes upfront memory consumption.
- **Lazy Symbol Table**: Variables allocated incrementally on first `DECLARE`, not pre-allocated.

### Implementation Constraints
- **Symbol Table Size**: Currently grows unbounded. In practice, limited by first few pages of process memory.
- **No TLB**: Every address translation checks page table (performance tradeoff for simplicity).
- **No Page Sharing**: Each process has isolated page tables (no shared memory between processes).
- **Fixed Page Size**: Page size is global, not per-process configurable.

## Testing Strategy
Comprehensive unit tests in `tests/test_memory_manager.cpp` cover:
1. Initialization and frame allocation
2. FIFO eviction correctness
3. Backing store persistence
4. READ/WRITE operations at various offsets
5. Multi-process memory isolation
6. Dirty page handling during eviction
7. Statistics tracking (paged in/out counters)

All tests verify proper interaction between MemoryManager, Process, and backing store.
