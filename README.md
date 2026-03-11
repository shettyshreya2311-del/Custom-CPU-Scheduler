# Custom Hybrid CPU Scheduler

A Linux kernel module implementing a priority-based hybrid CPU scheduler with a real-time web dashboard for visualization.

## Overview

This project demonstrates true kernel-level task scheduling using a custom hybrid algorithm:

| Priority | Algorithm | Description |
|----------|-----------|-------------|
| **HIGH** | EDF (Earliest Deadline First) | Shortest burst time first among high-priority tasks |
| **MEDIUM** | SJF (Shortest Job First) | Shortest burst time first among medium-priority tasks |
| **LOW** | FIFO (First Come First Serve) | Execution in order of arrival |

## Features

- **True Kernel Scheduling**: Tasks are managed and executed by the Linux kernel, not simulated in user-space
- **Real-time Dashboard**: Flask-based web UI showing live task status, Gantt chart, and metrics
- **Priority Queues**: Three separate queues for HIGH, MEDIUM, and LOW priority tasks
- **Performance Metrics**: Turnaround time, waiting time, and context switches tracking
- **CFS Comparison**: Visual comparison between custom scheduler and Linux CFS

## Project Structure

```
custom-scheduler/
├── demo_script.sh              # Main presentation/demo script
├── README.md                   # This file
├── QUICKSTART.md               # Quick start guide
├── docs/
│   ├── design.md               # System design document
│   ├── scheduler_architecture.md # Architecture details
│   ├── kernel_integration_v4.md  # v4 kernel integration details
│   └── final_report.md         # Project final report
├── include/
│   └── custom_sched.h          # Header file
└── src/
    ├── custom_scheduler_v4.c   # Main kernel module (v4 - current)
    ├── custom_scheduler.c      # Legacy kernel module
    ├── scheduler_helpers.c     # Scheduling helper functions
    ├── scheduler_picker.c      # Task picker implementation
    ├── process_manager.c       # Process manager module
    ├── dashboard_v4.py         # Flask web dashboard
    ├── Makefile                # Build system
    └── templates/
        └── dashboard_v4.html   # Dashboard web interface
```

## Requirements

- Linux kernel headers (matching running kernel)
- GCC compiler
- Python 3.x with Flask (`pip install flask`)
- Root/sudo access for loading kernel modules

## Quick Start

### Terminal 1: Run Demo Script
```bash
sudo ./demo_script.sh
```

### Terminal 2: Start Dashboard
```bash
python3 src/dashboard_v4.py
```

Then open: **http://localhost:5000**

## How It Works

1. **Kernel Module** (`custom_scheduler_v4.ko`) creates a `/proc/custom_scheduler` interface
2. **Dashboard** sends task creation/start commands to the kernel via `/proc`
3. **Kernel** schedules tasks using the hybrid algorithm and executes them
4. **Dashboard** polls the kernel for status updates and displays real-time visualization

## Manual Testing (Without Dashboard)

```bash
# Create tasks
echo "create_task P1 0 3000" | sudo tee /proc/custom_scheduler  # HIGH priority, 3 seconds
echo "create_task P2 1 2000" | sudo tee /proc/custom_scheduler  # MEDIUM priority, 2 seconds
echo "create_task P3 2 1000" | sudo tee /proc/custom_scheduler  # LOW priority, 1 second

# Start execution
echo "start" | sudo tee /proc/custom_scheduler

# Check status
cat /proc/custom_scheduler

# Watch kernel logs
dmesg -w | grep Scheduler
```

## Building

```bash
cd src
make clean
make all
```

## License

GPL v2 (required for Linux kernel modules)
