#define _CRT_SECURE_NO_WARNINGS
#include <cstdint>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <nfd.h>

#include "nes.h"

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 960

struct tas_frame
{
	bool    Reset;
	u8      Buttons[2];
};

static bool OpenFileDialog(nfdu8char_t** Path)
{
};

i32 ReadTASFile(const char* Path, tas_frame** OutFrameData, i32* OutFrameCount)
{
	FILE* File = fopen(Path, "rt");
	if (!File) return -1;

	i32 FrameCount = 0;
	while (!feof(File)) {
		if (fgetc(File) == '\n')
			FrameCount += 1;
	}
	fseek(File, 0, SEEK_SET);

	tas_frame* FrameData = (tas_frame*)calloc(FrameCount ,sizeof(tas_frame));

	for (i32 I = 0; I < FrameCount; I++) {
		int Reset = 0;
		char Buttons[2][9] = {};

		if (fscanf(File, "|%d|%8s||| ", &Reset, Buttons[0]) == 0) {
			free(FrameData);
			fclose(File);
			return -1;
		}

		tas_frame* F = &FrameData[I];

		F->Reset = Reset;
		for (i32 J = 0; J < 2; J++)
			for (i32 K = 0; K < 8; K++)
				if (Buttons[J][K] != '.')
					F->Buttons[J] |= 0x80 >> K;
	}

	*OutFrameData = FrameData;
	*OutFrameCount = FrameCount;

	fclose(File);
	return 0;
}

int main(int argc, char* args[])
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		return -1;
	}

	// Init video.
	SDL_Window* Window = SDL_CreateWindow("NES Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	assert(Window);
	SDL_Surface* Surface = SDL_GetWindowSurface(Window);
	assert(Surface);

	// Init audio.
	SDL_AudioSpec AudioSpec;
	AudioSpec.freq = 44100;
	AudioSpec.format = AUDIO_U8;
	AudioSpec.channels = 1;
	AudioSpec.samples = 1024;
	AudioSpec.callback = nullptr;
	AudioSpec.userdata = nullptr;

	SDL_AudioSpec ObtainedAudioSpec;
	SDL_AudioDeviceID AudioDeviceID = SDL_OpenAudioDevice(nullptr, 0, &AudioSpec, &ObtainedAudioSpec, 0);
	SDL_PauseAudioDevice(AudioDeviceID, 0);

	// Init NES.
	machine M;
	memset(&M, 0, sizeof(machine));

	u64 CurrentFrame = 0;
	u64 PreviousTime = SDL_GetTicks64();
	double TimeToNextFrame = 0.0;

	f64 QueuedAudioSize = 0.0;

	bool Exit = false;
	bool Paused = false;
	bool FrameSteppingMode = false;

	while (!Exit) {

		SDL_Event Event;
		while (SDL_PollEvent(&Event)) {
			if (Event.type == SDL_QUIT) {
				Exit = true;
				break;
			}
			// F1: Open ROM file.
			if (Event.type == SDL_KEYDOWN && Event.key.keysym.scancode == SDL_SCANCODE_F1) {
				nfdu8filteritem_t Filter = { "INES ROM", "nes" };
				nfdu8char_t* Path = nullptr;
				if (NFD_OpenDialogU8(&Path, &Filter, 1, nullptr) == NFD_OKAY) {
					i64 Result = Load(M, Path);

					if (Result >= 0) {
						char Title[1024];
						snprintf(Title, 1024, "NES Emulator - %s (Mapper %d)\n", Path, M.Mapper.ID);
						SDL_SetWindowTitle(Window, Title);
					}
					else {
						SDL_SetWindowTitle(Window, "NES Emulator");
					}

					CurrentFrame = 0;
					NFD_FreePathU8(Path);
				}
			}
			// T: Open/close debug trace file.
			if (Event.type == SDL_KEYDOWN && Event.key.keysym.scancode == SDL_SCANCODE_T) {
				if (M.TraceFile) {
					printf("Closing trace file\n");
					fclose(M.TraceFile);
					M.TraceFile = nullptr;
					M.TraceLine = 0;
				}
				else {
					printf("Opening trace file\n");
					M.TraceFile = fopen("trace.txt", "wb");
					M.TraceLine = 0;
				}
			}
			// P: Pause emulator.
			if (Event.type == SDL_KEYDOWN && Event.key.keysym.scancode == SDL_SCANCODE_P) {
				Paused = !Paused;
			}
			// F: Step the emulator one frame at a time.
			if (Event.type == SDL_KEYDOWN && Event.key.keysym.scancode == SDL_SCANCODE_F) {
				FrameSteppingMode = true;
				Paused = false;
			}
			// G: Cancel frame-stepping.
			if (Event.type == SDL_KEYDOWN && Event.key.keysym.scancode == SDL_SCANCODE_G) {
				FrameSteppingMode = false;
				Paused = false;
			}
		}

		if (Exit) break;

		if (!M.IsLoaded) continue;

		if (!Paused) {
			const u8* Keys = SDL_GetKeyboardState(nullptr);
			M.Input[0] = 0x00;
			if (Keys[SDL_SCANCODE_A]     ) M.Input[0] |= ButtonB;
			if (Keys[SDL_SCANCODE_S]     ) M.Input[0] |= ButtonA;
			if (Keys[SDL_SCANCODE_RETURN]) M.Input[0] |= ButtonStart;
			if (Keys[SDL_SCANCODE_SPACE] ) M.Input[0] |= ButtonSelect;
			if (Keys[SDL_SCANCODE_UP]    ) M.Input[0] |= ButtonUp;
			if (Keys[SDL_SCANCODE_DOWN]  ) M.Input[0] |= ButtonDown;
			if (Keys[SDL_SCANCODE_LEFT]  ) M.Input[0] |= ButtonLeft;
			if (Keys[SDL_SCANCODE_RIGHT] ) M.Input[0] |= ButtonRight;

			RunUntilVerticalBlank(M);
		}

		if (FrameSteppingMode) {
			Paused = true;
		}

		// Queue audio.
		SDL_QueueAudio(AudioDeviceID, M.APU.AudioBuffer, M.APU.AudioPointer);
		M.APU.AudioPointer = 0;

		// Adjust the APU output sample rate to avoid buffer under- and overruns.
		QueuedAudioSize = 0.95 * QueuedAudioSize + 0.05 * SDL_GetQueuedAudioSize(AudioDeviceID);
		M.APU.AudioSampleRate = 44100 + (4096 - QueuedAudioSize) * 0.2;
		//printf("%5u %10.2lf %10.2lf\n", SDL_GetQueuedAudioSize(AudioDeviceID), QueuedAudioSize, M.APU.AudioSampleRate);

		// Display the finished frame buffer.
		SDL_LockSurface(Surface);
		u32* FrameBuffer = M.PPU.FrameBuffer[~M.PPU.Frame & 1];
		u32* Pixels = (u32*)Surface->pixels;
		for (i32 Y = 0; Y < 960; Y++) {
			u32* FrameBufferLine = FrameBuffer + (Y >> 2) * 256;
			for (i32 X = 0; X < 256; X++) {
				u32 Color = FrameBufferLine[X];
				*Pixels++ = Color;
				*Pixels++ = Color;
				*Pixels++ = Color;
				*Pixels++ = Color;
			}
		}
		SDL_UnlockSurface(Surface);
		SDL_UpdateWindowSurface(Window);

		TimeToNextFrame += 1000.0 / 60.0;

		while (TimeToNextFrame > 0.0) {
			// Get time elapsed since last call to SDL_GetTicks64().
			u64 CurrentTime = SDL_GetTicks64();
			i64 DeltaTime = CurrentTime - PreviousTime;
			PreviousTime = CurrentTime;

			// Subtract from time to next frame.
			TimeToNextFrame -= DeltaTime;

			// Don't try to catch up for more than 100 ms when lagging behind.
			// This is to stop the emulator from going wild if the event loop
			// was paused (for example, when the open file dialog was active).
			if (TimeToNextFrame < -100.0)
				TimeToNextFrame = -100.0;
		}

		CurrentFrame++;
	}

	SDL_DestroyWindow(Window);

	SDL_Quit();

	return 0;
}
