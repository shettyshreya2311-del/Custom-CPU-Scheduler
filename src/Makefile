# =============================================================================
# Custom Hybrid CPU Scheduler - Makefile
# =============================================================================

# Kernel Modules to build
# Main module (v4 - current version, single-file)
obj-m += custom_scheduler_v4.o

# Legacy module (multi-file, for reference)
obj-m += custom_scheduler_full.o
custom_scheduler_full-objs := custom_scheduler.o scheduler_helpers.o scheduler_picker.o

obj-m += process_manager.o

# Build Variables
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# =============================================================================
# Build Targets
# =============================================================================

# Default: build kernel modules only
all: modules
	@echo ""
	@echo "Build complete!"
	@echo "Run: sudo insmod custom_scheduler_v4.ko"

# Build kernel modules
modules:
	@echo "Building kernel modules..."
	make -C $(KDIR) M=$(PWD) modules

# =============================================================================
# Cleanup
# =============================================================================

clean:
	@echo "Cleaning build artifacts..."
	make -C $(KDIR) M=$(PWD) clean 2>/dev/null || true
	rm -f *.o *.ko *.mod.c *.mod *.order *.symvers .*.cmd
	rm -f Module.symvers modules.order
	@echo "Clean complete"

# =============================================================================
# Module Management (require sudo)
# =============================================================================

# Install v4 module (primary)
install:
	sudo insmod custom_scheduler_v4.ko
	@echo "✓ custom_scheduler_v4.ko loaded"
	@echo "  Run dashboard: python3 dashboard_v4.py"

# Remove v4 module
remove:
	sudo rmmod custom_scheduler_v4 2>/dev/null || true
	@echo "✓ Module unloaded"

# Reinstall v4 module
reinstall: remove install

# =============================================================================
# Dashboard
# =============================================================================

# Run the dashboard (module must be loaded first)
dashboard:
	@echo "Starting dashboard..."
	@echo "Open http://localhost:5000 in your browser"
	python3 dashboard_v4.py

# Full demo: load module and start dashboard
demo: install dashboard

# =============================================================================
# Status/Debug Commands
# =============================================================================

# Check if module is loaded
status:
	@echo "Loaded scheduler modules:"
	@lsmod | grep -E "custom|scheduler" || echo "  (none loaded)"
	@echo ""
	@if [ -f /proc/custom_scheduler ]; then \
		echo "/proc/custom_scheduler:"; \
		cat /proc/custom_scheduler | head -20; \
	else \
		echo "/proc/custom_scheduler not found (module not loaded)"; \
	fi

# Show recent kernel logs
logs:
	@echo "Recent kernel messages:"
	@dmesg | grep -i scheduler | tail -30

# =============================================================================
# Help
# =============================================================================

help:
	@echo "Custom Hybrid CPU Scheduler - Build System"
	@echo ""
	@echo "Build:"
	@echo "  make          - Build kernel modules"
	@echo "  make clean    - Remove build artifacts"
	@echo ""
	@echo "Run (require sudo):"
	@echo "  make install  - Load kernel module"
	@echo "  make remove   - Unload kernel module"
	@echo "  make reinstall- Reload module"
	@echo "  make dashboard- Start web dashboard"
	@echo "  make demo     - Load module + start dashboard"
	@echo ""
	@echo "Debug:"
	@echo "  make status   - Show module status"
	@echo "  make logs     - Show kernel logs"

.PHONY: all modules clean install remove reinstall dashboard demo status logs help
