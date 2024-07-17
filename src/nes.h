#pragma once

#include <stdio.h>
#include <cstdint>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

/* --- CPU ----------------------------------------------------------------- */

struct cpu_instruction
{
	u8              Opcode;
	u8              Operation;
	u8              State;
	bool            XY;
};

enum cpu_interrupt
{
	NO_INTERRUPT = 0,
	NMI          = 1,
	IRQ          = 2,
};

struct cpu
{
	u64             Cycle;
	u64             Stall;

	bool            InternalIRQ;   // IRQ level detector output.
	bool            PreviousNMI;   // Previous NMI signal.
	bool            InternalNMI;   // NMI edge detector output.
	cpu_interrupt   Interrupt;     // Hardware interrupt to serve.

	cpu_instruction Instruction;   // Decoded instruction.
	u8              State;         // Operation state.
	u16             R0;            // Operation register 0.
	u16             R1;            // Operation register 1.
	u16             R2;            // Operation register 2.
	u16             R3;            // Operation register 3.
	u8              M;             // Operand register.

	u16             PC;            // Program counter.
	u8              SP;            // Stack pointer.
	u8              A;             // Accumulator.
	u8              X;             // Index X.
	u8              Y;             // Index Y.
	bool            CF;            // Carry flag.
	bool            ZF;            // Zero flag.
	bool            IF;            // Interrupt flag.
	bool            DF;            // Decimal flag (unused).
	bool            BF;            // Break flag.
	bool            VF;            // Overflow flag.
	bool            NF;            // Negative flag.

	bool            IRQ;
	bool            NMI;
};

/* --- PPU: Picture Processing Unit ---------------------------------------- */

const u32 PPUColorTable[64] =
{
	0xFF666666, 0xFF002A88, 0xFF1412A7, 0xFF3B00A4,
	0xFF5C007E, 0xFF6E0040, 0xFF6C0600, 0xFF561D00,
	0xFF333500, 0xFF0B4800, 0xFF005200, 0xFF004F08,
	0xFF00404D, 0xFF000000, 0xFF000000, 0xFF000000,
	0xFFADADAD, 0xFF155FD9, 0xFF4240FF, 0xFF7527FE,
	0xFFA01ACC, 0xFFB71E7B, 0xFFB53120, 0xFF994E00,
	0xFF6B6D00, 0xFF388700, 0xFF0C9300, 0xFF008F32,
	0xFF007C8D, 0xFF000000, 0xFF000000, 0xFF000000,
	0xFFFFFEFF, 0xFF64B0FF, 0xFF9290FF, 0xFFC676FF,
	0xFFF36AFF, 0xFFFE6ECC, 0xFFFE8170, 0xFFEA9E22,
	0xFFBCBE00, 0xFF88D800, 0xFF5CE430, 0xFF45E082,
	0xFF48CDDE, 0xFF4F4F4F, 0xFF000000, 0xFF000000,
	0xFFFFFEFF, 0xFFC0DFFF, 0xFFD3D2FF, 0xFFE8C8FF,
	0xFFFBC2FF, 0xFFFEC4EA, 0xFFFECCC5, 0xFFF7D8A5,
	0xFFE4E594, 0xFFCFEF96, 0xFFBDF4AB, 0xFFB3F3CC,
	0xFFB5EBF2, 0xFFB8B8B8, 0xFF000000, 0xFF000000,
};

struct ppu
{
	u64             MasterCycle;                // Current master clock cycle.

	u64             Frame;                      // Current frame.
	u32             ScanY;                      // Current scan line.
	u32             ScanX;                      // Current scan line cycle.
	bool            VerticalBlankFlag;          // Vertical blank active.
	bool            VerticalBlankFlagInhibit;   // Inhibit vblank flag for 1 PPU cycle.
	u64             VerticalBlankCount;

	u32*            FrameBuffer[2];             // Frame buffers (256x240, RGBA8).

	u16             V;                          // Current VRAM address.
	u16             T;                          // Scroll, Y & coarse X.
	u8              X;                          // Scroll, fine X.
	u8              W;                          // Register write H/L byte toggle.
	u8              VIncrementBy32;             // V increment mode.

	bool            NMIOutput;                  // Generate NMI during V-blank.
	bool            GrayscaleEnable;            // Select grayscale color table.
	u8              MasterSlaveSelect;          // PPU master/slave select.
	u8              TintMode;                   // R/G/B tint flags.
	u8              OAMAddress;                 // OAMADDR.
	u8              ReadBuffer;                 // PPUDATA read buffer.
	u8              BusData;                    // Open bus data.

	bool            BackgroundEnable;           // Enable background rendering.
	bool            BackgroundShowLeftMargin;   // Show background at X < 8.
	u8              BackgroundPatternTable;     // Active BG pattern table.

	u8              TilePatternIndex;           // Fetched tile pattern index (0-255).
	u8              TilePaletteIndex;           // Fetched tile palette index (0-3).
	u8              TilePatternL;               // Fetched color LSBs for tile row (8 pixels).
	u8              TilePatternH;               // Fetched color MSBs for tile row (8 pixels).
	u64             TileColorData;              // Output tile pixel FIFO (16 pixels, 4 bpp).

	bool            SpriteEnable;               // Enable sprite rendering.
	bool            SpriteShowLeftMargin;       // Show sprites at X < 8.
	u8              SpritePatternTable;         // Active sprite pattern table (8x8 sprites only).
	bool            Sprite8x16;                 // Use 8x16 sprites.

	bool            SpriteOverflow;             // Found more than 8 sprites on this scan line.
	bool            SpriteZeroHit;              // Sprite 0 collided with background.
	u8              SpriteCount;                // Number of sprites on this scan line.
	u8              SpriteIndex[8];             // OAM indices of sprites.
	u8              SpritePriority[8];          // Sprite BG priority bits.
	u8              SpriteX[8];                 // Sprite X coordinates.
	u8              SpriteY[8];                 // Sprite Y coordinates.
	u32             SpriteColorData[8];         // Sprite row color data (8 pixels, 4 bpp).

	u8              SpriteOAM[256];             // Sprite OAM memory (64 sprites).
	u8              Palette[32];                // Active palette (indexes PPU color table).
};

/* --- APU: Audio Processing Unit ------------------------------------------ */

// Duty cycle sequences for the pulse channels.
const u8 APUPulseSequenceTable[4][8] =
{
	{ 0, 1, 1, 0, 0, 0, 0, 0 },
	{ 0, 1, 1, 1, 0, 0, 0, 0 },
	{ 0, 1, 1, 1, 1, 0, 0, 0 },
	{ 1, 0, 0, 1, 1, 1, 1, 1 },
};

// Output waveform sequence for the triangle channel.
const u8 APUTriangleSequenceTable[32] =
{
	0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
	0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
};

// Length counter preset values.
const u8 APULengthTable[32] =
{
	0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06,
	0xA0, 0x08, 0x3C, 0x0A, 0x0E, 0x0C, 0x1A, 0x0E,
	0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16,
	0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E,
};

// Noise sequencer clock generator period table.
const u16 APUNoiseTimerPeriodTable[16] =
{
	0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0060, 0x0080, 0x00A0,
	0x00CA, 0x00FE, 0x017C, 0x01FC, 0x02FA, 0x03F8, 0x07F2, 0x0FE4,
};

// DMC output clock generator period table.
const u16 APUDMCTimerPeriodTable[16] =
{
	0x01AC, 0x017C, 0x0154, 0x0140, 0x011E, 0x00FE, 0x00E2, 0x00D6,
	0x00BE, 0x00A0, 0x008E, 0x0080, 0x006A, 0x0054, 0x0048, 0x0036,
};

struct apu_pulse
{
	bool            Enable;                     // Channel enable.

	u8              ConstantVolume;             // Constant volume when envelope disabled.
	bool            EnvelopeEnable;             // Envelope generator enable.
	bool            EnvelopeReset;              // Reset envelope on next cycle.
	bool            EnvelopeLoop;               // Restart envelope after it ends.
	u8              EnvelopeVolume;             // Current envelope level.
	u8              EnvelopeDividerPeriod;      // Envelope clocking divider period.
	u8              EnvelopeDividerCount;       // Envelope clocking divider count.

	bool            SweepEnable;                // Sweep unit enable.
	bool            SweepNegate;                // Negate sweep unit period change.
	u8              SweepShift;                 // Amount to shift period to obtain period delta.
	u8              SweepDividerPeriod;         // Sweep clocking divider period.
	u8              SweepDividerCount;          // Sweep clocking divider count.
	bool            SweepDividerReset;          // Reset divider count on next half-frame cycle.

	u16             TimerPeriod;                // Sequencer clock generator period.
	u16             Timer;                      // Sequencer clock generator timer.
	u8              SequenceMode;               // Pulse sequence selector.
	u8              SequenceTime;               // Pulse sequence time.
	bool            LengthEnable;               // Length counter decrement enable.
	u8              Length;                     // Length counter value.
};

struct apu_triangle
{
	bool            Enable;                     // Channel enable.

	u8              SequenceTime;               // Sequencer position.
	u16             TimerPeriod;                // Sequencer clock generator period.
	u16             Timer;                      // Sequencer clock generator timer.
	u8              Counter;                    // Linear counter value.
	u8              CounterPreset;              // Linear counter preset.
	bool            CounterLoad;                // If true, load linear counter at next cycle.
	bool            CounterLoadControl;         // If false, clear CounterLoad after loading.
	bool            LengthEnable;               // Length counter decrement enable.
	u8              Length;                     // Length counter value.
};

struct apu_noise
{
	bool            Enable;                     // Channel enable.

	u8              ConstantVolume;             // Constant volume when envelope disabled.
	bool            EnvelopeEnable;             // Envelope generator enable.
	bool            EnvelopeReset;              // Reset envelope on next cycle.
	bool            EnvelopeLoop;               // Restart envelope after it ends.
	u8              EnvelopeVolume;             // Current envelope level.
	u8              EnvelopeDividerPeriod;      // Envelope clocking divider period.
	u8              EnvelopeDividerCount;       // Envelope clocking divider count.

	u16             TimerPeriod;                // Sequencer clock generator period.
	u16             Timer;                      // Sequencer clock generator timer.
	u8              NoiseMode;                  // Noise register feedback mode.
    u16             NoiseRegister;              // Noise register state.
	bool            LengthEnable;               // Length counter decrement enable.
	u8              Length;                     // Length counter value.
};

struct apu_dmc
{
	bool            Enable;                     // Channel enable.
	bool            InterruptEnable;            // Enable DMC interrupt generation.
	bool            Interrupt;                  // Set if DMC interrupt raised.

	bool            SampleLoop;                 // Restart sample after ending.
	u16             SampleAddress;              // Sample memory address.
	u16             SampleLength;               // Sample length in bytes.
	u16             SampleTransferPointer;      // Sample DMA transfer pointer.
	u16             SampleTransferCounter;      // Sample DMA transfer remaining bytes.
	u8              SampleBuffer;               // Sample DMA buffer.
	bool            SampleBufferEmpty;          // True if sample DMA buffer is empty.

	u16             TimerPeriod;                // Output clock generator period.
	u16             Timer;                      // Output clock generator timer.
	u8              OutputRegister;             // Output shift register.
	u8              OutputTime;                 // Output shift count.
	bool            OutputEnable;               // Output sample on this cycle?
	u8              Output;                     // Current output value.
};

struct apu
{
	u64             Cycle;                      // Current global CPU cycle.
	u64             Frame;                      // Current APU frame.
	u16             FrameCycle;                 // Current cycle within APU frame.
	u8              FrameCycleResetTimer;       // Timer for FrameCycle reset after register write.
	u8              FrameCounterMode;           // Frame counter mode.
	u8              FrameInterruptDisable;      // Disable frame interrupt generation.
	bool            FrameInterrupt;             // Frame interrupt raised.
	u16             FrameInterruptCycle;        //

	apu_pulse       Pulse[2];                   // Pulse channels.
	apu_triangle    Triangle;                   // Triangle channel.
	apu_noise       Noise;                      // Noise channel.
	apu_dmc         DMC;                        // Delta-modulation channel.

	f64             AudioSampleRate;            // Output audio sample rate.
	f64             AudioSampleCount;

	i32             AudioPointer;               // Audio buffer write position.
	u64             AudioSampleCycle;
	f64             AudioSample;
	u8*             AudioBuffer;                // Audio buffer.

	f64             AudioSamplePrevious;
	f64             AudioSampleIntegrator;
};

/* --- Mappers ------------------------------------------------------------- */

struct mapper1
{
	u8              LoadCount;                  // Load register shift count.
	u8              LoadRegister;               // Load register value.
	u8              Control;                    // Bank switch mode.
	u8              CHRBank0;                   // Lower CHR ROM bank switch register.
	u8              CHRBank1;                   // Upper CHR ROM bank switch register.
	u8              PRGBank;                    // PRG ROM bank switch register.
	u32             PRGMap[2];                  // Maps CPU $8000-$FFFF to PRG ROM in 16K pages.
	u32             CHRMap[2];                  // Maps PPU $0000-$1FFF to CHR ROM in 4K pages.
};

struct mapper2
{
	u8              PRGBank;                    // CPU $8000-$BFFF bank select.
};

struct mapper3
{
	u16             CHRBank;                    // PPU $0000-$1FFF bank select.
};

struct mapper4
{
	u8              BankControl;                // Bank control register.
	u8              BankRegister[8];            // Bank select registers.
	u32             PRGMap[4];                  // Maps CPU $8000-$FFFF to PRG ROM in 8K pages.
	u32             CHRMap[8];                  // Maps PPU $0000-$1FFF to CHR ROM in 1K pages.
	bool            PRGRAMEnable;               // Allow PRG RAM access.
	bool            PRGRAMProtect;              // Disallow PRG RAM writes.
	bool            IRQEnable;                  // Generate IRQ when IRQCounter == 0.
	bool            IRQCounterLoad;             // Load IRQ counter on next scanline.
	u8              IRQCounterPreset;           // Load IRQ counter with this value.
	u8              IRQCounter;                 // Current IRQ counter value.
};


enum mapper_event
{
	PPUFilteredA12Edge,
};

using mapper_reset  = void (*)(struct machine*);
using mapper_read   = u8   (*)(struct machine*, u16);
using mapper_write  = void (*)(struct machine*, u16, u8);
using mapper_notify = void (*)(struct machine*, mapper_event);

struct mapper
{
	i32             ID;                         // INES mapper number.
	u8              MirrorMode;                 // CIRAM mirroring mode.

	u8              IRQTrigger;                 // Mapper IRQ.

	mapper_reset    Reset;
	mapper_read     Read;
	mapper_write    Write;
	mapper_notify   Notify;

	union
	{
		mapper1     _1;
		mapper2     _2;
		mapper3     _3;
		mapper4     _4;
	};
};

/* --- NES ----------------------------------------------------------------- */

enum button
{
	ButtonA         = 0x01,
	ButtonB         = 0x02,
	ButtonSelect    = 0x04,
	ButtonStart     = 0x08,
	ButtonUp        = 0x10,
	ButtonDown      = 0x20,
	ButtonLeft      = 0x40,
	ButtonRight     = 0x80,
};

struct machine
{
	u64             MasterCycle = 0;            // Current master clock cycle.

	bool            IsLoaded;                   // True if loaded with cartridge data.
	bool            Battery;
	bool            TraceEnable;
	FILE*           TraceFile;

	u8              Input[2];                   // Controller button states.
	bool            InputStrobe;                // Controller register strobe.
	u8              InputData[2];               // Controller register data.

	cpu             CPU;
	ppu             PPU;
	apu             APU;
	mapper          Mapper;

	u8*             RAM;                        // 2K system RAM.
	u8*             CIRAM;                      // 2K PPU internal RAM.
	u32             PRGROMSize;                 // PRG ROM size in bytes.
	u8*             PRGRAM;                     // PRG ROM.
	u32             PRGRAMSize;                 // PRG RAM size in bytes.
	u8*             PRGROM;                     // PRG RAM.
	u32             CHRSize;                    // CHR RAM/ROM size in bytes.
	u8*             CHR;                        // CHR RAM/ROM.
};

/* --- cpu.cpp -------------------------------------------------------------- */

void StepCPU(machine* M);
void StepCPUPhase2(machine* M);

/* --- ppu.cpp -------------------------------------------------------------- */

u8   ReadPPU(machine* M, u16 Address);
void WritePPU(machine* M, u16 Address, u8 Data);
void StepPPU(machine* M);

/* --- apu.cpp -------------------------------------------------------------- */

u8   ReadAPU(machine* M, u16 Address);
void WriteAPU(machine* M, u16 Address, u8 Data);
void StepAPU(machine* M);

/* --- mapper.cpp ----------------------------------------------------------- */

u8   ReadMapper0(machine* M, u16 Address);
void WriteMapper0(machine* M, u16 Address, u8 Data);

void ResetMapper1(machine* M);
u8   ReadMapper1(machine* M, u16 Address);
void WriteMapper1(machine* M, u16 Address, u8 Data);

u8   ReadMapper2(machine* M, u16 Address);
void WriteMapper2(machine* M, u16 Address, u8 Data);

u8   ReadMapper3(machine* M, u16 Address);
void WriteMapper3(machine* M, u16 Address, u8 Data);

void ResetMapper4(machine* M);
u8   ReadMapper4(machine* M, u16 Address);
void WriteMapper4(machine* M, u16 Address, u8 Data);
void NotifyMapper4(machine* M, mapper_event Event);

inline u8 ReadMapper(machine* M, u16 Address)
{
	return M->Mapper.Read(M, Address);
}

inline void WriteMapper(machine* M, u16 Address, u8 Data)
{
	M->Mapper.Write(M, Address, Data);
}

/* --- nes.cpp -------------------------------------------------------------- */

i32  Load(machine* M, const char* Path);
void Reset(machine* M);
void RunUntilVerticalBlank(machine* M);

u8   Read(machine* M, u16 Address);
void Write(machine* M, u16 Address, u8 Data);

inline u16 Read16(machine* M, u16 Address)
{
	u16 L = Read(M, Address);
	u16 H = Read(M, Address + 1);
	return (H << 8) | L;
}
