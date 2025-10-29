// instruction.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>

enum class InstructionType { PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR };

struct Instruction {
  InstructionType type;
  std::vector<std::string> args; // raw args; parse into vars/values
  // For FOR: args[0] can be repeat count; nested instructions stored in nested
  // vector
  std::vector<Instruction> nested;
};
