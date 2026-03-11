/*
 * scheduler_picker.c - Task selection logic
 * Updated to record decisions
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/list.h>
#include "../include/custom_sched.h"

// Reference the global variable from custom_scheduler.c
extern int last_scheduled_prio;

static struct custom_task_info *find_earliest_deadline(struct list_head *queue)
{
    struct custom_task_info *task_info, *earliest = NULL;
    if (list_empty(queue)) return NULL;
    
    list_for_each_entry(task_info, queue, list) {
        if (!earliest || time_before(task_info->deadline, earliest->deadline)) {
            earliest = task_info;
        }
    }
    return earliest;
}

static struct custom_task_info *find_shortest_job(struct list_head *queue)
{
    struct custom_task_info *task_info, *shortest = NULL;
    if (list_empty(queue)) return NULL;
    
    list_for_each_entry(task_info, queue, list) {
        if (!shortest || task_info->burst_time < shortest->burst_time) {
            shortest = task_info;
        }
    }
    return shortest;
}

struct custom_task_info *custom_pick_next_task(struct custom_rq *rq)
{
    struct custom_task_info *next_task = NULL;
    unsigned long flags;
    
    spin_lock_irqsave(&rq->lock, flags);
    
    // Strategy 1: HIGH (EDF)
    if (!list_empty(&rq->high_priority_tasks)) {
        next_task = find_earliest_deadline(&rq->high_priority_tasks);
        if (next_task) {
            last_scheduled_prio = PRIORITY_HIGH; // <--- RECORD HIGH
        }
    }
    // Strategy 2: MEDIUM (SJF)
    else if (!list_empty(&rq->medium_priority_tasks)) {
        next_task = find_shortest_job(&rq->medium_priority_tasks);
        if (next_task) {
            last_scheduled_prio = PRIORITY_MEDIUM; // <--- RECORD MEDIUM
        }
    }
    // Strategy 3: LOW (FIFO)
    else if (!list_empty(&rq->low_priority_tasks)) {
        next_task = list_first_entry(&rq->low_priority_tasks, struct custom_task_info, list);
        if (next_task) {
            last_scheduled_prio = PRIORITY_LOW; // <--- RECORD LOW
        }
    }
    // No tasks
    else {
        last_scheduled_prio = -1; // IDLE
    }
    
    spin_unlock_irqrestore(&rq->lock, flags);
    return next_task;
}
EXPORT_SYMBOL(custom_pick_next_task);
