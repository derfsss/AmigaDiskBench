CC = ppc-amigaos-gcc
CXX = ppc-amigaos-g++
CFLAGS = -O2 -Wall -I./include
LDFLAGS = -lauto

BUILD_DIR = build
DIST_DIR = dist
TARGET = $(BUILD_DIR)/AmigaDiskBench
SRC = src/main.c src/engine.c src/gui.c src/gui_details_window.c
OBJ = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(SRC))

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

install: all
	mkdir -p $(DIST_DIR)
	cp $(TARGET) $(DIST_DIR)/AmigaDiskBench
	cp tool.info $(DIST_DIR)/AmigaDiskBench.info
	@echo "Installation complete to $(DIST_DIR)/"

clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR)
