#define _CRT_SECURE_NO_WARNINGS

#include <assert.h>
#include <stdlib.h>
#include <memory.h>

#include "nes.h"

struct mapper_entry
{
	i32             MapperID;
	mapper_read     Read;
	mapper_write    Write;
	mapper_reset    Reset;
	mapper_notify   Notify;
};

const mapper_entry MapperTable[] =
{
	{ 0, ReadMapper0, WriteMapper0 },
	{ 1, ReadMapper1, WriteMapper1, ResetMapper1 },
	{ 2, ReadMapper2, WriteMapper2 },
	{ 3, ReadMapper3, WriteMapper3 },
	{ 4, ReadMapper4, WriteMapper4, ResetMapper4, NotifyMapper4 },
	{ -1 }
};

static const mapper_entry* FindMapperEntry(i32 MapperID)
{
	for (const mapper_entry* E = MapperTable; E->MapperID >= 0; E += 1)
		if (E->MapperID == MapperID)
			return E;
	return nullptr;
}

void Reset(machine& Machine)
{
	if (Machine.Mapper.Reset) Machine.Mapper.Reset(Machine);

	Machine.PPU.BackgroundPatternTable = 0;
	Machine.PPU.SpritePatternTable = 0;
	Machine.PPU.Sprite8x16 = false;
	Machine.PPU.VIncrementBy32 = false;
	Machine.PPU.MasterSlaveSelect = false;
	Machine.PPU.NMIOutput = false;

	Machine.PPU.OAMAddress = 0;

	Machine.PPU.V = 0;
	Machine.PPU.T = 0;
	Machine.PPU.X = 0;
	Machine.PPU.W = 0;

	Machine.PPU.Frame = 0;
	Machine.PPU.ScanY = 261;
	Machine.PPU.ScanX = 0;

	Machine.APU.AudioPointer = 0;

	Machine.APU.Noise.NoiseRegister = 0x0001;

	Machine.CPU.State = 0;
}

static inline u8 ReadController(machine& Machine, i32 Index)
{
	u8 Bit = Machine.InputData[Index] & 0x01;
	Machine.InputData[Index] = 0x80 | Machine.InputData[Index] >> 1;
	return Bit;
}

u8 Read(machine& Machine, u16 Address)
{
	// $0000-$1FFF: SRAM space.
	if (Address < 0x2000) {
		return Machine.BusData = Machine.RAM[Address & 0x07FF];
	}

	// $2000-$3FFF: PPU register space.
	if (Address < 0x4000) {
		return Machine.BusData = ReadPPU(Machine, Address & 0x2007);
	}

	// $4000-$401F: CPU register space.
	if (Address < 0x4020) {
		if (Address == 0x4015) Machine.BusData = ReadAPU(Machine, Address);
		if (Address == 0x4016) Machine.BusData = (Machine.BusData & 0xE0) | ReadController(Machine, 0);
		if (Address == 0x4017) Machine.BusData = (Machine.BusData & 0xE0) | ReadController(Machine, 1);
		return Machine.BusData;
	}

	// $4020-$FFFF: Cartridge space.
	return Machine.BusData = ReadMapper(Machine, Address);
}

void Write(machine& Machine, u16 Address, u8 Data)
{
	Machine.BusData = Data;

	// $0000-$1FFF: SRAM space.
	if (Address < 0x2000) {
		Machine.RAM[Address & 0x07FF] = Data;
		return;
	}

	// $2000-$3FFF: PPU register space.
	if (Address < 0x4000) {
		WritePPU(Machine, Address & 0x2007, Data);
		return;
	}

	// $4000-$401F: CPU register space.
	if (Address < 0x4020) {
		// OAM DMA.
		if (Address == 0x4014) {
			u16 Address = u16(Data) << 8;
			for (u32 I = 0; I < 256; I++)
				WritePPU(Machine, 0x2004, Read(Machine, Address + I));
			Machine.CPU.Stall += 513 + (Machine.CPU.Cycle % 2);
			return;
		}
		if (Address == 0x4016) {
			Machine.InputStrobe = Data & 0x01;
			return;
		}
		WriteAPU(Machine, Address, Data);
		return;
	}

	// $4020-$FFFF: Cartridge space.
	WriteMapper(Machine, Address, Data);
}

void RunUntilVerticalBlank(machine& Machine)
{
	cpu& CPU = Machine.CPU;
	apu& APU = Machine.APU;
	ppu& PPU = Machine.PPU;
	mapper& Mapper = Machine.Mapper;

	u64 VBC = PPU.VerticalBlankCount;

	while (PPU.VerticalBlankCount == VBC) {
		if (Machine.InputStrobe) {
			Machine.InputData[0] = Machine.Input[0];
			Machine.InputData[1] = Machine.Input[1];
		}

		// Cycle 0
		StepPPU(Machine);
		StepAPU(Machine);
		StepCPU(Machine);

		// Cycle 4
		StepPPU(Machine);

		// Cycle 6
		CPU.NMI = PPU.VerticalBlankFlag && PPU.NMIOutput;
		CPU.IRQ = Mapper.IRQTrigger > 0 || APU.FrameInterrupt || APU.DMC.Interrupt;
		StepCPUPhase2(Machine);

		// Cycle 8
		StepPPU(Machine);

		// Cycle 12
		Machine.MasterCycle += 12;

		if (Mapper.IRQTrigger > 0) Mapper.IRQTrigger--;
	}
}

void Unload(machine& Machine)
{
	free(Machine.RAM               ); Machine.RAM = nullptr;
	free(Machine.CIRAM             ); Machine.CIRAM = nullptr;
	free(Machine.PRGRAM            ); Machine.PRGRAM = nullptr;
	free(Machine.PRGROM            ); Machine.PRGROM = nullptr;
	free(Machine.CHR               ); Machine.CHR = nullptr;
	free(Machine.APU.AudioBuffer   ); Machine.APU.AudioBuffer = nullptr;
	free(Machine.PPU.FrameBuffer[0]); Machine.PPU.FrameBuffer[0] = nullptr;
	free(Machine.PPU.FrameBuffer[1]); Machine.PPU.FrameBuffer[1] = nullptr;
	Machine.IsLoaded = false;
}

i32 Load(machine& Machine, const char* Path)
{
	struct ines_header
	{
		u8 Magic[4];
		u8 NumPRG;
		u8 NumCHR;
		u8 Flags6;
		u8 Flags7;
		u8 NumRAM;
		u8 Unused[7];
	};

	// Clear current data.
	Unload(Machine);
	memset(&Machine, 0, sizeof(machine));

	FILE* File = fopen(Path, "rb");
	if (!File) return -1;

	// Read header.
	ines_header Header;
	if (fread(&Header, 1, sizeof(ines_header), File) < sizeof(ines_header)) {
		fclose(File);
		return -1;
	}

	// Verify header magic.
	const u8 INESMagic[4] = { 'N', 'E', 'S', 0x1A };
	for (int K = 0; K < 4; K++) {
		if (Header.Magic[K] != INESMagic[K]) {
			fclose(File);
			return -1;
		}
	}

	i32 MapperID = (Header.Flags6 >> 4) | (Header.Flags7 & 0xF0);
	u8 MirrorMode = (Header.Flags6 & 0x01) | ((Header.Flags7 >> 2) & 0x02);

	// Configure mapper.
	const mapper_entry* ME = FindMapperEntry(MapperID);

	if (!ME) {
		fclose(File);
		return -1;
	}

	Machine.Mapper.ID         = MapperID;
	Machine.Mapper.MirrorMode = MirrorMode;
	Machine.Mapper.Reset      = ME->Reset;
	Machine.Mapper.Read       = ME->Read;
	Machine.Mapper.Write      = ME->Write;
	Machine.Mapper.Notify     = ME->Notify;

	// Allocate RAM.
	Machine.RAM = (u8*)calloc(2048, 1);
	Machine.CIRAM = (u8*)calloc(2048, 1);

	// Allocate PRG RAM.
	Machine.PRGRAMSize = 8192;
	Machine.PRGRAM = (u8*)calloc(8192, 1);

	// Load PRG ROM data.
	Machine.PRGROMSize = Header.NumPRG * 16384;
	Machine.PRGROM = (u8*)calloc(Machine.PRGROMSize, 1);
	if (fread(Machine.PRGROM, 1, Machine.PRGROMSize, File) < Machine.PRGROMSize) {
		fclose(File);
		return -1;
	}
	
	// Load CHR ROM data.
	if (Header.NumCHR > 0) {
		Machine.CHRSize = Header.NumCHR * 8192;
		Machine.CHR = (u8*)calloc(Machine.CHRSize, 1);
		if (fread(Machine.CHR, 1, Machine.CHRSize, File) < Machine.CHRSize) {
			fclose(File);
			return -1;
		}
	}
	else {
		Machine.CHRSize = 8192;
		Machine.CHR = (u8*)calloc(8192, 1);
	}

	// Allocate frame buffers.
	Machine.PPU.FrameBuffer[0] = (u32*)calloc(256 * 240, sizeof(u32));
	Machine.PPU.FrameBuffer[1] = (u32*)calloc(256 * 240, sizeof(u32));

	Machine.APU.AudioBuffer = (u8*)calloc(8192, 1);

	Machine.IsLoaded = true;

	Reset(Machine);

	return 0;
}