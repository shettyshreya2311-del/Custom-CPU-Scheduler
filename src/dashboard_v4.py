"""
dashboard_v4.py - TRUE Kernel Integration Dashboard

This dashboard is a THIN CLIENT that:
- Writes user tasks to kernel via /proc/custom_scheduler
- Reads execution state from kernel
- Displays kernel's actual scheduling decisions
- NO Python scheduling logic - kernel controls everything!
"""

from flask import Flask, jsonify, render_template, request
import os
import json

app = Flask(__name__)

PROC_FILE = '/proc/custom_scheduler'

def kernel_module_loaded():
    """Check if kernel module is loaded"""
    return os.path.exists(PROC_FILE)

def write_to_kernel(command):
    """Write command to kernel via /proc interface"""
    if not kernel_module_loaded():
        return False, "Kernel module not loaded! Run: sudo insmod custom_scheduler_v4.ko"
    
    try:
        with open(PROC_FILE, 'w') as f:
            f.write(command)
        return True, "OK"
    except Exception as e:
        return False, str(e)

def read_from_kernel():
    """Read scheduler state from kernel"""
    if not kernel_module_loaded():
        return {
            "error": "Kernel module not loaded",
            "high_waiting": 0,
            "medium_waiting": 0,
            "low_waiting": 0,
            "total": 0,
            "running": 0,
            "completed": 0,
            "current_task": None,
            "current_priority": -1,
            "elapsed_ms": 0,
            "tasks": [],
            "history": []
        }
    
    try:
        with open(PROC_FILE, 'r') as f:
            data = json.load(f)
        return data
    except Exception as e:
        return {"error": str(e)}

# ============================================================================
# ROUTES
# ============================================================================

@app.route('/')
def home():
    return render_template('dashboard_v4.html')

@app.route('/api/status')
def get_status():
    """Check if kernel module is loaded"""
    return jsonify({
        "kernel_loaded": kernel_module_loaded(),
        "proc_file": PROC_FILE
    })

@app.route('/api/add_task', methods=['POST'])
def add_task():
    """
    Add a task - writes directly to kernel!
    The kernel will create and schedule this task.
    """
    data = request.json
    task_name = data.get('name', 'P1')
    priority = int(data.get('priority', 2))
    burst_time = int(data.get('burst_time', 3)) * 1000  # Convert to milliseconds
    
    # Write to kernel
    command = f"create_task {task_name} {priority} {burst_time}"
    success, message = write_to_kernel(command)
    
    if success:
        return jsonify({
            "status": "ok",
            "message": f"Task {task_name} sent to kernel (priority={priority}, burst={burst_time}ms)"
        })
    else:
        return jsonify({
            "status": "error",
            "message": message
        }), 500

@app.route('/api/start', methods=['POST'])
def start_execution():
    """Tell kernel to start executing tasks"""
    success, message = write_to_kernel("start")
    
    if success:
        return jsonify({"status": "ok", "message": "Kernel started task execution"})
    else:
        return jsonify({"status": "error", "message": message}), 500

@app.route('/api/reset', methods=['POST'])
def reset():
    """Tell kernel to reset all tasks"""
    success, message = write_to_kernel("reset")
    
    if success:
        return jsonify({"status": "ok", "message": "Kernel scheduler reset"})
    else:
        return jsonify({"status": "error", "message": message}), 500

@app.route('/api/stats')
def get_stats():
    """
    Read scheduler state from kernel.
    This returns the ACTUAL kernel state, not a simulation!
    """
    kernel_data = read_from_kernel()
    return jsonify(kernel_data)

# ============================================================================
# MAIN
# ============================================================================

if __name__ == '__main__':
    print("=" * 70)
    print("  Custom Scheduler Dashboard v4.0 - TRUE KERNEL INTEGRATION")
    print("=" * 70)
    print("")
    
    if kernel_module_loaded():
        print("  ✅ Kernel module is LOADED")
        print(f"  ✅ Reading from {PROC_FILE}")
    else:
        print("  ❌ Kernel module NOT loaded!")
        print("")
        print("  To load the kernel module, run:")
        print("    cd src/")
        print("    make")
        print("    sudo insmod custom_scheduler_v4.ko")
        print("")
    
    print("")
    print("  Dashboard: http://127.0.0.1:5000")
    print("")
    print("  How it works:")
    print("    1. User adds task → Dashboard writes to /proc/custom_scheduler")
    print("    2. Kernel creates task and puts in priority queue")
    print("    3. User clicks Start → Kernel begins executing tasks")
    print("    4. Kernel scheduler picks next task (EDF/SJF/FIFO)")
    print("    5. Dashboard reads kernel state and displays Gantt chart")
    print("")
    print("  All scheduling decisions are made by the KERNEL!")
    print("=" * 70)
    
    app.run(debug=False, port=5000, threaded=True)
