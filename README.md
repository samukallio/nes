## Overview

A simple NES emulator written in C++ (or rather, C with some C++ features sprinkled on).

## Features

* Cycle-accurate CPU emulation, including dummy reads and double writes
* Supported INES mappers: 000 (NROM), 001 (MMC1), 002 (UxROM), 003 (CNROM), 004 (MMC3)

## Screenshots

<p align="center">
    <img src="https://samukallio.net/nes/screenshots.png">
</p>

## Tests

### CPU Tests

| Pass               | Test                  | Author  | Description                                                |
| :----------------: | --------------------- | ------- | ---------------------------------------------------------- |
| :heavy_check_mark: | `branch_timing_tests` | blargg  | Timing of the branch instruction, including edge cases     |
| :heavy_check_mark: | `cpu_dummy_reads`     | blargg  | Test addressing modes that have dummy reads                |
| :heavy_check_mark: | `cpu_dummy_writes`    | bisqwit | Verify double writes by read-modify-write instructions     |
| :x:                | `cpu_exec_space`      | bisqwit | Verify that CPU can execute code from any memory location  |
| :x:                | `cpu_interrupts_v2`   | blargg  | Behavior and timing of CPU interrupts (NMI and IRQ)        |
| :heavy_check_mark: | `cpu_timing_test6`    | blargg  | Instruction timing (official and unofficial)               |
| :heavy_check_mark: | `instr_misc`          | blargg  | Miscellaneous instruction test                             |
| :heavy_check_mark: | `instr_timing`        | blargg  | Instruction timing (official and unofficial)               |
| :heavy_check_mark: | `nestest`             | kevtris | Instruction behavior (official and unofficial)             |

### PPU Tests

| Pass               | Test                          | Author   | Description                                                |
| :----------------: | ----------------------------- | -------- | ---------------------------------------------------------- |
| :heavy_check_mark: | `oam_read`                    | blargg   | Reading from OAMDATA returns data at OAMADDR               |
| :heavy_check_mark: | `oam_stress`                  | blargg   | Thoroughly test OAMDATA and OAMADDR                        |
| :x:                | `ppu_open_bus`                | blargg   | Test PPU open bus behavior                                 |
| :heavy_check_mark: | `ppu_read_buffer`             | bisqwit  | PPU test suite mostly concentrating on the PPU read buffer |
| :x:                | `ppu_vbl_nmi`                 | blargg   | Test PPU VBL flag behavior and timing                      |
| :x:                | `scanline`                    | Quietust | Test PPU scanline timing accuracy                          |
| :heavy_check_mark: | `sprite_hit_tests_2005.10.05` | blargg   | Sprite 0 hit behavior and timing                           |
| :x:                | `sprite_overflow_tests`       | blargg   | Test sprite overflow behavior and timing                   |
