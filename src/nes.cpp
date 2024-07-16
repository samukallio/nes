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

void Reset(machine* M)
{
	if (M->Mapper.Reset) M->Mapper.Reset(M);

	M->PPU.BackgroundPatternTable = 0;
	M->PPU.SpritePatternTable = 0;
	M->PPU.Sprite8x16 = false;
	M->PPU.VIncrementBy32 = false;
	M->PPU.MasterSlaveSelect = false;
	M->PPU.NMIOutput = false;

	M->PPU.OAMAddress = 0;

	M->PPU.V = 0;
	M->PPU.T = 0;
	M->PPU.X = 0;
	M->PPU.W = 0;

	M->PPU.Frame = 0;
	M->PPU.ScanY = 261;
	M->PPU.ScanX = 0;


	M->APU.AudioPointer = 0;

	M->APU.Noise.NoiseRegister = 0x0001;

	M->CPU.State = 0;
}

static inline u8 ReadController(machine* M, i32 Index)
{
	u8 Bit = M->InputData[Index] & 0x01;
	M->InputData[Index] = 0x80 | M->InputData[Index] >> 1;
	return Bit;
}

u8 Read(machine* M, u16 Address)
{
	u8 Data = 0;

	if (Address >= 0x4020)
		Data = ReadMapper(M, Address);
	else if (Address == 0x4017)
		Data = ReadController(M, 1);
	else if (Address == 0x4016)
		Data = ReadController(M, 0);
	else if (Address == 0x4015)
		Data = ReadAPU(M, Address);
	else if (Address == 0x4014)
		Data = ReadPPU(M, Address);
	else if (Address >= 0x4000)
		Data = 0;
	else if (Address >= 0x2000)
		Data = ReadPPU(M, Address & 0x2007);
	else
		Data = M->RAM[Address & 0x07FF];

	return Data;
}

void Write(machine* M, u16 Address, u8 Data)
{
	if (Address >= 0x4020)
		WriteMapper(M, Address, Data);
	else if (Address == 0x4017)
		WriteAPU(M, 0x4017, Data);
	else if (Address == 0x4016)
		M->InputStrobe = Data & 0x01;
	else if (Address == 0x4015)
		WriteAPU(M, 0x4015, Data);
	else if (Address == 0x4014)
		WritePPU(M, 0x4014, Data);
	else if (Address >= 0x4000)
		WriteAPU(M, Address, Data);
	else if (Address >= 0x2000)
		WritePPU(M, Address & 0x2007, Data);
	else
		M->RAM[Address & 0x07FF] = Data;
}

void RunUntilVerticalBlank(machine* M)
{
	cpu &CPU = M->CPU;
	apu &APU = M->APU;
	ppu &PPU = M->PPU;
	mapper &Mapper = M->Mapper;

	u64 VBC = PPU.VerticalBlankCount;

	while (PPU.VerticalBlankCount == VBC) {
		if (M->InputStrobe) {
			M->InputData[0] = M->Input[0];
			M->InputData[1] = M->Input[1];
		}

		// Cycle 0
		StepPPU(M);
		StepAPU(M);
		StepCPU(M);

		// Cycle 4
		StepPPU(M);

		// Cycle 6
		CPU.NMI = PPU.VerticalBlankFlag && PPU.NMIOutput;
		CPU.IRQ = Mapper.IRQTrigger > 0 || APU.FrameInterrupt || APU.DMC.Interrupt;
		StepCPUPhase2(M);

		// Cycle 8
		StepPPU(M);

		// Cycle 12
		M->MasterCycle += 12;

		if (Mapper.IRQTrigger > 0)
			Mapper.IRQTrigger--;
	}
}

void Unload(machine* M)
{
	free(M->RAM); M->RAM = nullptr;
	free(M->CIRAM); M->CIRAM = nullptr;
	free(M->PRGRAM); M->PRGRAM = nullptr;
	free(M->PRGROM); M->PRGROM = nullptr;
	free(M->CHRROM); M->CHRROM = nullptr;
	free(M->APU.AudioBuffer); M->APU.AudioBuffer = nullptr;
	free(M->PPU.FrameBuffer[0]); M->PPU.FrameBuffer[0] = nullptr;
	free(M->PPU.FrameBuffer[1]); M->PPU.FrameBuffer[1] = nullptr;
	M->IsLoaded = false;
}

i32 Load(machine* M, const char* Path)
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
	Unload(M);

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

	M->Mapper.ID         = MapperID;
	M->Mapper.MirrorMode = MirrorMode;
	M->Mapper.Reset      = ME->Reset;
	M->Mapper.Read       = ME->Read;
	M->Mapper.Write      = ME->Write;
	M->Mapper.Notify     = ME->Notify;

	// Allocate RAM.
	M->RAM = (u8*)calloc(2048, 1);
	M->CIRAM = (u8*)calloc(2048, 1);

	// Allocate PRG RAM.
	M->PRGRAMSize = 8192;
	M->PRGRAM = (u8*)calloc(8192, 1);

	// Load PRG ROM data.
	M->PRGROMSize = Header.NumPRG * 16384;
	M->PRGROM = (u8*)calloc(M->PRGROMSize, 1);
	if (fread(M->PRGROM, 1, M->PRGROMSize, File) < M->PRGROMSize) {
		fclose(File);
		return -1;
	}
	
	// Load CHR ROM data.
	if (Header.NumCHR > 0) {
		M->CHRROMSize = Header.NumCHR * 8192;
		M->CHRROM = (u8*)calloc(M->CHRROMSize, 1);
		if (fread(M->CHRROM, 1, M->CHRROMSize, File) < M->CHRROMSize) {
			fclose(File);
			return -1;
		}
	}
	else {
		M->CHRROMSize = 8192;
		M->CHRROM = (u8*)calloc(8192, 1);
	}

	// Allocate frame buffers.
	M->PPU.FrameBuffer[0] = (u32*)calloc(256 * 240, sizeof(u32));
	M->PPU.FrameBuffer[1] = (u32*)calloc(256 * 240, sizeof(u32));

	M->APU.AudioBuffer = (u8*)calloc(8192, 1);

	M->IsLoaded = true;

	Reset(M);

	return 0;
}