/*
 * custom_sched.h - Custom Scheduler Header
 * Updated for Real-Time Decision Visualization
 */

#ifndef CUSTOM_SCHED_H
#define CUSTOM_SCHED_H

#include <linux/sched.h>
#include <linux/spinlock.h> 

// Priority levels
#define HIGH_PRIORITY_MIN    0
#define HIGH_PRIORITY_MAX    33
#define MEDIUM_PRIORITY_MIN  34
#define MEDIUM_PRIORITY_MAX  66
#define LOW_PRIORITY_MIN     67
#define LOW_PRIORITY_MAX     100

// Priority classifications
#define PRIORITY_HIGH    0
#define PRIORITY_MEDIUM  1
#define PRIORITY_LOW     2

// --- NEW: Global variable to track the scheduler's last decision ---
// 0=High, 1=Med, 2=Low, -1=Idle
extern int last_scheduled_prio; 

// Task information structure
struct custom_task_info {
    struct task_struct *task;       // Pointer to Linux task
    int priority;                   // Task priority (0-100)
    unsigned long deadline;         // Deadline in jiffies
    
    /* --- Dynamic Burst Time Fields (EMA) --- */
    unsigned long burst_time;       // Predicted execution time (EMA)
    unsigned long last_burst_actual;// Actual execution time from previous run
    unsigned long enqueue_time;     // Time when task entered queue
    /* --------------------------------------- */
    
    unsigned long remaining_time;   // Time left to complete
    struct list_head list;          // For linked list
};

// Scheduler run queue
struct custom_rq {
    struct list_head high_priority_tasks;
    struct list_head medium_priority_tasks;
    struct list_head low_priority_tasks;
    spinlock_t lock;                // Protect queue from race conditions
    unsigned int nr_running;        // Number of tasks in queue
};

// Function declarations
void custom_enqueue_task(struct custom_rq *rq, struct task_struct *p);
void custom_dequeue_task(struct custom_rq *rq, struct task_struct *p);
struct custom_task_info *custom_pick_next_task(struct custom_rq *rq);
void custom_enqueue_task_with_priority(struct custom_rq *rq, struct task_struct *p, int forced_priority);

#endif /* CUSTOM_SCHED_H */
