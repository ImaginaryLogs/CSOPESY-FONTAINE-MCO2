CXX := g++
CXXFLAGS := -std=c++17 -Wall -pthread -O2

SRC_DIR := src
BUILD_DIR := build
BIN_DIR := bin
TARGET := $(BIN_DIR)/csopesy.exe

SRC := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRC))

.PHONY: all run clean rebuild

all: $(TARGET)

$(TARGET): $(OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

# compile .cpp -> .o in build/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# alternate rule to allow producing .obj in build/ if desired
$(BUILD_DIR)/%.obj: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ensure dirs exist
$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR)/* $(BIN_DIR)/*

rebuild: clean all
