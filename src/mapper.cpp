#include "nes.h"

static inline u16 NameTableOffset(u8 MirrorMode, u16 Address)
{
	if (MirrorMode & 1) {
		// Vertical mirroring.
		return (Address & 0x07FF);
	}
	else {
		// Horizontal mirroring.
		return (Address & 0x0800) >> 1 | (Address & 0x03FF);
	}
}

static inline u8 ReadCIRAM(machine* M, u16 Address)
{
	u16 Offset = NameTableOffset(M->Mapper.MirrorMode, Address);
	return M->CIRAM[Offset];
}

static inline void WriteCIRAM(machine* M, u16 Address, u8 Data)
{
	u16 Offset = NameTableOffset(M->Mapper.MirrorMode, Address);
	M->CIRAM[Offset] = Data;
}

/* --- Mapper 000 ---------------------------------------------------------- */

u8 ReadMapper0(machine* M, u16 Address)
{
	if (Address >= 0x8000) {
		u16 Mask = M->PRGROMSize - 1;
		return M->PRGROM[Address & Mask];
	}
	else if (Address >= 0x6000) {
		return M->PRGRAM[Address & 0x0FFF];
	}
	else if (Address >= 0x4000) {
		// Unmapped.
		return 0;
	}

	// PPU space.
	else if (Address >= 0x2000) {
		return ReadCIRAM(M, Address);
	}
	else {
		return M->CHRROM[Address & 0x1FFF];
	}
}

void WriteMapper0(machine* M, u16 Address, u8 Data)
{
	if (Address >= 0x8000) {
		u16 Mask = M->PRGROMSize - 1;
		M->PRGROM[Address & Mask] = Data;
	}
	else if (Address >= 0x6000) {
		M->PRGRAM[Address & 0x0FFF] = Data;
	}
	else if (Address >= 0x4000) {
		// Unmapped.
		return;
	}

	// PPU space.
	else if (Address >= 0x2000) {
		WriteCIRAM(M, Address, Data);
	}
	else {
		// Read only.
		//M->CHRROM[Address & 0x1FFF] = Data;
	}
}

/* --- Mapper 001 ---------------------------------------------------------- */

static void M001ComputeBankMaps(machine* M)
{
	mapper1* M1 = &M->Mapper._1;

	// Compute PRG offsets based on PRG bank mode.
	switch ((M1->Control >> 2) & 3) {
		case 0: case 1: {
			M1->PRGMap[0] = (M1->PRGBank & 0xFE) * 0x4000;
			M1->PRGMap[1] = (M1->PRGBank | 0x01) * 0x4000;
			break;
		}
		case 2: {
			M1->PRGMap[0] = 0;
			M1->PRGMap[1] = M1->PRGBank * 0x4000;
			break;
		}
		case 3: {
			M1->PRGMap[0] = M1->PRGBank * 0x4000;
			M1->PRGMap[1] = M->PRGROMSize - 0x4000;
			break;
		}
	}

	// Compute CHR offsets based on CHR bank mode.
	if ((M1->Control >> 4) & 1) {
		M1->CHRMap[0] = M1->CHRBank0 * 4096;
		M1->CHRMap[1] = M1->CHRBank1 * 4096;
	}
	else {
		M1->CHRMap[0] = (M1->CHRBank0 & 0xFE) * 4096;
		M1->CHRMap[1] = (M1->CHRBank0 | 0x01) * 4096;
	}
}

void ResetMapper1(machine* M)
{
	mapper1* M1 = &M->Mapper._1;

	M1->LoadCount = 0;
	M1->Control = 0x0C;
	M001ComputeBankMaps(M);
}

u8 ReadMapper1(machine* M, u16 Address)
{
	mapper1* M1 = &M->Mapper._1;

	// CPU space.
	if (Address >= 0x8000) {
		u32 Base = M1->PRGMap[(Address >> 14) & 1];
		u32 Offset = Address & 0x3FFF;
		return M->PRGROM[Base + Offset];
	}
	else if (Address >= 0x6000) {
		return M->PRGRAM[Address & 0x1FFF];
	}
	else if (Address >= 0x4000) {
		// Unmapped.
		return 0;
	}

	// PPU space.
	else if (Address >= 0x2000) {
		return ReadCIRAM(M, Address);
	}
	else {
		u32 Base = M1->CHRMap[(Address >> 12) & 1];
		u32 Offset = Address & 0x0FFF;
		return M->CHRROM[Base + Offset];
	}
}

void WriteMapper1(machine* M, u16 Address, u8 Data)
{
	mapper1* M1 = &M->Mapper._1;

	if (Address >= 0x8000) {
		if (Data & 0x80) {
			ResetMapper1(M);
		}
		else {
			M1->LoadRegister = (M1->LoadRegister >> 1) | (Data & 1) << 4;
			M1->LoadCount += 1;
		}

		if (M1->LoadCount == 5) {
			switch (Address & 0xE000) {
				case 0x8000: M1->Control  = M1->LoadRegister; break;
				case 0xA000: M1->CHRBank0 = M1->LoadRegister; break;
				case 0xC000: M1->CHRBank1 = M1->LoadRegister; break;
				case 0xE000: M1->PRGBank  = M1->LoadRegister & 0x0F; break;
			}
			M001ComputeBankMaps(M);
			M1->LoadCount = 0;
		}
	}
	else if (Address >= 0x6000) {
		M->PRGRAM[Address & 0x1FFF] = Data;
	}
	else if (Address >= 0x2000) {
		WriteCIRAM(M, Address, Data);
	}
	else {
		u32 Base = M1->CHRMap[(Address >> 12) & 1];
		u32 Offset = Address & 0x0FFF;
		M->CHRROM[Base + Offset] = Data;
	}
}

/* --- Mapper 002 ---------------------------------------------------------- */

u8 ReadMapper2(machine* M, u16 Address)
{
	mapper2* M2 = &M->Mapper._2;

	// CPU space.
	if (Address >= 0xC000) {
		u32 Base = M->PRGROMSize - 0x4000;
		u32 Offset = Address & 0x3FFF;
		return M->PRGROM[Base + Offset];
	}
	else if (Address >= 0x8000) {
		u32 Base = M2->PRGBank * 0x4000;
		u32 Offset = Address & 0x3FFF;
		return M->PRGROM[Base + Offset];
	}
	else if (Address >= 0x4000) {
		// Unmapped.
		return 0;
	}

	// PPU space.
	else if (Address >= 0x2000) {
		return ReadCIRAM(M, Address);
	}
	else {
		return M->CHRROM[Address & 0x1FFF];
	}
}

void WriteMapper2(machine* M, u16 Address, u8 Data)
{
	mapper2* M2 = &M->Mapper._2;

	// CPU space.
	if (Address >= 0x10000) {
	}
	else if (Address >= 0x8000) {
		M2->PRGBank = Data;
	}
	else if (Address >= 0x4000) {
		// Unmapped.
		return;
	}

	// PPU space.
	else if (Address >= 0x2000) {
		WriteCIRAM(M, Address, Data);
	}
	else {
		M->CHRROM[Address & 0x1FFF] = Data;
	}
}

/* --- Mapper 003 ---------------------------------------------------------- */

u8 ReadMapper3(machine* M, u16 Address)
{
	mapper3* M3 = &M->Mapper._3;

	if (Address >= 0x8000) {
		u16 Mask = M->PRGROMSize - 1;
		return M->PRGROM[Address & Mask];
	}
	else if (Address >= 0x4000) {
		// Unmapped.
		return 0;
	}

	// PPU space.
	else if (Address >= 0x2000) {
		return ReadCIRAM(M, Address);
	}
	else {
		u32 Base = M3->CHRBank * 8192;
		return M->CHRROM[Base + Address];
	}
}

void WriteMapper3(machine* M, u16 Address, u8 Data)
{
	mapper3* M3 = &M->Mapper._3;

	// CPU space.
	if (Address >= 0x8000) {
		M3->CHRBank = Data;
	}
	else if (Address >= 0x4000) {
		// Unmapped.
	}

	// PPU space.
	else if (Address >= 0x2000) {
		WriteCIRAM(M, Address, Data);
	}
	else {
		// Read only.
		return;
	}
}

/* --- Mapper 004 ---------------------------------------------------------- */

static void ComputeBankMaps_Mapper4(machine* M)
{
	mapper4* M4 = &M->Mapper._4;

	// PRG ROM banks.
	if (M4->BankControl & 0x40) {
		// CPU $8000-$9FFF: 8K fixed PRG ROM bank (-2).
		// CPU $A000-$BFFF: 8K switchable PRG ROM bank 2.
		// CPU $C000-$DFFF: 8K switchable PRG ROM bank 1.
		// CPU $E000-$FFFF: 8K fixed PRG ROM bank (-1).
		M4->PRGMap[0] = M->PRGROMSize - 0x4000;
		M4->PRGMap[1] = M4->BankRegister[7] * 8192;
		M4->PRGMap[2] = M4->BankRegister[6] * 8192;
		M4->PRGMap[3] = M->PRGROMSize - 0x2000;
	}
	else {
		// CPU $8000-$9FFF: 8K switchable PRG ROM bank 1.
		// CPU $A000-$BFFF: 8K switchable PRG ROM bank 2.
		// CPU $C000-$DFFF: 8K fixed PRG ROM bank.
		// CPU $E000-$FFFF: 8K fixed PRG ROM bank.
		M4->PRGMap[0] = M4->BankRegister[6] * 8192;
		M4->PRGMap[1] = M4->BankRegister[7] * 8192;
		M4->PRGMap[2] = M->PRGROMSize - 0x4000;
		M4->PRGMap[3] = M->PRGROMSize - 0x2000;
	}

	// CHR ROM banks.
	if (M4->BankControl & 0x80) {
		// PPU $0000-03FF: 1K switchable CHR bank 1.
		// PPU $0400-07FF: 1K switchable CHR bank 2.
		// PPU $0800-0BFF: 1K switchable CHR bank 3.
		// PPU $0C00-0FFF: 1K switchable CHR bank 4.
		// PPU $1000-17FF: 2K switchable CHR bank 1.
		// PPU $1800-1FFF: 2K switchable CHR bank 2.
		M4->CHRMap[0] = M4->BankRegister[2] * 0x0400;
		M4->CHRMap[1] = M4->BankRegister[3] * 0x0400;
		M4->CHRMap[2] = M4->BankRegister[4] * 0x0400;
		M4->CHRMap[3] = M4->BankRegister[5] * 0x0400;
		M4->CHRMap[4] = (M4->BankRegister[0] & 0xFE) * 0x0400;
		M4->CHRMap[5] = (M4->BankRegister[0] | 0x01) * 0x0400;
		M4->CHRMap[6] = (M4->BankRegister[1] & 0xFE) * 0x0400;
		M4->CHRMap[7] = (M4->BankRegister[1] | 0x01) * 0x0400;
	}
	else {
		// PPU $0000-07FF: 2K switchable CHR bank 1.
		// PPU $0800-0FFF: 2K switchable CHR bank 2.
		// PPU $1000-13FF: 1K switchable CHR bank 1.
		// PPU $1400-17FF: 1K switchable CHR bank 2.
		// PPU $1800-1BFF: 1K switchable CHR bank 3.
		// PPU $1C00-1FFF: 1K switchable CHR bank 4.
		M4->CHRMap[0] = (M4->BankRegister[0] & 0xFE) * 0x0400;
		M4->CHRMap[1] = (M4->BankRegister[0] | 0x01) * 0x0400;
		M4->CHRMap[2] = (M4->BankRegister[1] & 0xFE) * 0x0400;
		M4->CHRMap[3] = (M4->BankRegister[1] | 0x01) * 0x0400;
		M4->CHRMap[4] = M4->BankRegister[2] * 0x0400;
		M4->CHRMap[5] = M4->BankRegister[3] * 0x0400;
		M4->CHRMap[6] = M4->BankRegister[4] * 0x0400;
		M4->CHRMap[7] = M4->BankRegister[5] * 0x0400;
	}
}

void ResetMapper4(machine* M)
{
	ComputeBankMaps_Mapper4(M);
}

u8 ReadMapper4(machine* M, u16 Address)
{
	mapper4* M4 = &M->Mapper._4;

	// CPU space.
	if (Address >= 0x8000) {
		u32 Base = M4->PRGMap[(Address >> 13) & 3];
		u32 Offset = Address & 0x1FFF;
		return M->PRGROM[Base + Offset];
	}
	else if (Address >= 0x6000) {
		// CPU $6000-$7FFF: 8K PRG RAM bank.
		if (M4->PRGRAMEnable) {
			return M->PRGRAM[Address & 0x1FFF];
		}
		else {
			return 0;
		}
	}
	else if (Address >= 0x4000) {
		// Unmapped.
		return 0;
	}

	// PPU space.
	else if (Address >= 0x2000) {
		return ReadCIRAM(M, Address);
	}
	else {
		u32 Base = M4->CHRMap[(Address >> 10) & 7];
		u32 Offset = Address & 0x03FF;
		return M->CHRROM[Base + Offset];
	}
}

void WriteMapper4(machine* M, u16 Address, u8 Data)
{
	mapper4* M4 = &M->Mapper._4;

	// CPU space.
	if (Address >= 0x8000) {
		// CPU $8000-$FFFF: Mapper registers.
		switch (Address & 0xE001) {
			case 0x8000: {
				// Bank control register.
				M4->BankControl = Data;
				ComputeBankMaps_Mapper4(M);
				break;
			}
			case 0x8001: {
				// Bank data register.
				M4->BankRegister[M4->BankControl & 7] = Data;
				ComputeBankMaps_Mapper4(M);
				break;
			}
			case 0xA000: {
				// Nametable mirroring control.
				M->Mapper.MirrorMode = ~Data & 0x01;
				break;
			}
			case 0xA001: {
				// PRG RAM control.
				M4->PRGRAMEnable = (Data >> 7) & 1;
				M4->PRGRAMProtect = (Data >> 6) & 1;
				break;
			}
			case 0xC000: {
				M4->IRQCounterPreset = Data;
				break;
			}
			case 0xC001: {
				M4->IRQCounter = 0;
				M4->IRQCounterLoad = true;
				break;
			}
			case 0xE000: {
				M4->IRQEnable = false;
				break;
			}
			case 0xE001: {
				M4->IRQEnable = true;
				break;
			}
		}
	}
	else if (Address >= 0x6000) {
		// CPU $6000-$7FFF: 8K PRG RAM.
		if (!M4->PRGRAMProtect) {
			M->PRGRAM[Address & 0x1FFF] = Data;
		}
	}
	else if (Address >= 0x4000) {
		// Unmapped.
		return;
	}

	// PPU space.
	else if (Address >= 0x2000) {
		WriteCIRAM(M, Address, Data);
	}
	else {
		// Read only.
		return;
	}
}

void NotifyMapper4(machine* M, mapper_event Event)
{
	mapper4* M4 = &M->Mapper._4;

	if (Event == PPUFilteredA12Edge) {
		if (M4->IRQCounter == 0 || M4->IRQCounterLoad) {
			M4->IRQCounter = M4->IRQCounterPreset;
			M4->IRQCounterLoad = false;
		}
		else {
			M4->IRQCounter -= 1;
		}

		if (M4->IRQEnable && M4->IRQCounter == 0)
			M->Mapper.IRQTrigger = 8;
	}
}
