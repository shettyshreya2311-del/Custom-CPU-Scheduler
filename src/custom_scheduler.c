/*
 * custom_scheduler.c - Main Module with Decision Tracking
 * Updated: Real-time process registration via /proc interface
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/pid.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "../include/custom_sched.h" 

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Custom Hybrid CPU Scheduler");
MODULE_VERSION("3.0");

struct custom_rq global_rq;
EXPORT_SYMBOL(global_rq);

// --- Global decision variable ---
int last_scheduled_prio = -1; 
EXPORT_SYMBOL(last_scheduled_prio);

// --- Scheduler simulation thread ---
static struct task_struct *scheduler_thread;
static int scheduler_running = 1;

// Function declarations (implemented in other files)
extern void custom_enqueue_task(struct custom_rq *rq, struct task_struct *p);
extern void custom_dequeue_task(struct custom_rq *rq, struct task_struct *p);
extern struct custom_task_info *custom_pick_next_task(struct custom_rq *rq);
extern void custom_enqueue_task_with_priority(struct custom_rq *rq, struct task_struct *p, int forced_priority);

// --- DASHBOARD DATA EXPORTER ---
static int sched_show(struct seq_file *m, void *v)
{
    struct custom_task_info *task;
    int high = 0, medium = 0, low = 0;
    unsigned long flags;
    
    // Lock to ensure we get accurate counts
    spin_lock_irqsave(&global_rq.lock, flags);
    
    // Count tasks in queues for the Bar Chart
    list_for_each_entry(task, &global_rq.high_priority_tasks, list) high++;
    list_for_each_entry(task, &global_rq.medium_priority_tasks, list) medium++;
    list_for_each_entry(task, &global_rq.low_priority_tasks, list) low++;
    
    spin_unlock_irqrestore(&global_rq.lock, flags);
    
    // Export data as JSON including last scheduled priority
    seq_printf(m, "{\"high\": %d, \"medium\": %d, \"low\": %d, \"total\": %d, \"last_prio\": %d}\n",
               high, medium, low, global_rq.nr_running, last_scheduled_prio);
    return 0;
}

static int sched_open(struct inode *inode, struct file *file)
{
    return single_open(file, sched_show, NULL);
}

// --- PROCESS REGISTRATION HANDLER ---
// Write format: "register <pid> <priority_level>"
// priority_level: 0=HIGH, 1=MEDIUM, 2=LOW
static ssize_t sched_write(struct file *file, const char __user *buffer, 
                           size_t count, loff_t *pos)
{
    char kbuf[64];
    pid_t pid;
    int priority_level;
    struct task_struct *task;
    struct pid *pid_struct;
    
    if (count >= sizeof(kbuf))
        return -EINVAL;
    
    if (copy_from_user(kbuf, buffer, count))
        return -EFAULT;
    
    kbuf[count] = '\0';
    
    // Parse: "register <pid> <priority>"
    if (sscanf(kbuf, "register %d %d", &pid, &priority_level) == 2) {
        printk(KERN_INFO "Custom Scheduler: Registering PID %d with priority %d\n", 
               pid, priority_level);
        
        // Find the task
        pid_struct = find_get_pid(pid);
        if (!pid_struct) {
            printk(KERN_ERR "Custom Scheduler: PID %d not found\n", pid);
            return -ESRCH;
        }
        
        task = pid_task(pid_struct, PIDTYPE_PID);
        if (!task) {
            put_pid(pid_struct);
            return -ESRCH;
        }
        
        // Enqueue with specified priority
        custom_enqueue_task_with_priority(&global_rq, task, priority_level);
        put_pid(pid_struct);
        
        printk(KERN_INFO "Custom Scheduler: Task '%s' (PID: %d) registered as priority %d\n",
               task->comm, pid, priority_level);
    }
    // Parse: "unregister <pid>"
    else if (sscanf(kbuf, "unregister %d", &pid) == 1) {
        printk(KERN_INFO "Custom Scheduler: Unregistering PID %d\n", pid);
        
        pid_struct = find_get_pid(pid);
        if (pid_struct) {
            task = pid_task(pid_struct, PIDTYPE_PID);
            if (task) {
                custom_dequeue_task(&global_rq, task);
            }
            put_pid(pid_struct);
        }
    }
    
    return count;
}

static const struct proc_ops sched_fops = {
    .proc_open = sched_open,
    .proc_read = seq_read,
    .proc_write = sched_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

// --- SCHEDULER SIMULATION THREAD ---
// This thread periodically picks tasks to simulate scheduling decisions
static int scheduler_thread_fn(void *data)
{
    while (!kthread_should_stop() && scheduler_running) {
        struct custom_task_info *next_task;
        
        // Pick next task (this updates last_scheduled_prio)
        next_task = custom_pick_next_task(&global_rq);
        
        if (next_task) {
            printk(KERN_DEBUG "Scheduler: Would run %s (prio level: %d)\n",
                   next_task->task->comm, last_scheduled_prio);
        }
        
        // Check every 100ms
        msleep(100);
    }
    return 0;
}

static int __init custom_scheduler_init(void)
{
    printk(KERN_INFO "Custom Scheduler: Initializing v3.0...\n");
    
    INIT_LIST_HEAD(&global_rq.high_priority_tasks);
    INIT_LIST_HEAD(&global_rq.medium_priority_tasks);
    INIT_LIST_HEAD(&global_rq.low_priority_tasks);
    spin_lock_init(&global_rq.lock);
    global_rq.nr_running = 0;
    
    // Create the /proc file for the dashboard (with write support)
    proc_create("custom_scheduler", 0666, NULL, &sched_fops);
    
    // Start the scheduler simulation thread
    scheduler_thread = kthread_run(scheduler_thread_fn, NULL, "custom_sched");
    if (IS_ERR(scheduler_thread)) {
        printk(KERN_ERR "Custom Scheduler: Failed to create thread\n");
        scheduler_thread = NULL;
    }
    
    printk(KERN_INFO "Custom Scheduler: /proc/custom_scheduler created (read/write)\n");
    printk(KERN_INFO "Custom Scheduler: Use 'echo \"register <pid> <priority>\" > /proc/custom_scheduler'\n");
    return 0;
}

static void __exit custom_scheduler_exit(void)
{
    struct custom_task_info *task_info, *tmp;
    unsigned long flags;
    
    // Stop the scheduler thread
    scheduler_running = 0;
    if (scheduler_thread) {
        kthread_stop(scheduler_thread);
    }
    
    remove_proc_entry("custom_scheduler", NULL);
    
    spin_lock_irqsave(&global_rq.lock, flags);
    // Cleanup high queue
    list_for_each_entry_safe(task_info, tmp, &global_rq.high_priority_tasks, list) {
        list_del(&task_info->list); kfree(task_info);
    }
    // Cleanup medium queue
    list_for_each_entry_safe(task_info, tmp, &global_rq.medium_priority_tasks, list) {
        list_del(&task_info->list); kfree(task_info);
    }
    // Cleanup low queue
    list_for_each_entry_safe(task_info, tmp, &global_rq.low_priority_tasks, list) {
        list_del(&task_info->list); kfree(task_info);
    }
    spin_unlock_irqrestore(&global_rq.lock, flags);
    
    printk(KERN_INFO "Custom Scheduler: Unloaded\n");
}

module_init(custom_scheduler_init);
module_exit(custom_scheduler_exit);
