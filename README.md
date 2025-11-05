# CSOPESY-FONTAINE — Process Scheduler Emulator <!-- omit from toc -->

![title](./assets/readme/furina-4.jpg)

![Year, Term, Course](https://img.shields.io/badge/AY2526--T1-CSOPESY-blue) ![C++](https://img.shields.io/badge/C++-%2300599C.svg?logo=c%2B%2B&logoColor=white)

A tick-driven, multi-threaded process scheduler emulator written in C++20. It simulates long-term, short-term, and (scaffolded) medium-term scheduling across N CPU worker threads, with a simple CLI for interaction and a reporter for human-readable snapshots.

Created by CSOPESY S13:

- Christian Joseph C. Bunyi
- Enzo Rafael S. Chan
- Roan Cedric V. Campo
- Mariella Jeanne A. Dellosa

Entry point: `src/main.cpp`

## Table of Contents <!-- omit from toc -->

- [1. Overview](#1-overview)
  - [1.1. Command recognition](#11-command-recognition)
  - [1.2. Console UI](#12-console-ui)
  - [1.3. Command interpreter](#13-command-interpreter)
  - [1.4. Process model](#14-process-model)
  - [1.5. Scheduler](#15-scheduler)
- [2. Architecture](#2-architecture)
- [3. Build](#3-build)
  - [3.1. Windows (MSYS2 UCRT64)](#31-windows-msys2-ucrt64)
  - [3.2. Linux/macOS/WSL](#32-linuxmacoswsl)
  - [3.3. Windows (MSVC)](#33-windows-msvc)
- [4. Run](#4-run)
- [5. Usage](#5-usage)
  - [5.1. Commands](#51-commands)
  - [5.2. Sample session](#52-sample-session)
- [6. Configuration](#6-configuration)
- [7. Key files](#7-key-files)
- [8. Notes](#8-notes)

## 1. Overview

### 1.1. Command recognition

The CLI reads one line at a time (`std::getline`) and dispatches by exact string match. Supported commands:

- `exit` | `quit` — terminate the CLI
- `initialize` — prints initialization status
- `scheduler-start` — acknowledges (generator already starts at boot)
- `report-util` | `screen -ls` — print a formatted scheduler snapshot

See `src/cli.cpp` (`CLI::run`).

### 1.2. Console UI

Minimal, shell-like UX with a banner, a prompt (`csopesy>`) and synchronous outputs. The `slow_print` helper animates the welcome banner.

### 1.3. Command interpreter

Interpreter logic currently lives in the CLI loop. Unknown commands print a helpful message and the REPL continues. A `handle_command` stub exists for future extensibility.

### 1.4. Process model

`Process` represents a program with a flat instruction list and runtime state. Supported instructions: `PRINT`, `DECLARE`, `ADD`, `SUBTRACT`, `SLEEP`, `FOR` (unrolled at construction). Execution is tick-based via `execute_tick`, returning a `ProcessReturnContext` that informs the scheduler of the next state (RUNNING/WAITING/READY/FINISHED).

### 1.5. Scheduler

One scheduler thread coordinates phases each tick; N worker threads execute processes in lockstep using a barrier. Queues include a job channel, a policy-aware ready queue, and a min-heap sleep queue. Snapshots aggregate job/ready/CPU/sleep/finished state.

## 2. Architecture

```mermaid
flowchart LR
  subgraph CLI
    CLI[CLI REPL]\n(src/cli.cpp)
    RPTR[Reporter]\n(src/reporter.cpp)
  end

  subgraph Core
    SCH[Scheduler\n(src/scheduler.cpp)]
    RQ[Ready Queue\nDynamicVictimChannel]
    JQ[Job Queue\nChannel<Process>]
    SQ[Sleep Queue\npriority_queue]
    FM[FinishedMap]
  end

  subgraph Workers
    W1[CPUWorker #0]
    Wn[CPUWorker #N-1]
  end

  subgraph Gen
    PG[ProcessGenerator]
  end

  CLI -- report-util --> RPTR -- snapshot() --> SCH
  PG -- submit_process(p) --> JQ
  JQ --> SCH
  SCH -- admit --> RQ
  RQ -- dispatch --> Workers
  Workers -- execute_tick() / yield --> SCH
  SCH -- sleep --> SQ
  SCH -- finish --> FM
```

- Barrier sync: `std::barrier` aligns the scheduler thread and all CPUWorker threads each tick.
- Ready queue policy: FCFS/RR/PRIORITY via comparators (see `src/scheduler_utils.cpp`).
- Sleep queue: `std::priority_queue<TimerEntry, …, std::greater<>>` by `wake_tick`.

## 3. Build

Requirements: C++20 compiler (barrier requires modern libstdc++/MSVC), standard threading support.

### 3.1. Windows (MSYS2 UCRT64)

#### MSYS2 UCRT64
> g++ -std=c++20 -O2 -pthread -Iinclude src/*.cpp -o csopesy

## 4. Run

```sh
./csopesy           # if you used the one-liner
./build/app         # if you used make
csopesy.exe         # on Windows
```

You’ll see a banner and the prompt `csopesy>`.

## 5. Usage

### 5.1. Commands

- `initialize` — prints “System initialized.”
- `scheduler-start` — acknowledges (generator already runs)
- `report-util` or `screen -ls` — prints a multi-section snapshot
- `exit` or `quit` — exit the program

### 5.2. Sample session

```
=============================================
         PROCESS SCHEDULER EMULATOR
=============================================
... (banner) ...
csopesy> report-util
CPU / Process Report
Timestamp: 2025-11-05 12:34:56

=== Scheduler Snapshot ===
Tick: 42
Paused: false
[Sleep Queue]
 (empty)
[Job Queue]
 (empty)
[Ready Queue]
  ...

[CPU States]:
  CPU 0: IDLE
  CPU 1: IDLE
... 
===========================
csopesy> exit
```

## 6. Configuration

Configuration is currently compile-time via `include/config.hpp`:

- `num_cpu` — number of CPUWorker threads
- `scheduler` — `RR`, `FCFS`, or `PRIORITY`
- `quantum_cycles` — RR quantum
- `batch_process_freq` — generation cadence (in scheduler ticks)
- `min_ins` / `max_ins` — generator top-level instruction bounds
- `max_unrolled_instructions` — budget post-FOR unrolling
- `scheduler_tick_delay` — ms per tick
- `snapshot_cooldown` — ticks between auto snapshot logs

Future work may add a CLI/config file loader (see `Config load_config` declaration).

## 7. Key files

- CLI: `include/cli.hpp`, `src/cli.cpp`
- Scheduler: `include/scheduler.hpp`, `src/scheduler.cpp`, `src/scheduler_utils.cpp`
- CPU Worker: `include/cpu_worker.hpp`, `src/cpu_worker.cpp`
- Process: `include/process.hpp`, `src/process.cpp`
- Instructions: `include/instruction.hpp`
- Queues/Utils: `include/util.hpp`
- Process Generator: `include/process_generator.hpp`, `src/process_generator.cpp`
- Reporter (snapshots): `include/reporter.hpp`, `src/reporter.cpp`
- Finished Map: `include/finished_map.hpp`, `src/finished_map.cpp`
- Docs: `docs/scheduler.md`, `docs/technical_report.md`

## 8. Notes

- Requires C++20 (std::barrier). If your libstdc++ is older, consider MSYS2 UCRT64 or recent GCC/Clang.
- Medium-term scheduling (paging/swapping) is scaffolded for extension.
- Snapshots are human-readable and intended for demos; parsing output is out of scope.
