/*
 * process_manager.c - Process management for custom scheduler
 * This module demonstrates how our scheduler would manage real processes
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/pid.h>
#include <linux/slab.h>
#include "../include/custom_sched.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Process Manager for Custom Scheduler");
MODULE_VERSION("3.0");

// External functions from other modules
extern void custom_enqueue_task(struct custom_rq *rq, struct task_struct *p);
extern void custom_dequeue_task(struct custom_rq *rq, struct task_struct *p);
extern struct custom_task_info *custom_pick_next_task(struct custom_rq *rq);

// Global run queue (we'll link to the one in custom_scheduler.c)
extern struct custom_rq global_rq;

/*
 * Add a process to our scheduler by PID
 * This simulates what would happen when a task becomes runnable
 */
static int add_process_by_pid(pid_t pid)
{
    struct task_struct *task;
    struct pid *pid_struct;
    
    printk(KERN_INFO "Process Manager: Attempting to add PID %d\n", pid);
    
    // Find the task_struct for this PID
    pid_struct = find_get_pid(pid);
    if (!pid_struct) {
        printk(KERN_ERR "Process Manager: PID %d not found\n", pid);
        return -ESRCH;
    }
    
    task = pid_task(pid_struct, PIDTYPE_PID);
    if (!task) {
        printk(KERN_ERR "Process Manager: No task for PID %d\n", pid);
        put_pid(pid_struct);
        return -ESRCH;
    }
    
    // Add to our scheduler
    custom_enqueue_task(&global_rq, task);
    
    printk(KERN_INFO "Process Manager: Added process '%s' (PID: %d, Priority: %d)\n",
           task->comm, task->pid, task->prio);
    
    put_pid(pid_struct);
    return 0;
}

/*
 * Demonstrate scheduler decision for current processes
 */
static void demonstrate_scheduling(void)
{
    struct task_struct *task;
    int count = 0;
    
    printk(KERN_INFO "========================================\n");
    printk(KERN_INFO "Process Manager: Scanning Current Processes\n");
    printk(KERN_INFO "========================================\n");
    
    // Iterate through all processes
    for_each_process(task) {
        if (count < 10) {  // Limit to first 10 for demonstration
            printk(KERN_INFO "Process: %-16s PID: %5d  Priority: %3d  State: %u\n",
                   task->comm, task->pid, task->prio, task->__state);
            
            // Simulate adding to our scheduler
            if (task->__state == TASK_RUNNING) {
                custom_enqueue_task(&global_rq, task);
                count++;
            }
        }
    }
    
    printk(KERN_INFO "\nProcess Manager: Added %d running processes to scheduler\n", count);
    
    // Now demonstrate task selection
    if (count > 0) {
        struct custom_task_info *next_task;
        
        printk(KERN_INFO "\nProcess Manager: Demonstrating Task Selection...\n");
        next_task = custom_pick_next_task(&global_rq);
        
        if (next_task) {
            printk(KERN_INFO "Process Manager: Selected task: %s (PID: %d)\n",
                   next_task->task->comm, next_task->task->pid);
        } else {
            printk(KERN_INFO "Process Manager: No task selected (queues empty)\n");
        }
    }
    
    printk(KERN_INFO "========================================\n");
}

/*
 * Display statistics about tasks in our queues
 */
static void display_queue_statistics(void)
{
    struct custom_task_info *task_info;
    unsigned long flags;
    int high = 0, medium = 0, low = 0;
    
    spin_lock_irqsave(&global_rq.lock, flags);
    
    printk(KERN_INFO "\n========================================\n");
    printk(KERN_INFO "Scheduler Queue Statistics\n");
    printk(KERN_INFO "========================================\n");
    
    // Count and display high priority tasks
    printk(KERN_INFO "\nHIGH Priority Queue (EDF):\n");
    list_for_each_entry(task_info, &global_rq.high_priority_tasks, list) {
        printk(KERN_INFO "  - %s (PID: %d, Deadline: %lu)\n",
               task_info->task->comm, task_info->task->pid, task_info->deadline);
        high++;
    }
    
    // Count and display medium priority tasks
    printk(KERN_INFO "\nMEDIUM Priority Queue (SJF):\n");
    list_for_each_entry(task_info, &global_rq.medium_priority_tasks, list) {
        printk(KERN_INFO "  - %s (PID: %d, Burst: %lu)\n",
               task_info->task->comm, task_info->task->pid, task_info->burst_time);
        medium++;
    }
    
    // Count and display low priority tasks
    printk(KERN_INFO "\nLOW Priority Queue (FIFO):\n");
    list_for_each_entry(task_info, &global_rq.low_priority_tasks, list) {
        printk(KERN_INFO "  - %s (PID: %d)\n",
               task_info->task->comm, task_info->task->pid);
        low++;
    }
    
    printk(KERN_INFO "\n========================================\n");
    printk(KERN_INFO "Total: %d tasks (%d HIGH, %d MEDIUM, %d LOW)\n",
           high + medium + low, high, medium, low);
    printk(KERN_INFO "========================================\n");
    
    spin_unlock_irqrestore(&global_rq.lock, flags);
}

/*
 * Module initialization
 */
static int __init process_manager_init(void)
{
    printk(KERN_INFO "========================================\n");
    printk(KERN_INFO "Process Manager: Initializing...\n");
    printk(KERN_INFO "========================================\n");
    
    // Demonstrate scheduling with current processes
    demonstrate_scheduling();
    
    // Display queue statistics
    display_queue_statistics();
    
    printk(KERN_INFO "\nProcess Manager: Initialization complete\n");
    printk(KERN_INFO "========================================\n");
    
    return 0;
}

/*
 * Module cleanup
 */
static void __exit process_manager_exit(void)
{
    printk(KERN_INFO "Process Manager: Cleaning up...\n");
    
    // Display final statistics
    display_queue_statistics();
    
    printk(KERN_INFO "Process Manager: Unloaded\n");
}

module_init(process_manager_init);
module_exit(process_manager_exit);
