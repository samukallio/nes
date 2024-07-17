## Overview

A simple NES emulator written in C++ (or rather, C with some C++ features sprinkled on).

## Features

* Cycle-accurate CPU emulation, including dummy reads and double writes
* Supported INES mappers: 000 (NROM), 001 (MMC1), 002 (UxROM), 003 (CNROM), 004 (MMC3)

## Screenshots

<p align="center">
    <img src="https://samukallio.net/nes/montage.png">
</p>

## Tests

### CPU Tests

| Test                  | Author  | Description                                                | Status                                |
| --------------------- | ------- | ---------------------------------------------------------- | ------------------------------------- |
| `branch_timing_tests` | blargg  | Timing of the branch instruction, including edge cases     | :heavy_check_mark: Passing            |
| `cpu_dummy_reads`     | blargg  | Test addressing modes that have dummy reads                | :heavy_check_mark: Passing            |
| `cpu_dummy_writes`    | bisqwit | Verify double writes by read-modify-write instructions     | :heavy_check_mark: Passing            |
| `cpu_exec_space`      | bisqwit | Verify that CPU can execute code from any memory location  | :x: Failing                           |
| `cpu_interrupts_v2`   | blargg  | Behavior and timing of CPU interrupts (NMI and IRQ)        | :x: Failing                           |
| `cpu_timing_test6`    | blargg  | Instruction timing (official and unofficial)               | :heavy_check_mark: Passing (official) |
| `instr_misc`          | blargg  | Miscellaneous instruction test                             | :heavy_check_mark: Passing            |
| `instr_timing`        | blargg  | Instruction timing (official and unofficial)               | :heavy_check_mark: Passing (official) |
| `nestest`             | kevtris | Instruction behavior (official and unofficial)             | :heavy_check_mark: Passing (official) |

### PPU Tests

| Test                          | Author   | Description                                                | Status                     |
| ----------------------------- | -------- | ---------------------------------------------------------- | -------------------------- |
| `oam_read`                    | blargg   | Reading from OAMDATA returns data at OAMADDR               | :heavy_check_mark: Passing |
| `oam_stress`                  | blargg   | Thoroughly test OAMDATA and OAMADDR                        | :heavy_check_mark: Passing |
| `ppu_open_bus`                | blargg   | Test PPU open bus behavior                                 | :x: Failing                |
| `ppu_read_buffer`             | bisqwit  | PPU test suite mostly concentrating on the PPU read buffer | :heavy_check_mark: Passing |
| `ppu_vbl_nmi`                 | blargg   | Test PPU VBL flag behavior and timing                      | :x: Failing                |
| `scanline`                    | Quietust | Test PPU scanline timing accuracy                          | :x: Failing                |
| `sprite_hit_tests_2005.10.05` | blargg   | Sprite 0 hit behavior and timing                           | :heavy_check_mark: Passing |
| `sprite_overflow_tests`       | blargg   | Test sprite overflow behavior and timing                   | :x: Failing                |
