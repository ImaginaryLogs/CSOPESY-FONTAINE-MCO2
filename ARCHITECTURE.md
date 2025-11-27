# Memory Management & Demand Paging Architecture

## Overview
This document outlines the architecture for the Memory Management System (MMS) and Demand Paging extension for the CSOPESY OS project. The goal is to simulate a paged memory system with backing store support.

## Core Components

### 1. MemoryManager (Singleton/Global)
- **Responsibility**: Manages physical memory (RAM) and backing store (Disk).
- **Data Structures**:
    - `std::vector<uint8_t> ram_`: Represents physical memory. Size = `max-overall-mem`.
    - `std::vector<bool> frame_map_`: Tracks free/used frames.
    - `std::deque<size_t> active_frames_`: For FIFO page replacement.
- **Key Methods**:
    - `read(pid, v_addr)`: Reads byte/word from memory. Triggers page fault if page not present.
    - `write(pid, v_addr, value)`: Writes byte/word to memory. Triggers page fault if page not present.
    - `handle_page_fault(pid, v_addr)`: Loads page from backing store to RAM. Evicts victim if RAM full.
    - `allocate_page(pid)`: Allocates a new page for a process (used during initialization or stack growth).

### 2. Process Extensions
- **Symbol Table**: `std::unordered_map<std::string, uint32_t> symbol_table_`. Maps variable names to Virtual Addresses (VAddr).
- **Page Table**: `std::vector<PageEntry> page_table_`.
    - `PageEntry`: `{ frame_number, valid, dirty, on_disk }`.
- **Memory State**:
    - `memory_size`: Total memory required/allocated.
    - `current_brk`: Current top of heap/stack (for simple allocation).

### 3. Backing Store
- **Implementation**: File-based.
- **Location**: `backing_store/` directory.
- **Format**: One file per process (`<pid>.swap` or `<name>.swap`).
- **Operations**:
    - `store_page(pid, page_num, frame_data)`: Writes frame content to file.
    - `load_page(pid, page_num)`: Reads content from file.

## Interaction Flow

### 1. Instruction Execution (CPUWorker)
1.  `CPUWorker` calls `Process::execute_tick()`.
2.  `Process` parses instruction (e.g., `ADD varA varB`).
3.  `Process` resolves `varA` to `VAddr_A` using `symbol_table_`.
4.  `Process` requests `MemoryManager::read(pid, VAddr_A)`.
5.  **Hit**: `MemoryManager` returns value. Execution continues.
6.  **Miss (Page Fault)**:
    - `MemoryManager` throws/returns "Page Fault".
    - `Process` state changes to `BLOCKED_PAGE_FAULT`.
    - `CPUWorker` yields.

### 2. Page Fault Handling (Scheduler)
1.  Scheduler detects `BLOCKED_PAGE_FAULT` process.
2.  Scheduler calls `MemoryManager::handle_page_fault(pid, faulting_addr)`.
    - **Find Victim**: Select frame to evict (FIFO).
    - **Evict**: If dirty, write victim page to `backing_store/<victim_pid>.swap`. Update victim's Page Table.
    - **Load**: Read faulting page from `backing_store/<pid>.swap` (or initialize if new) into freed frame.
    - **Update**: Update Page Table of faulting process (valid=true, frame=new_frame).
3.  Process state changes to `READY`.

## Configuration (`config.txt`)
New parameters to be supported:
- `max-overall-mem`
- `mem-per-frame`
- `min-mem-per-proc`
- `max-mem-per-proc`

## Visualization
- `process-smi`: Iterates over `MemoryManager` state and Process Page Tables.
- `vmstat`: Aggregates stats from `MemoryManager`.

## Design Decisions (Bare Minimum)
- **Replacement Algorithm**: FIFO (First-In-First-Out). Easiest to implement.
- **Allocation**: Fixed partition or simple paging. We will use Paging as required.
- **Address Space**: Flat virtual address space per process, starting at 0.
- **Variable Storage**: Variables are 2 bytes (uint16). `DECLARE` allocates 2 bytes.
