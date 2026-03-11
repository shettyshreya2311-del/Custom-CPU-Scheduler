# Custom Scheduler v4.1 - Quick Start Guide

## Running the Demo (Two Terminals)

### Terminal 1: Run Presentation Script
```bash
sudo ./demo_script.sh
```
This will walk through:
- Building the kernel module
- Showing project structure
- Loading the module
- Displaying kernel logs

### Terminal 2: Start Dashboard
```bash
python3 src/dashboard_v4.py
```
Then open: **http://localhost:5000**

---

## Dashboard Features

- **Add Tasks**: Specify name, priority (HIGH/MEDIUM/LOW), and burst time
- **Start Execution**: Kernel schedules and executes all tasks
- **Gantt Chart**: Real-time visualization of task execution
- **Metrics**: Turnaround time, waiting time, context switches
- **CFS Comparison**: Compare with Linux CFS scheduler

---

## Scheduling Algorithm

| Priority | Algorithm | Behavior |
|----------|-----------|----------|
| **HIGH (0)** | EDF | Shortest burst runs first |
| **MEDIUM (1)** | SJF | Shortest burst runs first |
| **LOW (2)** | FIFO | First come, first served |

### Example:
Add tasks: P1(LOW, 5s), P2(HIGH, 3s), P3(MEDIUM, 2s), P4(HIGH, 1s)

**Execution Order:** P4 → P2 → P3 → P1

Why?
- HIGH queue: P4(1s) before P2(3s) - shorter burst first (EDF)
- MEDIUM queue: Only P3
- LOW queue: P1 runs last (FIFO)

---

## Manual Testing (Without Dashboard)

```bash
# Create tasks
echo "create_task P1 0 3000" | sudo tee /proc/custom_scheduler
echo "create_task P2 1 2000" | sudo tee /proc/custom_scheduler
echo "start" | sudo tee /proc/custom_scheduler

# Check status
cat /proc/custom_scheduler

# Watch kernel logs
dmesg | grep Scheduler
```

---

## Cleanup

Press **Ctrl+C** in Terminal 1 to unload module, or:
```bash
sudo rmmod custom_scheduler_v4
```
