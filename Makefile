# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++20 -Wall -pthread -O2
INCLUDE := -I include

# Directories
SRC_DIR := src
BUILD_DIR := build
TARGET := $(BUILD_DIR)/app

# Sources and objects
SRC := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRC))

# Test configuration (can override with TEST=name)
TEST ?= test_process
TEST_SRC := tests/$(TEST).cpp
TEST_BIN := $(BUILD_DIR)/$(TEST)

.PHONY: all run test clean rebuild

all: $(TARGET)

# Main app
$(TARGET): $(OBJ) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^

# Object build rule
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $@

# Test build and run
test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE) \
		$(TEST_SRC) $(filter-out $(SRC_DIR)/main.cpp,$(wildcard $(SRC_DIR)/*.cpp)) \
		-o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean all
