# Scheduler Class

## High Level Process flow

```mermaid
flowchart TB
  A["User Input"] -->|Gives New *P*| B1["Job Queue"]
  B1 -->|Ready to emplace *P*| B2{L.T.S.}
  B2 -->|*P* **allocates** mem| F["Memory Manager"]
  F -->|Memory full, return *P*| B2
  B2 -->|**Admitted** P ∋ ¬mem| C["Ready Queue"]
  F -->|**Admitted** P ∋ mem| C
  C -->|**Victim** P swapped out by M.T.S algo| F
  C -->|S.T.S Algo decides next P to run| C1{S.T.S}
  C1 --> D["Running in CPU"]
  D -->|S.T.S Algo RR rotates running P| C
  D -->|P is done| E["Finished Queue"]
  D -->|**Page fault**| F
  F -->|Page marked **swapped out**| G["Swapped Queue"]
  G -->|P checks if victim P exist| G1{M.T.S}
  G1 -->|P swapped in **Victim** P| C
  G1 -->|No **Victim P** Exist| G
  

  subgraph Scheduler
    B1
    B2
    C
    C1
    D
    E
    G
    G1
  end
```

## Low Level Process flow

```mermaid
flowchart TB
    %% === Long-Term Scheduler ===
    A["User / CLI (screen -s, scheduler-start)"] --> B["Long-Term Scheduler (job_queue_)"]
    B -->|Check memory availability| C["MemoryManager::allocate_process_memory()"]
    C -->|Success| D["Ready Queue (ready_queue_)"]
    C -->|Failure| E["Job waits until memory is available"]
    E -->|Requeued if Memory Size is possible| B
    E -->|Denied if Memory Size is invalid| A
    %% === Short-Term Scheduler ===
    D -->|CPU available| F["Scheduler::dispatch_to_cpu(cpu_id)"]
    F --> G["CPUWorker Thread (per core)"]
    G -->|Execute instructions| H["Process::execute_one_instruction()"]

    %% === Medium-Term Scheduler ===
    H -->|Page fault occurs| I["Scheduler::handle_page_fault(process, addr)"]
    I --> J["MemoryManager::handle_page_fault(process, addr)"]
    J -->|Page swapped out| K["Swapped Queue (swapped_queue_)"]
    J -->|Page loaded successfully| D
    K -->|Process gets Valid Page| D

    %% === Process State Changes ===
    H -->|Time slice expired| L["Scheduler::release_cpu(cpu_id, process, false, true)"]
    H -->|Process finished| M["Scheduler::release_cpu(cpu_id, process, true, false)"]
    L --> D
    M --> N["Finished List (finished_)"]
    N --> O["MemoryManager::free_process_memory(process)"]

    %% === CPU Thread Loop ===
    G -->|Needs new process| F

    %% === Summary Relationships ===
    subgraph Scheduler_Thread["Scheduler Thread (tick_loop)"]
        subgraph Long Term Scheduler
          B
          E
        end
        D
        I
        K
        L
        M
        N
        F
        subgraph CPU_Cores["CPUWorker Threads"]
          G
          H
      end
    end

    subgraph MemorySubsystem["Memory Manager & Paging"]
        C
        J
        O
    end

```

## Ready Queue

Ready Queue implementation needs to satisfy both short term queue and medium term queue.

- It needs to be a Channel like in go-lang, a FIFO data structure.

- It needs to select among the element the one with "least priority"; it has the ability to choose "a victim". This suggests a data structure that has a property of queue but be able to use some algorithm to select a victim. The closest to this is a multi-set `https://www.geeksforgeeks.org/cpp/multiset-in-cpp-stl/`.

- It needs to change the algorithm midway