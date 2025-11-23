#pragma once
#include <cstdint>
#include <string>
#include <vector>

enum class InstructionType { PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR };

// Represents a single instruction in the process's instruction set
struct Instruction {
  InstructionType type;
  std::vector<std::string> args; // raw args; parse into vars/values
  // For FOR: args[0] can be repeat count; nested instructions stored in nested
  // vector
  std::vector<Instruction> nested;
};

// Maximum allowed FOR nesting depth (language/runtime spec).
// Use this constant across generator and process unrolling to ensure
// consistent behavior and to avoid hardcoded duplicated values.
constexpr int FOR_MAX_NESTING = 3;
