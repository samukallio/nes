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

static inline u8 ReadCIRAM(machine& Machine, u16 Address)
{
	u16 Offset = NameTableOffset(Machine.Mapper.MirrorMode, Address);
	return Machine.CIRAM[Offset];
}

static inline void WriteCIRAM(machine& Machine, u16 Address, u8 Data)
{
	u16 Offset = NameTableOffset(Machine.Mapper.MirrorMode, Address);
	Machine.CIRAM[Offset] = Data;
}

/* --- Mapper 000 ---------------------------------------------------------- */

u8 ReadMapper0(machine& Machine, u16 Address)
{
	// PPU $0000-$1FFF: CHR RAM.
	if (Address < 0x2000) return Machine.CHR[Address];

	// PPU $2000-$3FFF: CIRAM.
	if (Address < 0x4000) return ReadCIRAM(Machine, Address);

	// CPU $4000-$5FFF: Unmapped.
	if (Address < 0x6000) return Machine.BusData;

	// CPU $6000-$7FFF: PRG RAM.
	if (Address < 0x8000) return Machine.PRGRAM[Address & 0x0FFF];

	// CPU $8000-$FFFF: PRG ROM.
	return Machine.PRGROM[Address & (Machine.PRGROMSize - 1)];
}

void WriteMapper0(machine& Machine, u16 Address, u8 Data)
{
	// PPU $0000-$1FFF: CHR RAM.
	if (Address < 0x2000) {
		Machine.CHR[Address] = Data;
		return;
	}

	// PPU $2000-$3FFF: CIRAM.
	if (Address < 0x4000) {
		WriteCIRAM(Machine, Address, Data);
		return;
	}

	// CPU $4000-$5FFF: Unmapped.
	if (Address < 0x6000) return;

	// CPU $6000-$7FFF: PRG RAM.
	if (Address < 0x8000) {
		Machine.PRGRAM[Address & 0x0FFF] = Data;
		return;
	}

	// CPU $8000-$FFFF: PRG ROM.
	return;
}

/* --- Mapper 001 ---------------------------------------------------------- */

static void Mapper01ComputeBankMaps(machine& Machine)
{
	mapper1& Mapper = Machine.Mapper._1;

	// Compute PRG offsets based on PRG bank mode.
	switch ((Mapper.Control >> 2) & 3) {
		case 0: case 1: {
			Mapper.PRGMap[0] = (Mapper.PRGBank & 0xFE) * 0x4000;
			Mapper.PRGMap[1] = (Mapper.PRGBank | 0x01) * 0x4000;
			break;
		}
		case 2: {
			Mapper.PRGMap[0] = 0;
			Mapper.PRGMap[1] = Mapper.PRGBank * 0x4000;
			break;
		}
		case 3: {
			Mapper.PRGMap[0] = Mapper.PRGBank * 0x4000;
			Mapper.PRGMap[1] = Machine.PRGROMSize - 0x4000;
			break;
		}
	}

	// Compute CHR offsets based on CHR bank mode.
	if ((Mapper.Control >> 4) & 1) {
		Mapper.CHRMap[0] = Mapper.CHRBank0 * 4096;
		Mapper.CHRMap[1] = Mapper.CHRBank1 * 4096;
	}
	else {
		Mapper.CHRMap[0] = (Mapper.CHRBank0 & 0xFE) * 4096;
		Mapper.CHRMap[1] = (Mapper.CHRBank0 | 0x01) * 4096;
	}
}

void ResetMapper1(machine& Machine)
{
	mapper1& Mapper = Machine.Mapper._1;

	Mapper.LoadCount = 0;
	Mapper.Control = 0x0C;
	Mapper01ComputeBankMaps(Machine);
}

u8 ReadMapper1(machine& Machine, u16 Address)
{
	mapper1& Mapper = Machine.Mapper._1;

	// PPU $0000-$1FFF: CHR RAM.
	if (Address < 0x2000) {
		u32 Base = Mapper.CHRMap[(Address >> 12) & 1];
		u32 Offset = Address & 0x0FFF;
		return Machine.CHR[Base + Offset];
	}

	// PPU $2000-$3FFF: CIRAM.
	if (Address < 0x4000) return ReadCIRAM(Machine, Address);

	// CPU $4000-$5FFF: Unmapped.
	if (Address < 0x6000) return Machine.BusData;

	// CPU $6000-$7FFF: PRG RAM.
	if (Address < 0x8000) return Machine.PRGRAM[Address & 0x1FFF];

	// CPU $8000-$FFFF: PRG ROM.
	u32 Base = Mapper.PRGMap[(Address >> 14) & 1];
	u32 Offset = Address & 0x3FFF;
	return Machine.PRGROM[Base + Offset];
}

void WriteMapper1(machine& Machine, u16 Address, u8 Data)
{
	mapper1& Mapper = Machine.Mapper._1;

	// PPU $0000-$1FFF: CHR RAM.
	if (Address < 0x2000) {
		u32 Base = Mapper.CHRMap[(Address >> 12) & 1];
		u32 Offset = Address & 0x0FFF;
		Machine.CHR[Base + Offset] = Data;
		return;
	}

	// PPU $2000-$3FFF: CIRAM.
	if (Address < 0x4000) {
		WriteCIRAM(Machine, Address, Data);
		return;
	}

	// CPU $4000-$5FFF: Unmapped.
	if (Address < 0x6000) return;

	// CPU $6000-$7FFF: PRG RAM.
	if (Address < 0x8000) {
		Machine.PRGRAM[Address & 0x1FFF] = Data;
		return;
	}

	// CPU $8000-$FFFF: PRG ROM registers.
	if (Data & 0x80) {
		ResetMapper1(Machine);
	}
	else {
		Mapper.LoadRegister = (Mapper.LoadRegister >> 1) | (Data & 1) << 4;
		Mapper.LoadCount += 1;
	}

	if (Mapper.LoadCount == 5) {
		switch (Address & 0xE000) {
			case 0x8000: Mapper.Control  = Mapper.LoadRegister; break;
			case 0xA000: Mapper.CHRBank0 = Mapper.LoadRegister; break;
			case 0xC000: Mapper.CHRBank1 = Mapper.LoadRegister; break;
			case 0xE000: Mapper.PRGBank  = Mapper.LoadRegister & 0x0F; break;
		}
		Mapper01ComputeBankMaps(Machine);
		Mapper.LoadCount = 0;
	}
}

/* --- Mapper 002 ---------------------------------------------------------- */

u8 ReadMapper2(machine& Machine, u16 Address)
{
	mapper2& Mapper = Machine.Mapper._2;

	// PPU $0000-$1FFF: CHR RAM.
	if (Address < 0x2000) return Machine.CHR[Address];

	// PPU $2000-$3FFF: CIRAM.
	if (Address < 0x4000) return ReadCIRAM(Machine, Address);

	// CPU $4000-$7FFF: Unmapped.
	if (Address < 0x8000) return Machine.BusData;

	// CPU $8000-$BFFF: 16K switchable PRG ROM bank.
	if (Address < 0xC000) {
		u32 Base = Mapper.PRGBank * 0x4000;
		u32 Offset = Address & 0x3FFF;
		return Machine.PRGROM[Base + Offset];
	}

	// CPU $C000-$FFFF: 16K fixed PRG ROM bank.
	u32 Base = Machine.PRGROMSize - 0x4000;
	u32 Offset = Address & 0x3FFF;
	return Machine.PRGROM[Base + Offset];
}

void WriteMapper2(machine& Machine, u16 Address, u8 Data)
{
	mapper2& Mapper = Machine.Mapper._2;

	// PPU $0000-$1FFF: CHR RAM.
	if (Address < 0x2000) {
		Machine.CHR[Address] = Data;
		return;
	}

	// PPU $2000-$3FFF: CIRAM.
	if (Address < 0x4000) {
		WriteCIRAM(Machine, Address, Data);
		return;
	}

	// CPU $4000-$7FFF: Unmapped.
	if (Address < 0x8000) return;

	// CPU $8000-$FFFF: PRG ROM bank select register.
	Mapper.PRGBank = Data;
}

/* --- Mapper 003 ---------------------------------------------------------- */

u8 ReadMapper3(machine& Machine, u16 Address)
{
	mapper3& Mapper = Machine.Mapper._3;

	// PPU $0000-$1FFF: CHR ROM.
	if (Address < 0x2000) return Machine.CHR[Mapper.CHRBank * 8192 + Address];

	// PPU $2000-$3FFF: CIRAM.
	if (Address < 0x4000) return ReadCIRAM(Machine, Address);

	// CPU $4000-$7FFF: Unmapped.
	if (Address < 0x8000) return Machine.BusData;

	// CPU $8000-$FFFF: PRG ROM.
	return Machine.PRGROM[Address & (Machine.PRGROMSize - 1)];
}

void WriteMapper3(machine& Machine, u16 Address, u8 Data)
{
	mapper3& Mapper = Machine.Mapper._3;

	// PPU $0000-$1FFF: CHR ROM (read only).
	if (Address < 0x2000) return;

	// PPU $2000-$3FFF: CIRAM.
	if (Address < 0x4000) {
		WriteCIRAM(Machine, Address, Data);
		return;
	}

	// CPU $4000-$7FFF: Unmapped.
	if (Address < 0x8000) return;

	// CPU $8000-$FFFF: CHR ROM bank select register.
	Mapper.CHRBank = Data;
}

/* --- Mapper 004 ---------------------------------------------------------- */

static void ComputeBankMaps_Mapper4(machine& Machine)
{
	mapper4& Mapper = Machine.Mapper._4;

	// PRG ROM banks.
	if (Mapper.BankControl & 0x40) {
		// CPU $8000-$9FFF: 8K fixed PRG ROM bank (-2).
		// CPU $A000-$BFFF: 8K switchable PRG ROM bank 2.
		// CPU $C000-$DFFF: 8K switchable PRG ROM bank 1.
		// CPU $E000-$FFFF: 8K fixed PRG ROM bank (-1).
		Mapper.PRGMap[0] = Machine.PRGROMSize - 0x4000;
		Mapper.PRGMap[1] = Mapper.BankRegister[7] * 8192;
		Mapper.PRGMap[2] = Mapper.BankRegister[6] * 8192;
		Mapper.PRGMap[3] = Machine.PRGROMSize - 0x2000;
	}
	else {
		// CPU $8000-$9FFF: 8K switchable PRG ROM bank 1.
		// CPU $A000-$BFFF: 8K switchable PRG ROM bank 2.
		// CPU $C000-$DFFF: 8K fixed PRG ROM bank.
		// CPU $E000-$FFFF: 8K fixed PRG ROM bank.
		Mapper.PRGMap[0] = Mapper.BankRegister[6] * 8192;
		Mapper.PRGMap[1] = Mapper.BankRegister[7] * 8192;
		Mapper.PRGMap[2] = Machine.PRGROMSize - 0x4000;
		Mapper.PRGMap[3] = Machine.PRGROMSize - 0x2000;
	}

	// CHR ROM banks.
	if (Mapper.BankControl & 0x80) {
		// PPU $0000-03FF: 1K switchable CHR bank 1.
		// PPU $0400-07FF: 1K switchable CHR bank 2.
		// PPU $0800-0BFF: 1K switchable CHR bank 3.
		// PPU $0C00-0FFF: 1K switchable CHR bank 4.
		// PPU $1000-17FF: 2K switchable CHR bank 1.
		// PPU $1800-1FFF: 2K switchable CHR bank 2.
		Mapper.CHRMap[0] = Mapper.BankRegister[2] * 0x0400;
		Mapper.CHRMap[1] = Mapper.BankRegister[3] * 0x0400;
		Mapper.CHRMap[2] = Mapper.BankRegister[4] * 0x0400;
		Mapper.CHRMap[3] = Mapper.BankRegister[5] * 0x0400;
		Mapper.CHRMap[4] = (Mapper.BankRegister[0] & 0xFE) * 0x0400;
		Mapper.CHRMap[5] = (Mapper.BankRegister[0] | 0x01) * 0x0400;
		Mapper.CHRMap[6] = (Mapper.BankRegister[1] & 0xFE) * 0x0400;
		Mapper.CHRMap[7] = (Mapper.BankRegister[1] | 0x01) * 0x0400;
	}
	else {
		// PPU $0000-07FF: 2K switchable CHR bank 1.
		// PPU $0800-0FFF: 2K switchable CHR bank 2.
		// PPU $1000-13FF: 1K switchable CHR bank 1.
		// PPU $1400-17FF: 1K switchable CHR bank 2.
		// PPU $1800-1BFF: 1K switchable CHR bank 3.
		// PPU $1C00-1FFF: 1K switchable CHR bank 4.
		Mapper.CHRMap[0] = (Mapper.BankRegister[0] & 0xFE) * 0x0400;
		Mapper.CHRMap[1] = (Mapper.BankRegister[0] | 0x01) * 0x0400;
		Mapper.CHRMap[2] = (Mapper.BankRegister[1] & 0xFE) * 0x0400;
		Mapper.CHRMap[3] = (Mapper.BankRegister[1] | 0x01) * 0x0400;
		Mapper.CHRMap[4] = Mapper.BankRegister[2] * 0x0400;
		Mapper.CHRMap[5] = Mapper.BankRegister[3] * 0x0400;
		Mapper.CHRMap[6] = Mapper.BankRegister[4] * 0x0400;
		Mapper.CHRMap[7] = Mapper.BankRegister[5] * 0x0400;
	}
}

void ResetMapper4(machine& Machine)
{
	ComputeBankMaps_Mapper4(Machine);
}

u8 ReadMapper4(machine& Machine, u16 Address)
{
	mapper4& Mapper = Machine.Mapper._4;

	// PPU $0000-$1FFF: CHR ROM.
	if (Address < 0x2000) {
		u32 Base = Mapper.CHRMap[(Address >> 10) & 7];
		u32 Offset = Address & 0x03FF;
		return Machine.CHR[Base + Offset];
	}

	// PPU $2000-$3FFF: CIRAM.
	if (Address < 0x4000) return ReadCIRAM(Machine, Address);

	// CPU $4000-$5FFF: Unmapped.
	if (Address < 0x6000) return Machine.BusData;

	// CPU $6000-$7FFF: 8K PRG RAM bank.
	if (Address < 0x8000) {
		if (!Mapper.PRGRAMEnable) return Machine.BusData;
		return Machine.PRGRAM[Address & 0x1FFF];
	}

	// CPU $8000-$FFFF: 8K PRG ROM banks.
	u32 Base = Mapper.PRGMap[(Address >> 13) & 3];
	u32 Offset = Address & 0x1FFF;
	return Machine.PRGROM[Base + Offset];
}

void WriteMapper4(machine& Machine, u16 Address, u8 Data)
{
	mapper4& Mapper = Machine.Mapper._4;

	// PPU $0000-$1FFF: CHR ROM (read only).
	if (Address < 0x2000) return;

	// PPU $2000-$3FFF: CIRAM.
	if (Address < 0x4000) {
		WriteCIRAM(Machine, Address, Data);
		return;
	}

	// CPU $4000-$5FFF: Unmapped.
	if (Address < 0x6000) return;

	// CPU $6000-$7FFF: 8K PRG RAM bank.
	if (Address < 0x8000) {
		if (Mapper.PRGRAMProtect) return;
		Machine.PRGRAM[Address & 0x1FFF] = Data;
		return;
	}

	// CPU $8000-$FFFF: Mapper registers.
	switch (Address & 0xE001) {
		case 0x8000: {
			// Bank control register.
			Mapper.BankControl = Data;
			ComputeBankMaps_Mapper4(Machine);
			break;
		}
		case 0x8001: {
			// Bank data register.
			Mapper.BankRegister[Mapper.BankControl & 7] = Data;
			ComputeBankMaps_Mapper4(Machine);
			break;
		}
		case 0xA000: {
			// Nametable mirroring control.
			Machine.Mapper.MirrorMode = ~Data & 0x01;
			break;
		}
		case 0xA001: {
			// PRG RAM control.
			Mapper.PRGRAMEnable = (Data >> 7) & 1;
			Mapper.PRGRAMProtect = (Data >> 6) & 1;
			break;
		}
		case 0xC000: {
			Mapper.IRQCounterPreset = Data;
			break;
		}
		case 0xC001: {
			Mapper.IRQCounter = 0;
			Mapper.IRQCounterLoad = true;
			break;
		}
		case 0xE000: {
			Mapper.IRQEnable = false;
			break;
		}
		case 0xE001: {
			Mapper.IRQEnable = true;
			break;
		}
	}
}

void NotifyMapper4(machine& Machine, mapper_event Event)
{
	mapper4& Mapper = Machine.Mapper._4;

	if (Event == PPUFilteredA12Edge) {
		if (Mapper.IRQCounter == 0 || Mapper.IRQCounterLoad) {
			Mapper.IRQCounter = Mapper.IRQCounterPreset;
			Mapper.IRQCounterLoad = false;
		}
		else {
			Mapper.IRQCounter -= 1;
		}

		if (Mapper.IRQEnable && Mapper.IRQCounter == 0)
			Machine.Mapper.IRQTrigger = 8;
	}
}
