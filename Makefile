CXX := g++
CXXFLAGS := -std=c++17 -Wall -pthread -O2
INCLUDE := -I include

SRC_DIR := src
BUILD_DIR := build
TARGET := $(BUILD_DIR)/csopesy

SRC := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRC))

TEST_SRC := $(wildcard tests/*.cpp)
TEST_BIN := $(BUILD_DIR)/test_generator

.PHONY: all test run clean rebuild

all: $(TARGET)

$(TARGET): $(OBJ) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^

# compile .cpp -> .o in build/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# ensure build dir exists
$(BUILD_DIR):
	mkdir -p $@

test: $(TEST_BIN)
	$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(OBJ) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^

run: $(TARGET)
	$(TARGET)

clean:
	rm -rf $(BUILD_DIR)/*

rebuild: clean all
