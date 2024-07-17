# Overview

A simple NES emulator written in C++ (or rather, C with some C++ features sprinkled on).

# Tests

### CPU Tests

| Test                  | Author  | Description                                                | Status                     |
| :-------------------- | :------ | :--------------------------------------------------------- | -------------------------- |
| `branch_timing_tests` | blargg  | Timing of the branch instruction, including edge cases     | :heavy_check_mark: Passing |
| `cpu_dummy_reads`     | blargg  | Test addressing modes that have dummy reads                | :heavy_check_mark: Passing |
| `cpu_dummy_writes`    | bisqwit | Verify double writes by read-modify-write instructions     | :heavy_check_mark: Passing |
| `cpu_exec_space`      | bisqwit | Verify that CPU can execute code from any memory location  | :x: Failing                |
| `cpu_interrupts_v2`   | blargg  | Behavior and timing of CPU interrupts (NMI and IRQ)        | :x: Failing (test 2 of 5)  |
| `cpu_timing_test6`    | blargg  | Instruction timing (official and unofficial)               | :heavy_check_mark: Passing |
| `instr_misc`          | blargg  | Miscellaneous instruction test (official and unofficial)   | :heavy_check_mark: Passing |
| `instr_timing`        | blargg  | Instruction timing (official and unofficial)               | :x: Failing (unofficial)   |
