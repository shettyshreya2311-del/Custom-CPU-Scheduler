/*
 * scheduler_helpers.c - Helper functions with EMA "Learning"
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include "../include/custom_sched.h"

/*
 * Determine priority level based on task priority value
 */
static int get_priority_level(int prio)
{
    int scaled_prio = (prio * 100) / 140;
    
    if (scaled_prio <= HIGH_PRIORITY_MAX) {
        return PRIORITY_HIGH;
    } else if (scaled_prio <= MEDIUM_PRIORITY_MAX) {
        return PRIORITY_MEDIUM;
    } else {
        return PRIORITY_LOW;
    }
}

/*
 * Calculate deadline based on priority
 */
static unsigned long calculate_deadline(struct task_struct *task, int priority_level)
{
    switch (priority_level) {
        case PRIORITY_HIGH:
            return jiffies + msecs_to_jiffies(100);
        case PRIORITY_MEDIUM:
            return jiffies + msecs_to_jiffies(500);
        case PRIORITY_LOW:
            return jiffies + msecs_to_jiffies(1000);
        default:
            return jiffies + msecs_to_jiffies(500);
    }
}

/*
 * Smart Estimate: Uses Exponential Moving Average (EMA)
 * Formula: Predicted = (Alpha * Actual) + ((1-Alpha) * Previous_Predicted)
 * We use Alpha = 0.5 for simple integer math (average of both)
 */
static unsigned long estimate_burst_time(struct custom_task_info *task_info)
{
    // 1. New task? Use default 10ms
    if (task_info->last_burst_actual == 0) {
        return msecs_to_jiffies(10);
    }

    // 2. EMA Calculation: (Previous_Predicted + Actual) / 2
    // Using bitwise shift (>> 1) is efficient division by 2
    unsigned long predicted = (task_info->burst_time + task_info->last_burst_actual) >> 1;
    
    // 3. Safety check: Don't let prediction fall to 0
    return (predicted == 0) ? 1 : predicted;
}

/*
 * Enqueue a task
 */
void custom_enqueue_task(struct custom_rq *rq, struct task_struct *p)
{
    struct custom_task_info *task_info;
    int priority_level;
    struct list_head *queue;
    unsigned long flags;
    
    // Allocate memory
    task_info = kmalloc(sizeof(struct custom_task_info), GFP_ATOMIC);
    if (!task_info) {
        printk(KERN_ERR "Custom Scheduler: Failed to allocate task_info\n");
        return;
    }
    
    // Initialize Basic Info
    task_info->task = p;
    task_info->priority = p->prio;
    priority_level = get_priority_level(p->prio);
    task_info->deadline = calculate_deadline(p, priority_level);
    
    // --- START LEARNING LOGIC ---
    // In a real OS, we would fetch 'last_burst_actual' from a persistent history table.
    // Since we are re-allocating here, we simulate it by assuming 0 for fresh tasks.
    task_info->last_burst_actual = 0; 
    
    // 1. Calculate Prediction
    task_info->burst_time = estimate_burst_time(task_info);
    
    // 2. Record Start Time (Critical for calculating actual duration later)
    task_info->enqueue_time = jiffies;
    
    task_info->remaining_time = task_info->burst_time;
    // --- END LEARNING LOGIC ---

    INIT_LIST_HEAD(&task_info->list);
    
    spin_lock_irqsave(&rq->lock, flags);
    
    switch (priority_level) {
        case PRIORITY_HIGH:
            queue = &rq->high_priority_tasks;
            break;
        case PRIORITY_MEDIUM:
            queue = &rq->medium_priority_tasks;
            break;
        case PRIORITY_LOW:
            queue = &rq->low_priority_tasks;
            break;
        default:
            queue = &rq->medium_priority_tasks;
    }
    
    list_add_tail(&task_info->list, queue);
    rq->nr_running++;
    
    spin_unlock_irqrestore(&rq->lock, flags);
    
    printk(KERN_INFO "Scheduler: Enqueued %s (Pred: %lu jiffies)\n", 
           p->comm, task_info->burst_time);
}

/*
 * Dequeue a task
 */
void custom_dequeue_task(struct custom_rq *rq, struct task_struct *p)
{
    struct custom_task_info *task_info, *tmp;
    unsigned long flags;
    struct list_head *queues[] = {
        &rq->high_priority_tasks, 
        &rq->medium_priority_tasks, 
        &rq->low_priority_tasks
    };
    int i;
    
    spin_lock_irqsave(&rq->lock, flags);
    
    for (i = 0; i < 3; i++) {
        list_for_each_entry_safe(task_info, tmp, queues[i], list) {
            if (task_info->task == p) {
                // --- START LEARNING LOGIC ---
                // 1. Calculate how long it actually ran
                unsigned long actual_duration = jiffies - task_info->enqueue_time;
                
                // 2. Update the 'actual' field (This is where we "SAVE" it)
                task_info->last_burst_actual = actual_duration;
                
                printk(KERN_INFO "Scheduler: Task %s finished. Predicted: %lu, Actual: %lu jiffies\n",
                       p->comm, task_info->burst_time, actual_duration);
                // --- END LEARNING LOGIC ---

                list_del(&task_info->list);
                kfree(task_info); // In a persistent system, we would save stats before freeing
                rq->nr_running--;
                
                spin_unlock_irqrestore(&rq->lock, flags);
                return;
            }
        }
    }
    
    spin_unlock_irqrestore(&rq->lock, flags);
}

EXPORT_SYMBOL(custom_enqueue_task);
EXPORT_SYMBOL(custom_dequeue_task);

/*
 * Enqueue a task with EXPLICIT priority level (for user-space registration)
 * This allows processes to self-declare their priority
 */
void custom_enqueue_task_with_priority(struct custom_rq *rq, struct task_struct *p, int forced_priority)
{
    struct custom_task_info *task_info;
    struct list_head *queue;
    unsigned long flags;
    
    // Check if task is already in queue
    spin_lock_irqsave(&rq->lock, flags);
    list_for_each_entry(task_info, &rq->high_priority_tasks, list) {
        if (task_info->task == p) {
            spin_unlock_irqrestore(&rq->lock, flags);
            printk(KERN_INFO "Scheduler: Task %s already in queue\n", p->comm);
            return;
        }
    }
    list_for_each_entry(task_info, &rq->medium_priority_tasks, list) {
        if (task_info->task == p) {
            spin_unlock_irqrestore(&rq->lock, flags);
            printk(KERN_INFO "Scheduler: Task %s already in queue\n", p->comm);
            return;
        }
    }
    list_for_each_entry(task_info, &rq->low_priority_tasks, list) {
        if (task_info->task == p) {
            spin_unlock_irqrestore(&rq->lock, flags);
            printk(KERN_INFO "Scheduler: Task %s already in queue\n", p->comm);
            return;
        }
    }
    spin_unlock_irqrestore(&rq->lock, flags);
    
    // Allocate memory
    task_info = kmalloc(sizeof(struct custom_task_info), GFP_ATOMIC);
    if (!task_info) {
        printk(KERN_ERR "Custom Scheduler: Failed to allocate task_info\n");
        return;
    }
    
    // Initialize task info
    task_info->task = p;
    task_info->priority = forced_priority;
    
    // Set deadline based on priority
    switch (forced_priority) {
        case PRIORITY_HIGH:
            task_info->deadline = jiffies + msecs_to_jiffies(100);
            break;
        case PRIORITY_MEDIUM:
            task_info->deadline = jiffies + msecs_to_jiffies(500);
            break;
        case PRIORITY_LOW:
        default:
            task_info->deadline = jiffies + msecs_to_jiffies(1000);
            break;
    }
    
    task_info->last_burst_actual = 0;
    task_info->burst_time = msecs_to_jiffies(10);
    task_info->enqueue_time = jiffies;
    task_info->remaining_time = task_info->burst_time;
    
    INIT_LIST_HEAD(&task_info->list);
    
    spin_lock_irqsave(&rq->lock, flags);
    
    switch (forced_priority) {
        case PRIORITY_HIGH:
            queue = &rq->high_priority_tasks;
            printk(KERN_INFO "Scheduler: Adding %s to HIGH queue (EDF)\n", p->comm);
            break;
        case PRIORITY_MEDIUM:
            queue = &rq->medium_priority_tasks;
            printk(KERN_INFO "Scheduler: Adding %s to MEDIUM queue (SJF)\n", p->comm);
            break;
        case PRIORITY_LOW:
        default:
            queue = &rq->low_priority_tasks;
            printk(KERN_INFO "Scheduler: Adding %s to LOW queue (FIFO)\n", p->comm);
            break;
    }
    
    list_add_tail(&task_info->list, queue);
    rq->nr_running++;
    
    spin_unlock_irqrestore(&rq->lock, flags);
    
    printk(KERN_INFO "Scheduler: Enqueued %s (PID: %d, Priority: %d) - Total tasks: %d\n", 
           p->comm, p->pid, forced_priority, rq->nr_running);
}
EXPORT_SYMBOL(custom_enqueue_task_with_priority);
