#include <assert.h>
#include <stdio.h>

#include "nes.h"

u8 ReadAPU(machine* M, u16 Address)
{
	apu* APU = &M->APU;

	switch (Address) {
		case 0x4015: {
			u8 Data
				= (APU->Pulse[0].Length > 0           ? 0x01 : 0x00)
				| (APU->Pulse[1].Length > 0           ? 0x02 : 0x00)
				| (APU->Triangle.Length > 0           ? 0x04 : 0x00)
				| (APU->Noise.Length > 0              ? 0x08 : 0x00)
				| (APU->DMC.SampleTransferCounter > 0 ? 0x10 : 0x00)
				| (APU->FrameInterrupt                ? 0x40 : 0x00)
				| (APU->DMC.Interrupt                 ? 0x80 : 0x00)
				;

			// Reading $4015 clears the frame interrupt flag,
			// unless the flag was set during this cycle.
			if (APU->FrameCycle != APU->FrameInterruptCycle)
				APU->FrameInterrupt = false;

			return Data;
		}
	}

	return 0;
}

void WriteAPU(machine* M, u16 Address, u8 Data)
{
	apu* APU = &M->APU;

	switch (Address) {
		// Pulse 1 & 2.
		case 0x4000: case 0x4004: {
			apu_pulse* P = &APU->Pulse[(Address >> 2) & 1];
			P->SequenceMode          =  (Data >> 6) & 0x03;
			P->LengthEnable          = ~(Data >> 5) & 0x01;
			P->EnvelopeLoop          =  (Data >> 5) & 0x01;
			P->EnvelopeEnable        = ~(Data >> 4) & 0x01;
			P->EnvelopeDividerPeriod =   Data       & 0x0F;
			P->ConstantVolume        =   Data       & 0x0F;
			break;
		}
		case 0x4001: case 0x4005: {
			apu_pulse* P = &APU->Pulse[(Address >> 2) & 1];
			P->SweepEnable        = (Data >> 7) & 0x01;
			P->SweepDividerPeriod = (Data >> 4) & 0x07;
			P->SweepNegate        = (Data >> 3) & 0x01;
			P->SweepShift         =  Data       & 0x07;
			P->SweepDividerReset = true;
			break;
		}
		case 0x4002: case 0x4006: {
			apu_pulse* P = &APU->Pulse[(Address >> 2) & 1];
			P->TimerPeriod = (P->TimerPeriod & 0xFF00) | Data;
			break;
		}
		case 0x4003: case 0x4007: {
			apu_pulse* P = &APU->Pulse[(Address >> 2) & 1];
			P->TimerPeriod = (P->TimerPeriod & 0x00FF) | u16(Data & 0x07) << 8;
			// Length counter value can only be changed if the unit is enabled.
			if (P->Enable) P->Length = APULengthTable[Data >> 3];
			P->EnvelopeReset = true;
			P->SequenceTime = 0;
			break;
		}
		// Triangle.
		case 0x4008: {
			apu_triangle* T = &APU->Triangle;
			T->LengthEnable       = ~(Data >> 7) & 0x01;
			T->CounterLoadControl =  (Data >> 7) & 0x01;
			T->CounterPreset      =   Data       & 0x7F;
			break;
		}
		case 0x400A: {
			apu_triangle* T = &APU->Triangle;
			T->TimerPeriod = (T->TimerPeriod & 0xFF00) | Data;
			break;
		}
		case 0x400B: {
			apu_triangle* T = &APU->Triangle;
			T->TimerPeriod = (T->TimerPeriod & 0x00FF) | u16(Data & 0x07) << 8;
			T->CounterLoad = true;
			// Length counter value can only be changed if the unit is enabled.
			if (T->Enable) T->Length = APULengthTable[Data >> 3];
			break;
		}
		// Noise.
		case 0x400C: {
			apu_noise* N = &APU->Noise;
			N->LengthEnable          = ~(Data >> 5) & 0x01;
			N->EnvelopeLoop          =  (Data >> 5) & 0x01;
			N->EnvelopeEnable        = ~(Data >> 4) & 0x01;
			N->EnvelopeDividerPeriod =  Data        & 0x0F;
			N->ConstantVolume        =  Data        & 0x0F;
			break;
		}
		case 0x400E: {
			apu_noise* N = &APU->Noise;
			N->NoiseMode = (Data >> 7) & 1;
			N->TimerPeriod = APUNoiseTimerPeriodTable[Data & 0x0F];
			break;
		}
		case 0x400F: {
			apu_noise* N = &APU->Noise;
			// Length counter value can only be changed if the unit is enabled.
			if (N->Enable) N->Length = APULengthTable[Data >> 3];
			N->EnvelopeReset = true;
			break;
		}
		// DMC.
		case 0x4010: {
			apu_dmc* D = &APU->DMC;
			D->InterruptEnable = (Data >> 7) & 1;
			D->SampleLoop      = (Data >> 6) & 1;
			D->TimerPeriod     = APUDMCTimerPeriodTable[Data & 0x0F];
			if (!D->InterruptEnable) D->Interrupt = false;
			break;
		}
		case 0x4011: {
			APU->DMC.Output = Data & 0x7F;
			break;
		}
		case 0x4012: {
			APU->DMC.SampleAddress = 0xC000 + 64 * Data;
			break;
		}
		case 0x4013: {
			APU->DMC.SampleLength = 16 * Data + 1;
			break;
		}
		// Status.
		case 0x4015: {
			APU->Pulse[0].Enable = (Data >> 0) & 1;
			APU->Pulse[1].Enable = (Data >> 1) & 1;
			APU->Triangle.Enable = (Data >> 2) & 1;
			APU->Noise.Enable    = (Data >> 3) & 1;
			APU->DMC.Enable      = (Data >> 4) & 1;

			if (!APU->Pulse[0].Enable) APU->Pulse[0].Length = 0;
			if (!APU->Pulse[1].Enable) APU->Pulse[1].Length = 0;
			if (!APU->Triangle.Enable) APU->Triangle.Length = 0;
			if (!APU->Noise.Enable)    APU->Noise.Length = 0;

			break;
		}
		// Frame counter.
		case 0x4017: {
			APU->FrameCounterMode      = (Data >> 7) & 0x01;
			APU->FrameInterruptDisable = (Data >> 6) & 0x01;
			APU->FrameCycleResetTimer  = (APU->FrameCycle % 2 == 0) ? 4 : 3;

			if (APU->FrameInterruptDisable)
				APU->FrameInterrupt = false;
			break;
		}
	}
}

static inline u16 SweepTargetPeriod(apu_pulse* P, bool NegateOnesComplement)
{
	// Compute sweep unit target time period and muting flag.
	i32 Period = P->TimerPeriod;
	i32 PeriodDelta = Period >> P->SweepShift;

	if (P->SweepNegate)
		Period -= PeriodDelta + (NegateOnesComplement ? 1 : 0);
	else
		Period += PeriodDelta;

	return Period < 0 ? 0 : u16(Period);
}

void StepAPU(machine* M)
{
	apu* APU = &M->APU;

	APU->Cycle += 1;
	APU->FrameCycle += 1;

	// Generate timing signals.
	bool IsQuarterFrameCycle = false;
	bool IsHalfFrameCycle = false;
	bool IsInterruptCycle = false;

	// Handle delayed reset of FrameCycle from write to $4017.
	if (APU->FrameCycleResetTimer > 0) {
		APU->FrameCycleResetTimer -= 1;
		if (APU->FrameCycleResetTimer == 0) {
			APU->FrameCycle = 0;
			// In mode 1 (5-step sequence), quarter- and half-frame cycles
			// occur immediately after the frame cycle is reset.
			if (APU->FrameCounterMode) {
				IsHalfFrameCycle = true;
				IsQuarterFrameCycle = true;
			}
		}
	}

	if (APU->FrameCounterMode) {
		// Mode 1: 5-step sequence.
		if (APU->FrameCycle == 7457 || APU->FrameCycle == 22371)
			IsQuarterFrameCycle = true;
		if (APU->FrameCycle == 14913 || APU->FrameCycle == 37281)
			IsQuarterFrameCycle = IsHalfFrameCycle = true;
		if (APU->FrameCycle == 37282)
			APU->FrameCycle = 0;
	}
	else {
		// Mode 0: 4-step sequence.
		if (APU->FrameCycle == 7457 || APU->FrameCycle == 22371)
			IsQuarterFrameCycle = true;
		if (APU->FrameCycle == 14913 || APU->FrameCycle == 29829)
			IsQuarterFrameCycle = IsHalfFrameCycle = true;
		if (APU->FrameCycle >= 29828 && APU->FrameCycle <= 29830)
			IsInterruptCycle = true;
		if (APU->FrameCycle == 29830)
			APU->FrameCycle = 0;
	}

	if (APU->FrameCycle == 0)
		APU->Frame += 1;

	if (IsInterruptCycle && !APU->FrameInterruptDisable) {
		APU->FrameInterruptCycle = APU->FrameCycle;
		APU->FrameInterrupt = true;
	}

	// Update pulse channels.
	for (i32 I = 0; I < 2; I++) {
		apu_pulse* P = &APU->Pulse[I];

		if (IsQuarterFrameCycle) {
			// Update the envelope generator.
			if (P->EnvelopeReset) {
				// Reset flag set, reset the divider and envelope volume.
				P->EnvelopeReset = false;
				P->EnvelopeVolume = 15;
				P->EnvelopeDividerCount = P->EnvelopeDividerPeriod;
			}
			else if (P->EnvelopeDividerCount > 0) {
				// Clock the divider.
				P->EnvelopeDividerCount -= 1;
			}
			else if (P->EnvelopeVolume > 0) {
				// Divider wrap, clock the volume level.
				P->EnvelopeDividerCount = P->EnvelopeDividerPeriod;
				P->EnvelopeVolume -= 1;
			}
			else if (P->EnvelopeLoop) {
				// Divider wrap and the envelope finished,
				// but the loop flag is set, so start over.
				P->EnvelopeDividerCount = P->EnvelopeDividerPeriod;
				P->EnvelopeVolume = 15;
			}
		}

		if (IsHalfFrameCycle) {
			// Clock the sweep unit.
			u16 TargetPeriod = SweepTargetPeriod(P, I == 0);

			// To update the period, the sweep unit must be enabled,
			// the period shift must be non-zero, and the sweep unit
			// must not be muting the channel.
			bool CanUpdatePeriod =
				P->SweepEnable &&
				P->SweepShift > 0 &&
				P->TimerPeriod >= 8 && TargetPeriod < 0x800;

			// If the divider is at zero, update the timer period.
			if (P->SweepDividerCount == 0 && CanUpdatePeriod)
				P->TimerPeriod = TargetPeriod;

			// Tick sweep divider.
			if (P->SweepDividerCount == 0 || P->SweepDividerReset) {
				P->SweepDividerCount = P->SweepDividerPeriod;
				P->SweepDividerReset = false;
			}
			else {
				P->SweepDividerCount -= 1;
			}

			// Length counter clocking.
			if (P->LengthEnable && P->Length > 0)
				P->Length -= 1;
		}

		// Clock the timer at half the CPU clock rate.
		if (APU->FrameCycle % 2 == 0) {
			if (P->Timer == 0) {
				// Advance the waveform sequencer.
				P->SequenceTime = (P->SequenceTime + 1) % 8;
				// Reset timer count.
				P->Timer = P->TimerPeriod;
			}
			else {
				P->Timer -= 1;
			}
		}
	}

	// Update triangle channel.
	{
		apu_triangle* T = &APU->Triangle;

		if (IsQuarterFrameCycle) {
			// Update the linear counter.
			if (T->CounterLoad)
				T->Counter = T->CounterPreset;
			else if (T->Counter > 0)
				T->Counter -= 1;

			if (!T->CounterLoadControl)
				T->CounterLoad = false;
		}

		if (IsHalfFrameCycle) {
			// Length counter clocking.
			if (T->LengthEnable && T->Length > 0)
				T->Length -= 1;
		}

		// Clock the timer at CPU clock rate.
		if (T->Timer == 0) {
			// Advance the sequencer if length and linear counters are both nonzero.
			if (T->Length > 0 && T->Counter > 0)
				T->SequenceTime = (T->SequenceTime + 1) % 32;

			T->Timer = T->TimerPeriod;
		}
		else {
			T->Timer -= 1;
		}
	}

	// Update noise channel.
	{
		apu_noise* N = &APU->Noise;

		if (IsQuarterFrameCycle) {
			// Update the envelope generator.
			if (N->EnvelopeReset) {
				// Reset flag set, reset the divider and envelope volume.
				N->EnvelopeReset = false;
				N->EnvelopeVolume = 15;
				N->EnvelopeDividerCount = N->EnvelopeDividerPeriod;
			}
			else if (N->EnvelopeDividerCount > 0) {
				// Clock the divider.
				N->EnvelopeDividerCount -= 1;
			}
			else if (N->EnvelopeVolume > 0) {
				// Divider wrap, decrement the volume level.
				N->EnvelopeDividerCount = N->EnvelopeDividerPeriod;
				N->EnvelopeVolume -= 1;
			}
			else if (N->EnvelopeLoop) {
				// Divider wrap and the envelope is finished,
				// but the loop flag is set, so start over.
				N->EnvelopeDividerCount = N->EnvelopeDividerPeriod;
				N->EnvelopeVolume = 15;
			}
		}

		if (IsHalfFrameCycle) {
			// Length counter clocking.
			if (N->LengthEnable && N->Length > 0)
				N->Length -= 1;
		}

		// Clock the timer at half the CPU clock rate.
		if (APU->FrameCycle % 2 == 0) {
			if (N->Timer == 0) {
				u16 R = N->NoiseRegister;
				u16 S = N->NoiseMode ? (R >> 6) : (R >> 1);
				u16 F = (R ^ S) & 1;
				N->NoiseRegister = (R >> 1) | (F << 14);

				N->Timer = N->TimerPeriod;
			}
			else {
				N->Timer -= 1;
			}
		}
	}

	// Update DMC channel.
	{
		apu_dmc* D = &APU->DMC;

		// Handle sample DMA.
		if (D->SampleBufferEmpty && D->SampleTransferCounter > 0) {
			M->CPU.Stall += 4;
			
			D->SampleBuffer = Read(M, D->SampleTransferPointer);
			D->SampleBufferEmpty = false;
			D->SampleTransferPointer = (D->SampleTransferPointer + 1) | 0x8000;
			D->SampleTransferCounter -= 1;

			if (D->SampleTransferCounter == 0) {
				if (D->SampleLoop) {
					D->SampleTransferPointer = D->SampleAddress;
					D->SampleTransferCounter = D->SampleLength;
				}
				else if (D->InterruptEnable) {
					D->Interrupt = true;
				}
			}
		}

		// Clock the timer at half the CPU clock rate.
		if (APU->FrameCycle % 2 == 0) {
			if (D->Timer == 0) {
				// Delta modulate output level based output register LSB.
				if (D->OutputEnable) {
					if (D->OutputRegister & 1) {
						if (D->Output <= 125)
							D->Output += 2;
					}
					else {
						if (D->Output >= 2)
							D->Output -= 2;
					}
				}

				// Clock the output shift register.
				D->OutputRegister >>= 1;
				D->OutputTime = (D->OutputTime + 1) % 8;

				if (D->OutputTime == 0) {
					// Output cycle finished, get next sample from sample buffer.
					D->OutputRegister = D->SampleBuffer;
					D->OutputEnable = !D->SampleBufferEmpty;
					D->SampleBufferEmpty = true;
				}

				D->Timer = D->TimerPeriod;
			}
			else {
				D->Timer -= 1;
			}
		}
	}

	APU->AudioSampleCount += APU->AudioSampleRate / 1789773.0;

	while (APU->AudioSampleCount >= 1.0) {
		// Channel values.
		u32 PValue = 0;
		u32 TValue = 0;
		u32 NValue = 0;
		u32 DValue = 0;

		for (i32 I = 0; I < 2; I++) {
			apu_pulse* P = &APU->Pulse[I];

			bool IsMutedBySweep = P->TimerPeriod < 8 || SweepTargetPeriod(P, I == 0) > 0x7FF;
			bool IsMutedBySequencer = APUPulseSequenceTable[P->SequenceMode][P->SequenceTime] == 0;

			if (P->Enable && P->Length > 0 && !IsMutedBySweep && !IsMutedBySequencer)
				PValue += P->EnvelopeEnable ? P->EnvelopeVolume : P->ConstantVolume;
		}

		apu_triangle* T = &APU->Triangle;
		if (T->Enable && T->Length > 0 && T->Counter > 0)
			TValue = APUTriangleSequenceTable[T->SequenceTime];

		apu_noise* N = &APU->Noise;
		if (N->Enable && N->Length > 0 && (N->NoiseRegister & 1) == 0)
			NValue = N->EnvelopeEnable ? N->EnvelopeVolume : N->ConstantVolume;

		apu_dmc* D = &APU->DMC;
		if (D->Enable && D->OutputEnable)
			DValue = D->Output;

		// Generate sample.
		f64 Output = 0.0;

		if (PValue > 0) {
			Output += 95.88 / (100.0 + 8128.0 / PValue);
		}

		if (TValue > 0 || NValue > 0 || DValue > 0) {
			f64 D = TValue / 8227.0 + NValue / 12241.0 + DValue / 22638.0;
			Output += 159.79 / (100.0 + 1.0 / D);
		}

		if (Output > 1.0) {
			Output = 1.0;
		}

		f64 Alpha = 1.0;
		APU->AudioSample = Alpha * Output + (1 - Alpha) * APU->AudioSample;

		if (APU->AudioPointer < 8192) {
			APU->AudioBuffer[APU->AudioPointer] = u8(APU->AudioSample * 255);
			APU->AudioPointer += 1;
		}

		APU->AudioSampleCount -= 1.0;
	}
}
