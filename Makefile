# ===============================
# Compiler and flags
# ===============================
CXX := g++
CXXFLAGS := -std=c++20 -Wall -pthread -O2
INCLUDE := -I include

# ===============================
# Optional Debug / Sanitizer flags
# ===============================
DEBUG_FLAGS := -g -O0
ASAN_FLAGS := -fsanitize=address -fno-omit-frame-pointer

# Directories
SRC_DIR := src
BUILD_DIR := build
TARGET := $(BUILD_DIR)/app

# Sources and objects
SRC := $(shell find $(SRC_DIR) -name '*.cpp')
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRC))

# Test configuration (can override with TEST=name)
TEST ?= scheduler
TEST_SRC := tests/test_$(TEST).cpp
TEST_BIN := $(BUILD_DIR)/$(TEST)

.PHONY: all run test clean rebuild debug asan gdb

# ===============================
# Normal optimized build
# ===============================
all: $(TARGET)

$(TARGET): $(OBJ) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# $(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
# 	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@



# ===============================
# Testing target
# ===============================
test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE) \
		$(TEST_SRC) $(filter-out $(SRC_DIR)/main.cpp,$(wildcard $(SRC_DIR)/*.cpp)) \
		-o $@

# ===============================
# Debug build (adds -g -O0)
# ===============================
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: rebuild

# ===============================
# AddressSanitizer build
# ===============================
asan: CXXFLAGS += $(DEBUG_FLAGS) $(ASAN_FLAGS)
asan: rebuild

# ===============================
# GDB target (build + run in gdb)
# ===============================
gdb: CXXFLAGS += $(DEBUG_FLAGS)
gdb: $(TEST_BIN)
	gdb ./$(TEST_BIN)

# ===============================
# Utility targets
# ===============================
run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean all
