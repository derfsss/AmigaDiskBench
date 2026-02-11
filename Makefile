CC = ppc-amigaos-gcc
CXX = ppc-amigaos-g++
CFLAGS = -O2 -Wall -I./include
LDFLAGS = -lauto

BUILD_DIR = build
DIST_DIR = dist
TARGET = $(BUILD_DIR)/AmigaDiskBench
SRC = src/main.c src/engine.c src/engine_info.c src/engine_tests.c src/engine_persistence.c src/engine_workloads.c \
      src/engine_system.c \
      src/workloads/workload_legacy_sprinter.c src/workloads/workload_legacy_heavy.c \
      src/workloads/workload_legacy_legacy.c src/workloads/workload_legacy_grind.c \
      src/workloads/workload_sequential.c src/workloads/workload_random_4k.c \
      src/workloads/workload_profiler.c \
      src/gui.c src/gui_details_window.c src/gui_utils.c src/gui_prefs.c src/gui_history.c src/gui_system.c src/gui_worker.c src/gui_report.c src/gui_layout.c src/gui_events.c src/gui_viz.c src/gui_bulk.c src/gui_export.c
OBJ = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(SRC))

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

install: all
	mkdir -p $(DIST_DIR)
	cp $(TARGET) $(DIST_DIR)/AmigaDiskBench
	cp AmigaDiskBench.info $(DIST_DIR)/AmigaDiskBench.info
	@echo "Installation complete to $(DIST_DIR)/"

clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR)
