#include <assert.h>

#include "nes.h"

static inline u16 PatternTableAddress(u16 Table, u16 Tile, u16 Row, u16 Plane)
{
	return 0x1000 * Table + 16 * Tile + 8 * Plane + Row;
}

static inline u16 PaletteOffset(u16 Address)
{
	if ((Address & 0x13) == 0x10)
		Address &= 0x0F;
	return Address & 0x1F;
}

u8 ReadPPU(machine& Machine, u16 Address)
{
	ppu& PPU = Machine.PPU;

	u8 BusData = 0;
	u8 BusMask = 0;

	switch (Address) {
		case 0x2002: { // PPUSTATUS
			BusData = (PPU.SpriteOverflow    ? 0x20 : 0x00)
			        | (PPU.SpriteZeroHit     ? 0x40 : 0x00)
			        | (PPU.VerticalBlankFlag ? 0x80 : 0x00)
			        ;
			BusMask = 0xE0;

			PPU.W = 0;
			PPU.VerticalBlankFlag = false;
			PPU.VerticalBlankFlagInhibit = true;
			break;
		}
		case 0x2004: { // OAMDATA
			BusData = PPU.SpriteOAM[PPU.OAMAddress];
			BusMask = 0xFF;
			break;
		}
		case 0x2007: { // PPUDATA
			u16 Address = PPU.V & 0x3FFF;

			if (Address < 0x3F00) {
				BusData = PPU.ReadBuffer;
				BusMask = 0xFF;
				PPU.ReadBuffer = ReadMapper(Machine, Address);
			}
			else {
				BusData = PPU.Palette[PaletteOffset(Address)];
				BusMask = 0x3F;
				PPU.ReadBuffer = ReadMapper(Machine, (PPU.V - 0x1000) & 0x3FFF);
			}

			PPU.V += PPU.VIncrementBy32 ? 32 : 1;

			break;
		}
	}

	// Drive the data that was just read onto the bus.
	PPU.BusData = (PPU.BusData & ~BusMask) | BusData;

	// Mark every bus data line that was driven as refreshed during this master cycle.
	for (int I = 0; I < 8; I++)
		if (BusMask & (1 << I))
			PPU.BusDataRefreshCycle[I] = PPU.MasterCycle;

	// For those data lines that haven't been driven in a while, decay the line value to zero.
	for (int I = 0; I < 8; I++)
		if (PPU.MasterCycle > PPU.BusDataRefreshCycle[I] + PPUBusDataDecayCycleCount)
			PPU.BusData &= ~(1 << I);

	return PPU.BusData;
}

void WritePPU(machine& Machine, u16 Address, u8 Data)
{
	ppu& PPU = Machine.PPU;

	// Remember the written data to emulate open-bus behavior.
	PPU.BusData = Data;

	// A CPU write to the PPU bus refreshes all data lines.
	for (int I = 0; I < 8; I++)
		PPU.BusDataRefreshCycle[I] = PPU.MasterCycle;

	switch (Address) {
		case 0x2000: { // PPUCTRL
			PPU.VIncrementBy32         = (Data >> 2) & 0x01;
			PPU.SpritePatternTable     = (Data >> 3) & 0x01;
			PPU.BackgroundPatternTable = (Data >> 4) & 0x01;
			PPU.Sprite8x16             = (Data >> 5) & 0x01;
			PPU.MasterSlaveSelect      = (Data >> 6) & 0x01;
			PPU.NMIOutput              = (Data >> 7) & 0x01;
			PPU.T = (PPU.T & 0x73FF) | u16(Data & 0x03) << 10;
			break;
		}
		case 0x2001: { // PPUMASK
			PPU.GrayscaleEnable           =  Data       & 0x01;
			PPU.BackgroundShowLeftMargin  = (Data >> 1) & 0x01;
			PPU.SpriteShowLeftMargin      = (Data >> 2) & 0x01;
			PPU.BackgroundEnable          = (Data >> 3) & 0x01;
			PPU.SpriteEnable              = (Data >> 4) & 0x01;
			PPU.TintMode                  = (Data >> 5) & 0x07;
			break;
		}
		case 0x2003: { // OAMADDR
			PPU.OAMAddress = Data;
			break;
		}
		case 0x2004: { // OAMDATA
			if ((PPU.OAMAddress & 3) == 2) Data &= 0xE3;
			PPU.SpriteOAM[PPU.OAMAddress] = Data;
			PPU.OAMAddress++;
			break;
		}
		case 0x2005: { // PPUSCROLL
			if (!PPU.W) {
				PPU.T = (PPU.T & 0x7FE0) | u16(Data) >> 3;
				PPU.X = Data & 0x07;
				PPU.W = 1;
			}
			else {
				PPU.T = (PPU.T & 0x0C1F) | u16(Data & 0x07) << 12 | u16(Data & 0xF8) << 2;
				PPU.W = 0;
			}
			break;
		}
		case 0x2006: { // PPUADDR
			if (!PPU.W) {
				PPU.T = (PPU.T & 0x00FF) | u16(Data & 0x3F) << 8;
				PPU.W = 1;
			}
			else {
				u16 OldV = PPU.V;

				PPU.T = (PPU.T & 0x7F00) | Data;
				PPU.V = PPU.T;
				PPU.W = 0;

				if (!(OldV & 0x1000) && (PPU.V & 0x1000) && Machine.Mapper.Notify)
					Machine.Mapper.Notify(Machine, PPUFilteredA12Edge);
			}
			break;
		}
		case 0x2007: { // PPUDATA
			if (PPU.V >= 0x3F00) {
				PPU.Palette[PaletteOffset(PPU.V)] = Data;
			}
			else {
				WriteMapper(Machine, PPU.V, Data);
			}

			PPU.V += PPU.VIncrementBy32 ? 32 : 1;
			break;
		}
	}
}

void StepPPU(machine& Machine)
{
	ppu& PPU = Machine.PPU;

	bool IsRendering = PPU.BackgroundEnable || PPU.SpriteEnable;

	// Odd frames end one cycle earlier than even frames.
	u32 ScanWidth;
	if (IsRendering && PPU.Frame % 2 == 1 && PPU.ScanY == 261)
		ScanWidth = 339;
	else
		ScanWidth = 340;

	// Increment scan position.
	PPU.ScanX += 1;

	if (PPU.ScanX > ScanWidth) {
		PPU.ScanX = 0;
		PPU.ScanY += 1;
	}

	if (PPU.ScanY > 261) {
		PPU.ScanY = 0;
		PPU.Frame += 1;
	}

	// Vertical scan position predicates.
	bool IsRenderY    = PPU.ScanY <  240;
	bool IsPreRenderY = PPU.ScanY == 261;
	bool IsFetchY     = IsPreRenderY || IsRenderY;

	// Horizontal scan position predicates.
	bool IsRenderX    = PPU.ScanX >=   1 && PPU.ScanX <= 256;
	bool IsPreRenderX = PPU.ScanX >= 321 && PPU.ScanX <= 336;
	bool IsFetchX     = IsRenderX || IsPreRenderX;

	// If rendering enabled and in visible area, produce an output pixel.
	if (IsRendering && IsRenderY && IsRenderX) {
		u32* FrameBuffer = PPU.FrameBuffer[PPU.Frame & 1];
		i32 FrameY = PPU.ScanY;
		i32 FrameX = PPU.ScanX - 1;

		bool IsLeftMargin = FrameX < 8;

		u8 FinalColor = 0;
		if (PPU.BackgroundEnable && (!IsLeftMargin || PPU.BackgroundShowLeftMargin)) {
			FinalColor = u8(PPU.TileColorData >> (60 - 4 * PPU.X)) & 0x0F;
		}

		bool IsBackground = (FinalColor & 0x03) != 0;

		if (PPU.SpriteEnable && (!IsLeftMargin || PPU.SpriteShowLeftMargin)) {
			for (u32 I = 0; I < PPU.SpriteCount; I++) {
				// Determine if the sprite is visible here, and the visible column index.
				i32 Column = FrameX - PPU.SpriteX[I];
				if (Column < 0 || Column > 7) continue;

				// Get the color value at this column and check that it is not transparent.
				u8 Color = ((PPU.SpriteColorData[I] >> (28 - 4 * Column)) & 0x0F) | 0x10;
				if ((Color & 0x03) == 0) continue;

				// Check for sprite 0 collision with the background.
				if (IsBackground && PPU.SpriteIndex[I] == 0 && FrameX < 255)
					PPU.SpriteZeroHit = true;

				// Determine final color depending on whether there's a background and
				// if the sprite has priority over the background.
				if (!IsBackground || PPU.SpritePriority[I] == 0)
					FinalColor = Color;

				break;
			}
		}

		//
		if ((FinalColor & 0x03) == 0)
			FinalColor = 0;

		FrameBuffer[FrameY * 256 + FrameX] = PPUColorTable[PPU.Palette[FinalColor]];
	}

	// Fetch background tile data from VRAM.
	if (IsRendering && IsFetchY && IsFetchX) {
		//
		PPU.TileColorData <<= 4;

		switch (PPU.ScanX % 8) {
			case 1: {
				// Fetch tile pattern index from nametable.
				u16 Address = 0x2000 | (PPU.V & 0x0FFF);
				PPU.TilePatternIndex = ReadMapper(Machine, Address);
				break;
			}
			case 3: {
				// Fetch tile palette index from attribute table.
				u16 Address = 0x23C0 | (PPU.V & 0x0C00) | ((PPU.V >> 4) & 0x38) | ((PPU.V >> 2) & 0x07);
				u8 Shift = ((PPU.V >> 4) & 4) | (PPU.V & 2);
				PPU.TilePaletteIndex = (ReadMapper(Machine, Address) >> Shift) & 0x03;
				break;
			}
			case 5: {
				// Fetch low byte of tile pattern.
				u16 Address = PatternTableAddress(PPU.BackgroundPatternTable, PPU.TilePatternIndex, (PPU.V >> 12) & 0x07, 0);
				PPU.TilePatternL = ReadMapper(Machine, Address);
				break;
			}
			case 7: {
				// Fetch high byte of tile pattern.
				u16 Address = PatternTableAddress(PPU.BackgroundPatternTable, PPU.TilePatternIndex, (PPU.V >> 12) & 0x07, 1);
				PPU.TilePatternH = ReadMapper(Machine, Address);
				break;
			}
			case 0: {
				u32 ColorData = 0;
				u8 ColorBase = PPU.TilePaletteIndex << 2;
				u8 PatternL = PPU.TilePatternL;
				u8 PatternH = PPU.TilePatternH;
				for (int I = 0; I < 8; I++) {
					// Compose the color for the pixel from the base color and two pattern bits.
					ColorData = (ColorData << 4) | ColorBase | (PatternH & 0x80) >> 6 | (PatternL & 0x80) >> 7;
					PatternH <<= 1;
					PatternL <<= 1;
				}
				PPU.TileColorData |= ColorData;
				break;
			}
		}
	}

	// Sprite evaluation logic.
	if (IsRendering && IsRenderY && PPU.ScanX == 257) {
		u32 Count = 0;
		for (u32 I = 0; I < 64; I++) {
			u8 Y         = PPU.SpriteOAM[4 * I + 0];
			u8 TileIndex = PPU.SpriteOAM[4 * I + 1];
			u8 Flags     = PPU.SpriteOAM[4 * I + 2];
			u8 X         = PPU.SpriteOAM[4 * I + 3];
			u8 H         = PPU.Sprite8x16 ? 16 : 8;

			if (PPU.ScanY < Y) continue;
			if (PPU.ScanY - Y >= H) continue;

			if (Count < 8) {
				// Compute sprite row, applying vertical flip if the flag is set.
				u8 Row = PPU.ScanY - Y;
				if (Flags & 0x80)
					Row = H - Row - 1;

				// Fetch sprite pattern data.
				u16 Address;
				if (PPU.Sprite8x16)
					Address = PatternTableAddress(TileIndex & 0x01, (TileIndex & 0xFE) | (Row >> 3), Row & 0x07, 0);
				else
					Address = PatternTableAddress(PPU.SpritePatternTable, TileIndex, Row, 0);

				u8 PatternL = ReadMapper(Machine, Address);
				u8 PatternH = ReadMapper(Machine, Address+8);

				// Make the color data for the visible sprite row.
				u8 ColorBase = (Flags & 0x03) << 2;
				u32 ColorData = 0;
				if (Flags & 0x40) {
					// Horizontal flipping.
					for (u32 I = 0; I < 8; I++) {
						ColorData = (ColorData << 4) | ColorBase | (PatternH & 0x01) << 1 | (PatternL & 0x01);
						PatternL >>= 1;
						PatternH >>= 1;
					}
				}
				else {
					// No horizontal flipping.
					for (u32 I = 0; I < 8; I++) {
						ColorData = (ColorData << 4) | ColorBase | (PatternH & 0x80) >> 6 | (PatternL & 0x80) >> 7;
						PatternL <<= 1;
						PatternH <<= 1;
					}
				}

				// Add sprite to render list.
				PPU.SpriteIndex[Count] = I;
				PPU.SpritePriority[Count] = (Flags >> 5) & 0x01;
				PPU.SpriteX[Count] = X;
				PPU.SpriteY[Count] = Y;
				PPU.SpriteColorData[Count] = ColorData;
			}
			Count += 1;
		}
		PPU.SpriteCount = Count > 8 ? 8 : Count;
		if (Count > 8) PPU.SpriteOverflow = true;
	}

	// VRAM address update logic.
	if (IsRendering && IsFetchY) {
		// Increment X every 8 cycles while fetching data.
		if (IsFetchX && PPU.ScanX % 8 == 0) {
			if ((PPU.V & 0x001F) == 0x1F) {
				// Reset X to 0 and switch horizontal nametable.
				PPU.V ^= 0x001F | 0x0400;
			}
			else {
				// Increment X.
				PPU.V += 1;
			}
		}

		// Increment Y at the end of a line.
		if (PPU.ScanX == 256) {
			if ((PPU.V & 0x7000) == 0x7000) {
				u8 Y = (PPU.V >> 5) & 0x1F;
				if (Y == 29) {
					// Reset Y to 0 and switch vertical nametable.
					PPU.V ^= 0x73A0 | 0x0800;
				}
				else if (Y == 31) {
					// Reset Y to 0.
					PPU.V &= 0x0C1F;
				}
				else {
					// Reset fine Y.
					PPU.V &= 0x0FFF;
					// Increment coarse Y.
					PPU.V += 0x0020;
				}
			}
			else {
				// Increment fine Y.
				PPU.V += 0x1000;
			}
		}

		// Load X from the T register for the next line.
		if (PPU.ScanX == 257) {
			PPU.V = (PPU.V & 0x7BE0) | (PPU.T & 0x041F);
		}

		// Load Y from the T register for the next frame.
		if (IsPreRenderY && PPU.ScanX >= 280 && PPU.ScanX <= 304) {
			PPU.V = (PPU.V & 0x041F) | (PPU.T & 0x7BE0);
		}
	}

	// Vertical blank logic.
	if (PPU.ScanY == 241 && PPU.ScanX == 1) {
		if (!PPU.VerticalBlankFlagInhibit) {
			PPU.VerticalBlankFlag = true;
		}
		PPU.VerticalBlankCount += 1;
	}
	if (IsPreRenderY && PPU.ScanX == 1) {
		PPU.VerticalBlankFlag = false;

		PPU.SpriteCount = 0;
		PPU.SpriteZeroHit = false;
		PPU.SpriteOverflow = false;
	}

	// MMC3-based mappers monitor PPU A12 to count scanlines. Since PPU
	// memory accesses are not emulated accurately, we instead detect an
	// (approximately) equivalent condition and notify the mapper.
	if (IsRendering && IsFetchY && PPU.ScanX == 260)
		if (Machine.Mapper.Notify) Machine.Mapper.Notify(Machine, PPUFilteredA12Edge);

	//
	PPU.MasterCycle += 4;
	PPU.VerticalBlankFlagInhibit = false;
}
