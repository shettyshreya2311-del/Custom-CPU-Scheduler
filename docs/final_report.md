# Custom Hybrid CPU Scheduler - Final Report

## Project Overview

This project implements a custom CPU scheduler as a Linux kernel loadable module, demonstrating true kernel-level task scheduling with a hybrid algorithm combining EDF, SJF, and FIFO strategies.

## Objectives Achieved

### 1. True Kernel Integration
- **Kernel Module**: `custom_scheduler_v4.ko` runs entirely in kernel space
- **Real Execution**: Tasks are executed as kernel threads with actual CPU work
- **Proc Interface**: `/proc/custom_scheduler` provides bidirectional communication

### 2. Hybrid Scheduling Algorithm
| Priority | Strategy | Benefit |
|----------|----------|---------|
| HIGH | EDF (Earliest Deadline First) | Critical tasks complete fastest |
| MEDIUM | SJF (Shortest Job First) | Minimizes average waiting time |
| LOW | FIFO (First Come First Serve) | Fair ordering for batch jobs |

### 3. Real-time Visualization Dashboard
- Web-based interface using Flask + HTML/JavaScript
- Live Gantt chart showing task execution timeline
- Performance metrics: turnaround time, waiting time, context switches
- CFS comparison graph

## Technical Implementation

### Kernel Module Components
1. **Task Management**: Priority queues using Linux kernel linked lists
2. **Scheduler Thread**: Kernel thread that picks and executes tasks
3. **Proc Filesystem**: JSON export for dashboard communication
4. **CPU Work Simulation**: Real computational loops (not just sleep)

### Dashboard Features
- Task creation with custom name, priority, and burst time
- Start/Reset scheduler controls
- Real-time polling every 300ms
- Automatic timer stop on completion

## Performance Analysis

### Custom Scheduler Advantages

1. **Lower Turnaround Time**
   - Non-preemptive execution eliminates context switch overhead
   - Priority-based ordering ensures important tasks finish first

2. **Predictable Execution**
   - Tasks run to completion without interruption
   - Easier deadline guarantees for HIGH priority tasks

3. **Efficient for Batch Workloads**
   - SJF minimizes average waiting time
   - FIFO ensures fairness for low-priority background tasks

### Comparison with Linux CFS

| Metric | Custom Scheduler | Linux CFS |
|--------|-----------------|-----------|
| Average Turnaround | Lower | Higher |
| Average Waiting | Higher for low-priority | More equitable |
| Context Switches | Minimal | High |
| Responsiveness | Priority-based | Fair share |

**Use Case Suitability:**
- Custom Scheduler: Batch processing, HPC, deadline-critical systems
- CFS: Interactive workloads, desktop systems, mixed usage

## Files Delivered

```
custom-scheduler/
├── demo_script.sh          # Presentation demo script
├── README.md               # Project overview
├── QUICKSTART.md           # Quick start guide
├── docs/
│   └── design.md           # System design document
├── include/
│   └── custom_sched.h      # Header file
└── src/
    ├── custom_scheduler_v4.c   # Main kernel module
    ├── dashboard_v4.py         # Flask dashboard
    ├── Makefile                # Build system
    └── templates/
        └── dashboard_v4.html   # Web interface
```

## How to Run

### Prerequisites
- Linux system with kernel headers installed
- Python 3.x with Flask (`pip install flask`)
- Root/sudo access

### Execution
```bash
# Terminal 1: Run demo
sudo ./demo_script.sh

# Terminal 2: Start dashboard
python3 src/dashboard_v4.py
# Open http://localhost:5000
```

## Conclusion

This project successfully demonstrates:
1. **Kernel-level scheduling** is achievable with loadable modules
2. **Hybrid algorithms** can outperform general-purpose schedulers for specific workloads
3. **Real-time visualization** helps understand scheduler behavior
4. **Priority-based scheduling** is effective for deadline-sensitive applications

The custom scheduler shows particularly strong benefits for CPU-intensive and deadline-critical workloads, making it suitable for specialized computing environments.

## Future Enhancements

- Preemptive scheduling support
- Dynamic priority adjustment
- Multi-core awareness
- I/O wait handling
- Integration with real process scheduling
