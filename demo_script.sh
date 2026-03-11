#!/bin/bash
# =============================================================================
# Custom Hybrid CPU Scheduler - Presentation Demo Script
# =============================================================================

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
WHITE='\033[1;37m'
NC='\033[0m'

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Clear screen and show header
clear
echo ""
echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                                                              ║${NC}"
echo -e "${CYAN}║   ${WHITE}🖥️  CUSTOM HYBRID CPU SCHEDULER${CYAN}                          ║${NC}"
echo -e "${CYAN}║   ${WHITE}   Linux Kernel Module Implementation${CYAN}                   ║${NC}"
echo -e "${CYAN}║                                                              ║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${YELLOW}Scheduling Algorithm:${NC}"
echo -e "  ${RED}●${NC} HIGH priority   → EDF (Earliest Deadline First)"
echo -e "  ${YELLOW}●${NC} MEDIUM priority → SJF (Shortest Job First)"
echo -e "  ${GREEN}●${NC} LOW priority    → FIFO (First Come First Serve)"
echo ""
read -p "Press Enter to start the demo..."

# =============================================================================
# Step 1: Compile the Project
# =============================================================================
clear
echo ""
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo -e "${WHITE}  STEP 1: Compiling the Kernel Module${NC}"
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo ""

cd "$SCRIPT_DIR/src"

echo -e "${BLUE}Cleaning previous build...${NC}"
make clean > /dev/null 2>&1
echo -e "${GREEN}✓ Clean complete${NC}"
echo ""

echo -e "${BLUE}Building kernel modules and tools...${NC}"
echo ""
make all 2>&1
BUILD_STATUS=$?
echo ""

if [[ $BUILD_STATUS -eq 0 ]]; then
    echo -e "${GREEN}✓ Build successful!${NC}"
else
    echo -e "${RED}✗ Build failed! Check errors above.${NC}"
    exit 1
fi

echo ""
read -p "Press Enter to continue..."

# =============================================================================
# Step 2: Show Project Structure
# =============================================================================
clear
echo ""
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo -e "${WHITE}  STEP 2: Project Structure${NC}"
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo ""

cd "$SCRIPT_DIR"

# Check if tree command is available
if command -v tree &> /dev/null; then
    tree -L 2 --noreport -I '__pycache__|*.pyc|.git'
else
    echo -e "${YELLOW}(tree command not available, using ls)${NC}"
    echo ""
    echo "custom-scheduler/"
    echo "├── demo_script.sh"
    echo "├── README.md"
    echo "├── QUICKSTART.md"
    echo "├── docs/"
    ls -1 docs/ 2>/dev/null | sed 's/^/│   └── /'
    echo "├── include/"
    ls -1 include/ 2>/dev/null | sed 's/^/│   └── /'
    echo "└── src/"
    ls -1 src/ 2>/dev/null | grep -v templates | sed 's/^/    ├── /'
    echo "    └── templates/"
    ls -1 src/templates/ 2>/dev/null | sed 's/^/        └── /'
fi

echo ""
read -p "Press Enter to continue..."

# =============================================================================
# Step 3: Show Compiled Modules
# =============================================================================
#clear
echo ""
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo -e "${WHITE}  STEP 3: Compiled Kernel Modules${NC}"
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo ""

cd "$SCRIPT_DIR/src"

echo -e "${BLUE}Kernel Modules (.ko files):${NC}"
echo ""
ls -lh *.ko 2>/dev/null || echo "No .ko files found yet"
echo ""

echo -e "${BLUE}Main module info:${NC}"
if [[ -f custom_scheduler_v4.ko ]]; then
    modinfo custom_scheduler_v4.ko 2>/dev/null | head -10
fi

echo ""
read -p "Press Enter to continue..."

# =============================================================================
# Step 4: Check Root Permissions
# =============================================================================
#clear
echo ""
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo -e "${WHITE}  STEP 4: Loading Kernel Module${NC}"
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo ""

if [[ $EUID -ne 0 ]]; then
    #echo -e "${RED}⚠️  Root privileges required to load kernel modules!${NC}"
    echo ""
    echo "Please run this script with sudo:"
    echo -e "${YELLOW}  sudo ./demo_script.sh${NC}"
    echo ""
    echo "Alternatively, you can manually load the module:"
    echo -e "${YELLOW}  cd src${NC}"
    echo -e "${YELLOW}  sudo insmod custom_scheduler_v4.ko${NC}"
    echo ""
    read -p "Press Enter to exit..."
    exit 1
fi

# Unload any existing modules
echo -e "${BLUE}Unloading any existing scheduler modules...${NC}"
rmmod custom_scheduler_v4 2>/dev/null
rmmod custom_scheduler_full 2>/dev/null
rmmod process_manager 2>/dev/null
echo -e "${GREEN}✓ Cleanup complete${NC}"
echo ""

# Clear kernel log buffer
echo -e "${BLUE}Clearing kernel log buffer...${NC}"
dmesg -C
echo -e "${GREEN}✓ Kernel logs cleared${NC}"
echo ""

# Load the module
echo -e "${BLUE}Loading custom_scheduler_v4.ko...${NC}"
cd "$SCRIPT_DIR/src"
insmod custom_scheduler_v4.ko

if [[ $? -eq 0 ]]; then
    echo -e "${GREEN}✓ Kernel module loaded successfully!${NC}"
else
    echo -e "${RED}✗ Failed to load module!${NC}"
    echo ""
    echo "Check dmesg for errors:"
    dmesg | tail -10
    exit 1
fi

echo ""
read -p "Press Enter to continue..."

# =============================================================================
# Step 5: Verify Module Loaded
# =============================================================================
#clear
echo ""
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo -e "${WHITE}  STEP 5: Verify Module Loaded${NC}"
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo ""

echo -e "${BLUE}Loaded kernel modules:${NC}"
lsmod | grep -E "custom|scheduler" | head -5
echo ""

echo -e "${BLUE}Checking /proc interface:${NC}"
if [[ -f /proc/custom_scheduler ]]; then
    echo -e "${GREEN}✓ /proc/custom_scheduler exists${NC}"
    echo ""
    echo -e "${CYAN}Initial scheduler state:${NC}"
    echo "────────────────────────────────────────"
    cat /proc/custom_scheduler | head -15
    echo "────────────────────────────────────────"
else
    echo -e "${RED}✗ /proc/custom_scheduler not found!${NC}"
fi

echo ""
read -p "Press Enter to continue..."

# =============================================================================
# Step 6: Show Kernel Logs
# =============================================================================
#clear
echo ""
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo -e "${WHITE}  STEP 6: Kernel Log Output${NC}"
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo ""

echo -e "${BLUE}Recent kernel messages:${NC}"
echo "────────────────────────────────────────"
dmesg | tail -20
echo "────────────────────────────────────────"

echo ""
read -p "Press Enter to continue..."

# =============================================================================
# Step 7: Launch Dashboard
# =============================================================================
#clear
echo ""
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo -e "${WHITE}  STEP 7: Web Dashboard${NC}"
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo ""

echo -e "${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║              🚀 READY TO LAUNCH DASHBOARD! 🚀                ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${WHITE}Open a NEW TERMINAL and run:${NC}"
echo ""
echo -e "    ${YELLOW}python3 src/dashboard_v4.py${NC}"
echo ""
echo -e "${WHITE}Then open this URL in your browser:${NC}"
echo ""
echo -e "    ${MAGENTA}👉 http://localhost:5000${NC}"
echo ""
echo -e "${CYAN}────────────────────────────────────────────────────────────────${NC}"
echo ""
echo -e "${WHITE}Dashboard Features:${NC}"
echo "  • Add tasks with custom priority and burst time"
echo "  • Real-time Gantt chart visualization"
echo "  • Performance metrics (turnaround, waiting time)"
echo "  • CFS comparison graph"
echo ""

read -p "Press Enter to start watching kernel logs..."

# =============================================================================
# Step 8: Watch Kernel Logs
# =============================================================================
#clear
echo ""
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo -e "${WHITE}  STEP 8: Live Kernel Logs${NC}"
echo -e "${CYAN}══════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "${YELLOW}Watching for scheduler events... (Press Ctrl+C to stop)${NC}"
echo ""
echo -e "${CYAN}────────────────────────────────────────────────────────────────${NC}"

# Cleanup function
cleanup() {
    echo ""
    echo -e "${CYAN}════════════════════════════════════════════════════════════════${NC}"
    echo -e "${WHITE}  CLEANUP${NC}"
    echo -e "${CYAN}════════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo -e "${BLUE}Unloading kernel module...${NC}"
    rmmod custom_scheduler_v4 2>/dev/null
    echo -e "${GREEN}✓ Module unloaded${NC}"
    echo ""
    echo -e "${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                   Demo Complete! 🎉                          ║${NC}"
    echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    exit 0
}

# Set trap for Ctrl+C
trap cleanup SIGINT SIGTERM

# Watch kernel logs with color coding
dmesg -w | grep --line-buffered "Scheduler" | while read line; do
    if [[ $line == *"EXECUTING"* ]]; then
        echo -e "${MAGENTA}$line${NC}"
    elif [[ $line == *"COMPLETED"* ]]; then
        echo -e "${GREEN}$line${NC}"
    elif [[ $line == *"[HIGH]"* ]] || [[ $line == *"HIGH queue"* ]]; then
        echo -e "${RED}$line${NC}"
    elif [[ $line == *"[MEDIUM]"* ]] || [[ $line == *"MEDIUM queue"* ]]; then
        echo -e "${YELLOW}$line${NC}"
    elif [[ $line == *"[LOW]"* ]] || [[ $line == *"LOW queue"* ]]; then
        echo -e "${GREEN}$line${NC}"
    elif [[ $line == *"Created"* ]] || [[ $line == *"Enqueuing"* ]]; then
        echo -e "${CYAN}$line${NC}"
    elif [[ $line == *"Started"* ]] || [[ $line == *"STARTED"* ]]; then
        echo -e "${WHITE}$line${NC}"
    elif [[ $line == *"Error"* ]] || [[ $line == *"Failed"* ]]; then
        echo -e "${RED}$line${NC}"
    else
        echo "$line"
    fi
done
