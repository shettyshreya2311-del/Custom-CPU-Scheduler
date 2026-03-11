# Custom Hybrid CPU Scheduler - Design Document

## Overview

This project implements a custom CPU scheduler as a Linux kernel loadable module. The scheduler uses a hybrid algorithm combining multiple scheduling strategies based on task priority levels.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Web Dashboard                             │
│                    (dashboard_v4.py + HTML)                      │
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐   │
│  │ Add Tasks    │  │ Start/Reset  │  │ Real-time Display    │   │
│  │ (Name,Pri,   │  │ Buttons      │  │ - Task Table         │   │
│  │  Burst)      │  │              │  │ - Gantt Chart        │   │
│  └──────────────┘  └──────────────┘  │ - Metrics            │   │
│                                       └──────────────────────┘   │
└───────────────────────────┬─────────────────────────────────────┘
                            │ HTTP API
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                     Flask Server (Python)                        │
│                                                                  │
│  /api/add_task  → echo "create_task..." > /proc/custom_scheduler │
│  /api/start     → echo "start" > /proc/custom_scheduler          │
│  /api/stats     → cat /proc/custom_scheduler (JSON)              │
│  /api/reset     → echo "reset" > /proc/custom_scheduler          │
└───────────────────────────┬─────────────────────────────────────┘
                            │ /proc filesystem
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                  Linux Kernel Module                             │
│               (custom_scheduler_v4.ko)                           │
│                                                                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │
│  │ HIGH Queue  │  │ MEDIUM Queue│  │ LOW Queue   │              │
│  │ (EDF)       │  │ (SJF)       │  │ (FIFO)      │              │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘              │
│         │                │                │                      │
│         └────────────────┼────────────────┘                      │
│                          ▼                                       │
│                  ┌───────────────┐                               │
│                  │ pick_next_task│                               │
│                  │ (Scheduler)   │                               │
│                  └───────┬───────┘                               │
│                          ▼                                       │
│                  ┌───────────────┐                               │
│                  │ Kernel Thread │                               │
│                  │ (Executor)    │                               │
│                  └───────────────┘                               │
└─────────────────────────────────────────────────────────────────┘
```

## Scheduling Algorithm

### Priority Queues

| Priority | Queue | Algorithm | Description |
|----------|-------|-----------|-------------|
| 0 | HIGH | EDF (Earliest Deadline First) | Shortest burst time executes first |
| 1 | MEDIUM | SJF (Shortest Job First) | Shortest burst time executes first |
| 2 | LOW | FIFO (First Come First Serve) | Order of arrival |

### Task Selection Logic

```c
struct kernel_task *pick_next_task(void) {
    // 1. Check HIGH priority queue first (EDF - shortest burst)
    if (!list_empty(&scheduler.high_queue)) {
        return select_shortest_burst(&scheduler.high_queue);
    }
    
    // 2. Check MEDIUM priority queue (SJF - shortest burst)
    if (!list_empty(&scheduler.medium_queue)) {
        return select_shortest_burst(&scheduler.medium_queue);
    }
    
    // 3. Check LOW priority queue (FIFO - first in queue)
    if (!list_empty(&scheduler.low_queue)) {
        return list_first_entry(&scheduler.low_queue, ...);
    }
    
    return NULL;  // No tasks available
}
```

## Data Structures

### Task Structure
```c
struct kernel_task {
    char name[16];              // Task name (P1, P2, etc.)
    int id;                     // Unique task ID
    int priority;               // 0=HIGH, 1=MEDIUM, 2=LOW
    unsigned long burst_time;   // Burst time in milliseconds
    unsigned long remaining;    // Remaining execution time
    unsigned long start_time;   // When task started (ms from scheduler start)
    unsigned long end_time;     // When task completed
    int status;                 // 0=WAITING, 1=RUNNING, 2=COMPLETED
    struct list_head list;      // Linked list node
};
```

### Scheduler State
```c
struct scheduler_state {
    struct list_head high_queue;    // HIGH priority tasks
    struct list_head medium_queue;  // MEDIUM priority tasks
    struct list_head low_queue;     // LOW priority tasks
    struct list_head completed;     // Completed tasks
    
    spinlock_t lock;                // Synchronization
    bool running;                   // Scheduler active?
    unsigned long start_jiffies;    // When scheduler started
    int task_count;                 // Total tasks created
    int completed_count;            // Tasks completed
};
```

## Kernel Interface

### /proc/custom_scheduler

**Write Commands:**
- `create_task <name> <priority> <burst_ms>` - Create a new task
- `start` - Begin task execution
- `reset` - Clear all tasks and reset scheduler

**Read Output (JSON):**
```json
{
  "timer_started": true,
  "scheduler_running": false,
  "elapsed_ms": 15234,
  "current_task": null,
  "high_waiting": 0,
  "medium_waiting": 0,
  "low_waiting": 0,
  "completed": 3,
  "tasks": [
    {"id": "P1", "priority": 0, "burst": 3000, "status": 2, "start": 0, "end": 3012},
    {"id": "P2", "priority": 1, "burst": 2000, "status": 2, "start": 3012, "end": 5024}
  ]
}
```

## Execution Model

1. **Task Creation**: User adds tasks via dashboard → HTTP API → /proc write → kernel enqueue
2. **Scheduling**: User clicks "Start" → kernel scheduler thread wakes up
3. **Execution**: Kernel thread runs `pick_next_task()` → executes task → repeats until queues empty
4. **Monitoring**: Dashboard polls /proc for JSON state → updates UI in real-time

## Performance Metrics

- **Turnaround Time**: End time - Arrival time (arrival = 0 for all tasks)
- **Waiting Time**: Start time - Arrival time
- **Context Switches**: Number of task transitions - 1
- **Throughput**: Tasks completed per unit time

## Comparison with CFS

The dashboard includes a CFS (Completely Fair Scheduler) simulation for comparison:
- CFS uses round-robin with fair time slices
- Custom scheduler uses priority-based non-preemptive execution
- Typically shows lower turnaround time for custom scheduler due to no context switch overhead
