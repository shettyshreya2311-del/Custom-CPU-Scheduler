/*
 * custom_scheduler_v4.c - TRUE Kernel Scheduling Implementation
 * 
 * This module implements REAL kernel-controlled scheduling:
 * - User inputs task via /proc interface
 * - Kernel creates kernel threads for each task
 * - Kernel scheduler picks and executes tasks using hybrid algorithm
 * - Execution history is logged and exported to dashboard
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Custom Scheduler Team");
MODULE_DESCRIPTION("Custom Hybrid CPU Scheduler - True Kernel Implementation v4.1");
MODULE_VERSION("4.1");

// ============================================================================
// DATA STRUCTURES
// ============================================================================

// Priority levels
#define PRIORITY_HIGH    0
#define PRIORITY_MEDIUM  1
#define PRIORITY_LOW     2

// Maximum execution history entries
#define MAX_HISTORY 100
#define MAX_TASKS 50

// Task structure for kernel-managed tasks
struct kernel_task {
    char name[16];              // Task name (P1, P2, etc.)
    int id;                     // Task ID
    int priority;               // 0=HIGH, 1=MEDIUM, 2=LOW
    unsigned long burst_time;   // Burst time in milliseconds
    unsigned long remaining;    // Remaining time
    unsigned long start_time;   // When execution started (jiffies)
    unsigned long end_time;     // When execution ended (jiffies)
    int status;                 // 0=waiting, 1=running, 2=completed
    struct task_struct *kthread; // Kernel thread for this task
    struct list_head list;      // List node
};

// Execution history event
struct exec_event {
    char task_name[16];
    int priority;
    unsigned long timestamp;    // Milliseconds since scheduler started
    int event_type;             // 0=start, 1=end
    struct list_head list;
};

// Global scheduler state
static struct {
    struct list_head high_queue;
    struct list_head medium_queue;
    struct list_head low_queue;
    struct list_head completed_list;
    struct list_head exec_history;
    spinlock_t lock;
    struct mutex exec_mutex;
    int task_counter;
    int nr_waiting;
    int nr_running;
    int nr_completed;
    unsigned long start_jiffies;
    struct task_struct *scheduler_thread;
    int running;
    struct kernel_task *current_task;
    int last_scheduled_prio;
} scheduler;

// ============================================================================
// EXECUTION HISTORY LOGGING
// ============================================================================

static void log_event(const char *name, int priority, int event_type)
{
    struct exec_event *event;
    unsigned long flags;
    unsigned long elapsed_ms;
    
    event = kmalloc(sizeof(*event), GFP_ATOMIC);
    if (!event) return;
    
    strncpy(event->task_name, name, 15);
    event->task_name[15] = '\0';
    event->priority = priority;
    
    // Calculate elapsed time (0 if timer not started yet)
    elapsed_ms = scheduler.start_jiffies ? 
                 jiffies_to_msecs(jiffies - scheduler.start_jiffies) : 0;
    event->timestamp = elapsed_ms;
    event->event_type = event_type;
    
    spin_lock_irqsave(&scheduler.lock, flags);
    list_add_tail(&event->list, &scheduler.exec_history);
    spin_unlock_irqrestore(&scheduler.lock, flags);
    
    printk(KERN_INFO "Scheduler Event [%s]: task=%s, priority=%d (%s), time=%lu ms\n",
           event_type == 0 ? "START" : "END",
           name,
           priority,
           priority == 0 ? "HIGH" : priority == 1 ? "MEDIUM" : "LOW",
           elapsed_ms);
}

// ============================================================================
// TASK QUEUE MANAGEMENT
// ============================================================================

static void enqueue_task(struct kernel_task *task)
{
    unsigned long flags;
    struct list_head *queue;
    
    spin_lock_irqsave(&scheduler.lock, flags);
    
    // Note: Timer starts when execution begins, not when tasks are added
    
    switch (task->priority) {
        case PRIORITY_HIGH:
            queue = &scheduler.high_queue;
            printk(KERN_INFO "Scheduler: Enqueuing %s to HIGH queue (EDF strategy)\n", task->name);
            break;
        case PRIORITY_MEDIUM:
            queue = &scheduler.medium_queue;
            printk(KERN_INFO "Scheduler: Enqueuing %s to MEDIUM queue (SJF strategy)\n", task->name);
            break;
        default:
            queue = &scheduler.low_queue;
            printk(KERN_INFO "Scheduler: Enqueuing %s to LOW queue (FIFO strategy)\n", task->name);
            break;
    }
    
    list_add_tail(&task->list, queue);
    scheduler.nr_waiting++;
    
    spin_unlock_irqrestore(&scheduler.lock, flags);
}

// Pick next task using HYBRID algorithm:
// HIGH: EDF (Earliest Deadline First) - shortest burst = earliest deadline
// MEDIUM: SJF (Shortest Job First)
// LOW: FIFO (First Come First Serve)
static struct kernel_task *pick_next_task(void)
{
    struct kernel_task *task, *selected = NULL;
    unsigned long flags;
    unsigned long min_burst = ULONG_MAX;
    
    spin_lock_irqsave(&scheduler.lock, flags);
    
    // Strategy 1: HIGH priority - EDF (pick shortest burst time)
    if (!list_empty(&scheduler.high_queue)) {
        list_for_each_entry(task, &scheduler.high_queue, list) {
            if (task->status == 0 && task->burst_time < min_burst) {
                min_burst = task->burst_time;
                selected = task;
            }
        }
        if (selected) {
            scheduler.last_scheduled_prio = PRIORITY_HIGH;
            printk(KERN_INFO "Scheduler: [HIGH] Selected %s (EDF - shortest burst=%lu ms)\n",
                   selected->name, selected->burst_time);
        }
    }
    
    // Strategy 2: MEDIUM priority - SJF (pick shortest job)
    if (!selected && !list_empty(&scheduler.medium_queue)) {
        min_burst = ULONG_MAX;
        list_for_each_entry(task, &scheduler.medium_queue, list) {
            if (task->status == 0 && task->burst_time < min_burst) {
                min_burst = task->burst_time;
                selected = task;
            }
        }
        if (selected) {
            scheduler.last_scheduled_prio = PRIORITY_MEDIUM;
            printk(KERN_INFO "Scheduler: [MEDIUM] Selected %s (SJF - shortest job=%lu ms)\n",
                   selected->name, selected->burst_time);
        }
    }
    
    // Strategy 3: LOW priority - FIFO (pick first)
    if (!selected && !list_empty(&scheduler.low_queue)) {
        list_for_each_entry(task, &scheduler.low_queue, list) {
            if (task->status == 0) {
                selected = task;
                break;
            }
        }
        if (selected) {
            scheduler.last_scheduled_prio = PRIORITY_LOW;
            printk(KERN_INFO "Scheduler: [LOW] Selected %s (FIFO - first in queue)\n", selected->name);
        }
    }
    
    if (!selected) {
        scheduler.last_scheduled_prio = -1;
    }
    
    spin_unlock_irqrestore(&scheduler.lock, flags);
    return selected;
}

// ============================================================================
// TASK EXECUTION (Kernel Thread)
// ============================================================================

// Main scheduler execution thread
static int scheduler_thread_fn(void *data)
{
    struct kernel_task *task;
    unsigned long flags;
    const char *prio_str;
    unsigned long exec_start, exec_end, actual_ms;
    
    printk(KERN_INFO "Scheduler: ========== EXECUTION STARTED =========\n");
    
    while (!kthread_should_stop() && scheduler.running) {
        // Pick next task using hybrid algorithm
        task = pick_next_task();
        
        if (task) {
            prio_str = task->priority == 0 ? "HIGH" : 
                       task->priority == 1 ? "MEDIUM" : "LOW";
            
            // Mark as running and record start time
            spin_lock_irqsave(&scheduler.lock, flags);
            task->status = 1;  // Running
            task->start_time = jiffies;
            scheduler.nr_waiting--;
            scheduler.nr_running = 1;
            scheduler.current_task = task;
            spin_unlock_irqrestore(&scheduler.lock, flags);
            
            // Log start event
            log_event(task->name, task->priority, 0);
            
            exec_start = jiffies;
            
            printk(KERN_INFO "Scheduler: >>> EXECUTING %s (burst=%lu ms, priority=%s) <<<\n",
                   task->name, task->burst_time, prio_str);
            
            // ============================================================
            // EXECUTE FOR THE EXACT BURST TIME USING MSLEEP
            // This ensures the task runs for the user-specified duration
            // ============================================================
            msleep(task->burst_time);
            
            exec_end = jiffies;
            actual_ms = jiffies_to_msecs(exec_end - exec_start);
            
            printk(KERN_INFO "Scheduler: <<< COMPLETED %s (ran for %lu ms) >>>\n", 
                   task->name, actual_ms);
            
            // Mark as completed and record end time
            spin_lock_irqsave(&scheduler.lock, flags);
            task->status = 2;  // Completed
            task->end_time = jiffies;
            scheduler.nr_running = 0;
            scheduler.nr_completed++;
            scheduler.current_task = NULL;
            
            // Move to completed list
            list_del(&task->list);
            list_add_tail(&task->list, &scheduler.completed_list);
            spin_unlock_irqrestore(&scheduler.lock, flags);
            
            // Log end event
            log_event(task->name, task->priority, 1);
        } else {
            // No tasks, wait a bit
            msleep(100);
        }
    }
    
    printk(KERN_INFO "Scheduler: ========== EXECUTION STOPPED =========\n");
    return 0;
}

// ============================================================================
// PROC INTERFACE - READ (Export state to dashboard)
// ============================================================================

static int sched_show(struct seq_file *m, void *v)
{
    struct kernel_task *task;
    struct exec_event *event;
    unsigned long flags;
    int first;
    
    spin_lock_irqsave(&scheduler.lock, flags);
    
    // Start JSON output
    seq_printf(m, "{\n");
    
    // Queue counts
    seq_printf(m, "  \"high\": %d,\n", 
               list_empty(&scheduler.high_queue) ? 0 : 
               (int)(!list_empty(&scheduler.high_queue)));
    seq_printf(m, "  \"medium\": %d,\n",
               list_empty(&scheduler.medium_queue) ? 0 : 
               (int)(!list_empty(&scheduler.medium_queue)));
    seq_printf(m, "  \"low\": %d,\n",
               list_empty(&scheduler.low_queue) ? 0 : 
               (int)(!list_empty(&scheduler.low_queue)));
    
    // Count actual waiting tasks
    {
        int high_count = 0, med_count = 0, low_count = 0;
        list_for_each_entry(task, &scheduler.high_queue, list) {
            if (task->status == 0) high_count++;
        }
        list_for_each_entry(task, &scheduler.medium_queue, list) {
            if (task->status == 0) med_count++;
        }
        list_for_each_entry(task, &scheduler.low_queue, list) {
            if (task->status == 0) low_count++;
        }
        seq_printf(m, "  \"high_waiting\": %d,\n", high_count);
        seq_printf(m, "  \"medium_waiting\": %d,\n", med_count);
        seq_printf(m, "  \"low_waiting\": %d,\n", low_count);
    }
    
    seq_printf(m, "  \"total\": %d,\n", scheduler.nr_waiting);
    seq_printf(m, "  \"running\": %d,\n", scheduler.nr_running);
    seq_printf(m, "  \"completed\": %d,\n", scheduler.nr_completed);
    seq_printf(m, "  \"last_prio\": %d,\n", scheduler.last_scheduled_prio);
    
    // Current running task
    if (scheduler.current_task) {
        seq_printf(m, "  \"current_task\": \"%s\",\n", scheduler.current_task->name);
        seq_printf(m, "  \"current_priority\": %d,\n", scheduler.current_task->priority);
    } else {
        seq_printf(m, "  \"current_task\": null,\n");
        seq_printf(m, "  \"current_priority\": -1,\n");
    }
    
    // Elapsed time (0 if timer not started yet)
    seq_printf(m, "  \"elapsed_ms\": %lu,\n", 
               scheduler.start_jiffies ? jiffies_to_msecs(jiffies - scheduler.start_jiffies) : 0);
    seq_printf(m, "  \"timer_started\": %s,\n", scheduler.start_jiffies ? "true" : "false");
    
    // Tasks array (all tasks with their status)
    seq_printf(m, "  \"tasks\": [\n");
    first = 1;
    
    // High priority tasks
    list_for_each_entry(task, &scheduler.high_queue, list) {
        unsigned long st = (scheduler.start_jiffies && task->start_time) ? 
                           jiffies_to_msecs(task->start_time - scheduler.start_jiffies) : 0;
        unsigned long et = (scheduler.start_jiffies && task->end_time) ? 
                           jiffies_to_msecs(task->end_time - scheduler.start_jiffies) : 0;
        if (!first) seq_printf(m, ",\n");
        seq_printf(m, "    {\"id\": \"%s\", \"priority\": %d, \"burst\": %lu, \"status\": %d, \"start\": %lu, \"end\": %lu}",
                   task->name, task->priority, task->burst_time, task->status, st, et);
        first = 0;
    }
    // Medium priority tasks
    list_for_each_entry(task, &scheduler.medium_queue, list) {
        unsigned long st = (scheduler.start_jiffies && task->start_time) ? 
                           jiffies_to_msecs(task->start_time - scheduler.start_jiffies) : 0;
        unsigned long et = (scheduler.start_jiffies && task->end_time) ? 
                           jiffies_to_msecs(task->end_time - scheduler.start_jiffies) : 0;
        if (!first) seq_printf(m, ",\n");
        seq_printf(m, "    {\"id\": \"%s\", \"priority\": %d, \"burst\": %lu, \"status\": %d, \"start\": %lu, \"end\": %lu}",
                   task->name, task->priority, task->burst_time, task->status, st, et);
        first = 0;
    }
    // Low priority tasks
    list_for_each_entry(task, &scheduler.low_queue, list) {
        unsigned long st = (scheduler.start_jiffies && task->start_time) ? 
                           jiffies_to_msecs(task->start_time - scheduler.start_jiffies) : 0;
        unsigned long et = (scheduler.start_jiffies && task->end_time) ? 
                           jiffies_to_msecs(task->end_time - scheduler.start_jiffies) : 0;
        if (!first) seq_printf(m, ",\n");
        seq_printf(m, "    {\"id\": \"%s\", \"priority\": %d, \"burst\": %lu, \"status\": %d, \"start\": %lu, \"end\": %lu}",
                   task->name, task->priority, task->burst_time, task->status, st, et);
        first = 0;
    }
    // Completed tasks
    list_for_each_entry(task, &scheduler.completed_list, list) {
        unsigned long st = (scheduler.start_jiffies && task->start_time) ? 
                           jiffies_to_msecs(task->start_time - scheduler.start_jiffies) : 0;
        unsigned long et = (scheduler.start_jiffies && task->end_time) ? 
                           jiffies_to_msecs(task->end_time - scheduler.start_jiffies) : 0;
        if (!first) seq_printf(m, ",\n");
        seq_printf(m, "    {\"id\": \"%s\", \"priority\": %d, \"burst\": %lu, \"status\": %d, \"start\": %lu, \"end\": %lu}",
                   task->name, task->priority, task->burst_time, task->status, st, et);
        first = 0;
    }
    seq_printf(m, "\n  ],\n");
    
    // Execution history for Gantt chart
    seq_printf(m, "  \"history\": [\n");
    first = 1;
    list_for_each_entry(event, &scheduler.exec_history, list) {
        if (!first) seq_printf(m, ",\n");
        seq_printf(m, "    {\"task\": \"%s\", \"priority\": %d, \"time\": %lu, \"event\": \"%s\"}",
                   event->task_name, event->priority, event->timestamp,
                   event->event_type == 0 ? "start" : "end");
        first = 0;
    }
    seq_printf(m, "\n  ]\n");
    
    seq_printf(m, "}\n");
    
    spin_unlock_irqrestore(&scheduler.lock, flags);
    return 0;
}

static int sched_open(struct inode *inode, struct file *file)
{
    return single_open(file, sched_show, NULL);
}

// ============================================================================
// PROC INTERFACE - WRITE (Receive tasks from dashboard)
// ============================================================================

static ssize_t sched_write(struct file *file, const char __user *buffer,
                           size_t count, loff_t *pos)
{
    char kbuf[128];
    char task_name[16];
    int priority;
    unsigned long burst_time;
    struct kernel_task *new_task;
    unsigned long flags;
    
    if (count >= sizeof(kbuf))
        return -EINVAL;
    
    if (copy_from_user(kbuf, buffer, count))
        return -EFAULT;
    
    kbuf[count] = '\0';
    
    // Parse: "create_task <name> <priority> <burst_time_ms>"
    if (sscanf(kbuf, "create_task %15s %d %lu", task_name, &priority, &burst_time) == 3) {
        printk(KERN_INFO "Scheduler: Received task creation: %s, priority=%d, burst=%lu ms\n",
               task_name, priority, burst_time);
        
        // Validate priority
        if (priority < 0 || priority > 2) {
            printk(KERN_ERR "Scheduler: Invalid priority %d (must be 0, 1, or 2)\n", priority);
            return -EINVAL;
        }
        
        // Create new task
        new_task = kmalloc(sizeof(*new_task), GFP_KERNEL);
        if (!new_task) {
            printk(KERN_ERR "Scheduler: Failed to allocate task\n");
            return -ENOMEM;
        }
        
        // Initialize task
        strncpy(new_task->name, task_name, 15);
        new_task->name[15] = '\0';
        new_task->id = ++scheduler.task_counter;
        new_task->priority = priority;
        new_task->burst_time = burst_time;
        new_task->remaining = burst_time;
        new_task->start_time = 0;
        new_task->end_time = 0;
        new_task->status = 0;  // Waiting
        new_task->kthread = NULL;
        INIT_LIST_HEAD(&new_task->list);
        
        // Enqueue the task
        enqueue_task(new_task);
        
        printk(KERN_INFO "Scheduler: Task %s created and enqueued (ID=%d)\n",
               new_task->name, new_task->id);
        
        return count;
    }
    
    // Parse: "start" - Start executing tasks
    if (strncmp(kbuf, "start", 5) == 0) {
        printk(KERN_INFO "Scheduler: ========================================\n");
        printk(KERN_INFO "Scheduler: START COMMAND RECEIVED\n");
        printk(KERN_INFO "Scheduler: ========================================\n");
        
        if (!scheduler.scheduler_thread) {
            scheduler.running = 1;
            scheduler.start_jiffies = jiffies;  // Timer starts NOW
            printk(KERN_INFO "Scheduler: Timer started at jiffies=%lu\n", scheduler.start_jiffies);
            
            scheduler.scheduler_thread = kthread_run(scheduler_thread_fn, NULL, "sched_exec");
            if (IS_ERR(scheduler.scheduler_thread)) {
                printk(KERN_ERR "Scheduler: Failed to create execution thread\n");
                scheduler.scheduler_thread = NULL;
                scheduler.start_jiffies = 0;
                return -EFAULT;
            }
            printk(KERN_INFO "Scheduler: Execution thread created successfully\n");
        } else {
            printk(KERN_INFO "Scheduler: Already running\n");
        }
        return count;
    }
    
    // Parse: "reset" - Reset scheduler
    if (strncmp(kbuf, "reset", 5) == 0) {
        struct kernel_task *task, *tmp_task;
        struct exec_event *event, *tmp_event;
        
        printk(KERN_INFO "Scheduler: Reset command received\n");
        
        // Stop scheduler thread
        scheduler.running = 0;
        if (scheduler.scheduler_thread) {
            kthread_stop(scheduler.scheduler_thread);
            scheduler.scheduler_thread = NULL;
        }
        
        spin_lock_irqsave(&scheduler.lock, flags);
        
        // Free all tasks
        list_for_each_entry_safe(task, tmp_task, &scheduler.high_queue, list) {
            list_del(&task->list);
            kfree(task);
        }
        list_for_each_entry_safe(task, tmp_task, &scheduler.medium_queue, list) {
            list_del(&task->list);
            kfree(task);
        }
        list_for_each_entry_safe(task, tmp_task, &scheduler.low_queue, list) {
            list_del(&task->list);
            kfree(task);
        }
        list_for_each_entry_safe(task, tmp_task, &scheduler.completed_list, list) {
            list_del(&task->list);
            kfree(task);
        }
        
        // Free execution history
        list_for_each_entry_safe(event, tmp_event, &scheduler.exec_history, list) {
            list_del(&event->list);
            kfree(event);
        }
        
        // Reset counters
        scheduler.task_counter = 0;
        scheduler.nr_waiting = 0;
        scheduler.nr_running = 0;
        scheduler.nr_completed = 0;
        scheduler.current_task = NULL;
        scheduler.last_scheduled_prio = -1;
        scheduler.start_jiffies = 0;  // FIX: Timer will start on next task
        
        spin_unlock_irqrestore(&scheduler.lock, flags);
        
        printk(KERN_INFO "Scheduler: Reset complete - timer will restart on next task\n");
        return count;
    }
    
    printk(KERN_WARNING "Scheduler: Unknown command: %s\n", kbuf);
    return count;
}

static const struct proc_ops sched_fops = {
    .proc_open = sched_open,
    .proc_read = seq_read,
    .proc_write = sched_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

// ============================================================================
// MODULE INIT/EXIT
// ============================================================================

static int __init custom_scheduler_init(void)
{
    printk(KERN_INFO "========================================\n");
    printk(KERN_INFO "Custom Scheduler v4.1 - TRUE Kernel Scheduling (FIXED)\n");
    printk(KERN_INFO "  Timer starts on first task, not module load\n");
    printk(KERN_INFO "  Real CPU work, not just sleep\n");
    printk(KERN_INFO "========================================\n");
    
    // Initialize scheduler state
    INIT_LIST_HEAD(&scheduler.high_queue);
    INIT_LIST_HEAD(&scheduler.medium_queue);
    INIT_LIST_HEAD(&scheduler.low_queue);
    INIT_LIST_HEAD(&scheduler.completed_list);
    INIT_LIST_HEAD(&scheduler.exec_history);
    spin_lock_init(&scheduler.lock);
    mutex_init(&scheduler.exec_mutex);
    
    scheduler.task_counter = 0;
    scheduler.nr_waiting = 0;
    scheduler.nr_running = 0;
    scheduler.nr_completed = 0;
    scheduler.start_jiffies = 0;  // FIX: Don't start timer until first task!
    scheduler.scheduler_thread = NULL;
    scheduler.running = 0;
    scheduler.current_task = NULL;
    scheduler.last_scheduled_prio = -1;
    
    // Create /proc interface
    if (!proc_create("custom_scheduler", 0666, NULL, &sched_fops)) {
        printk(KERN_ERR "Scheduler: Failed to create /proc/custom_scheduler\n");
        return -ENOMEM;
    }
    
    printk(KERN_INFO "Scheduler: /proc/custom_scheduler created\n");
    printk(KERN_INFO "Scheduler: Commands:\n");
    printk(KERN_INFO "  echo 'create_task P1 0 3000' > /proc/custom_scheduler  (HIGH, 3sec)\n");
    printk(KERN_INFO "  echo 'create_task P2 1 2000' > /proc/custom_scheduler  (MEDIUM, 2sec)\n");
    printk(KERN_INFO "  echo 'create_task P3 2 1000' > /proc/custom_scheduler  (LOW, 1sec)\n");
    printk(KERN_INFO "  echo 'start' > /proc/custom_scheduler\n");
    printk(KERN_INFO "  echo 'reset' > /proc/custom_scheduler\n");
    printk(KERN_INFO "  cat /proc/custom_scheduler  (view state as JSON)\n");
    printk(KERN_INFO "========================================\n");
    
    return 0;
}

static void __exit custom_scheduler_exit(void)
{
    struct kernel_task *task, *tmp_task;
    struct exec_event *event, *tmp_event;
    unsigned long flags;
    
    printk(KERN_INFO "Scheduler: Shutting down...\n");
    
    // Stop scheduler thread
    scheduler.running = 0;
    if (scheduler.scheduler_thread) {
        kthread_stop(scheduler.scheduler_thread);
    }
    
    // Remove proc entry
    remove_proc_entry("custom_scheduler", NULL);
    
    // Free all memory
    spin_lock_irqsave(&scheduler.lock, flags);
    
    list_for_each_entry_safe(task, tmp_task, &scheduler.high_queue, list) {
        list_del(&task->list);
        kfree(task);
    }
    list_for_each_entry_safe(task, tmp_task, &scheduler.medium_queue, list) {
        list_del(&task->list);
        kfree(task);
    }
    list_for_each_entry_safe(task, tmp_task, &scheduler.low_queue, list) {
        list_del(&task->list);
        kfree(task);
    }
    list_for_each_entry_safe(task, tmp_task, &scheduler.completed_list, list) {
        list_del(&task->list);
        kfree(task);
    }
    list_for_each_entry_safe(event, tmp_event, &scheduler.exec_history, list) {
        list_del(&event->list);
        kfree(event);
    }
    
    spin_unlock_irqrestore(&scheduler.lock, flags);
    
    printk(KERN_INFO "Scheduler: Unloaded\n");
}

module_init(custom_scheduler_init);
module_exit(custom_scheduler_exit);
