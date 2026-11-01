/**
 *    Name:        main.c
 *    Description: SDL2 window, keyboard controls and the shared-board front end
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 *
 *    The port and the headless runner call the same ddp_program_run/ddp_program_frame_wait path;
 *    only this file knows about SDL.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>

#include "cave/machine.h"
#include "cave/program.h"
#include "cave/video.h"
#include "ddp/rom.h"

#define WINDOW_SCALE 2

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_GameController *controller;
static int running = 1;
static uint32_t display_pixels[DDP_SCREEN_HEIGHT * DDP_SCREEN_WIDTH];
static uint64_t perf_frequency;
static const char *screenshot_path;
static int screenshot_frame = 120;
static int frame_number;
static int fast_mode;
static int boot_fast = 1;
static SDL_AudioDeviceID audio_device;
static int audio_rate = 44100;
static double audio_carry;
static int16_t audio_buffer[4096 * 2];
static const char *audio_wav_path;
static const char *audio_device_name;
static FILE *audio_wav_file;
static uint32_t audio_wav_frames;
static int list_audio;
static int stats;
static int exit_after;
static int auto_start;
static double next_frame_deadline;
static double frame_times[600];
static int frame_time_count;

static void set_inputs(cave_machine *machine)
{
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	uint16_t in0 = 0xFFFFu;
	uint16_t in1 = 0xFFFFu;

	if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) in0 &= (uint16_t)~0x0001u;
	if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) in0 &= (uint16_t)~0x0002u;
	if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) in0 &= (uint16_t)~0x0004u;
	if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) in0 &= (uint16_t)~0x0008u;
	if (keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_J]) in0 &= (uint16_t)~0x0010u;
	if (keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_K]) in0 &= (uint16_t)~0x0020u;
	if (keys[SDL_SCANCODE_C] || keys[SDL_SCANCODE_L]) in0 &= (uint16_t)~0x0040u;
	if (keys[SDL_SCANCODE_RETURN]) in0 &= (uint16_t)~0x0080u;
	if (keys[SDL_SCANCODE_5]) in0 &= (uint16_t)~0x0100u;
	if (keys[SDL_SCANCODE_1]) in0 &= (uint16_t)~0x0200u; /* service */

	if (keys[SDL_SCANCODE_I]) in1 &= (uint16_t)~0x0001u;
	if (keys[SDL_SCANCODE_K]) in1 &= (uint16_t)~0x0002u;
	if (keys[SDL_SCANCODE_J]) in1 &= (uint16_t)~0x0004u;
	if (keys[SDL_SCANCODE_L]) in1 &= (uint16_t)~0x0008u;
	if (keys[SDL_SCANCODE_N]) in1 &= (uint16_t)~0x0010u;
	if (keys[SDL_SCANCODE_M]) in1 &= (uint16_t)~0x0020u;
	if (keys[SDL_SCANCODE_COMMA]) in1 &= (uint16_t)~0x0040u;
	if (keys[SDL_SCANCODE_2]) in1 &= (uint16_t)~0x0080u;
	if (keys[SDL_SCANCODE_6]) in1 &= (uint16_t)~0x0100u;

	if (controller)
	{
		Sint16 axis_x = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
		Sint16 axis_y = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
		const int deadzone = 12000;

		if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP) || axis_y < -deadzone)
			in0 &= (uint16_t)~0x0001u;
		if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || axis_y > deadzone)
			in0 &= (uint16_t)~0x0002u;
		if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || axis_x < -deadzone)
			in0 &= (uint16_t)~0x0004u;
		if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || axis_x > deadzone)
			in0 &= (uint16_t)~0x0008u;
		if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A))
			in0 &= (uint16_t)~0x0010u; /* shot */
		if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B))
			in0 &= (uint16_t)~0x0020u; /* bomb */
		if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X))
			in0 &= (uint16_t)~0x0040u; /* auto */
		if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START) ||
		    SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
			in0 &= (uint16_t)~0x0080u;
		if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_BACK) ||
		    SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
			in0 &= (uint16_t)~0x0100u;
	}

	if (auto_start)
	{
		if (frame_number >= 3800 && frame_number < 3810) in0 &= (uint16_t)~0x0100u;
		if (frame_number >= 3860 && frame_number < 3875) in0 &= (uint16_t)~0x0080u;
		if (frame_number >= 3900) in0 &= (uint16_t)~0x0010u;
		if (((frame_number / 120) & 1u) != 0) in0 &= (uint16_t)~0x0004u;
		else in0 &= (uint16_t)~0x0008u;
	}

	if (in0 != 0xFFFFu || in1 != 0xFFFFu)
		boot_fast = 0;                  /* the player is interacting: show every frame */
	cave_machine_set_inputs(machine, in0, in1);
}

static void rotate_to_display(const uint32_t *source, uint32_t *target)
{
	int x, y;
	for (y = 0; y < DDP_SCREEN_WIDTH; ++y)
	{
		for (x = 0; x < DDP_SCREEN_HEIGHT; ++x)
		{
			/* ROT270: the 320x240 board picture is shown as 240x320. The texture is
			   ARGB8888, so the alpha byte must be opaque or SDL draws nothing. */
			target[y * DDP_SCREEN_HEIGHT + x] =
				0xFF000000u | source[x * DDP_SCREEN_WIDTH + (DDP_SCREEN_WIDTH - 1 - y)];
		}
	}
}

static int picture_ready(const cave_machine *machine)
{
	uint32_t background = machine->framebuffer[0];
	unsigned changed = 0;
	size_t i;
	for (i = 0; i < DDP_FRAMEBUFFER_PIXELS; ++i)
	{
		if (machine->framebuffer[i] != background && ++changed > 1000)
			return 1;
	}
	return 0;
}

static void write_audio_wav_header(FILE *file, uint32_t frames)
{
	uint32_t rate = (uint32_t)audio_rate;
	uint32_t data_size = frames * 2u * 2u;
	uint32_t riff_size = 36u + data_size;
	uint8_t header[44];
	uint32_t value;

	memcpy(header + 0, "RIFF", 4);
	value = riff_size; memcpy(header + 4, &value, 4);
	memcpy(header + 8, "WAVEfmt ", 8);
	value = 16; memcpy(header + 16, &value, 4);
	value = 1; memcpy(header + 20, &value, 2);
	value = 2; memcpy(header + 22, &value, 2);
	memcpy(header + 24, &rate, 4);
	value = rate * 4u; memcpy(header + 28, &value, 4);
	value = 4; memcpy(header + 32, &value, 2);
	value = 16; memcpy(header + 34, &value, 2);
	memcpy(header + 36, "data", 4);
	memcpy(header + 40, &data_size, 4);
	fseek(file, 0, SEEK_SET);
	fwrite(header, 1, sizeof(header), file);
}

static int compare_double(const void *a, const void *b)
{
	double left = *(const double *)a;
	double right = *(const double *)b;
	return (left > right) - (left < right);
}

static void report_frame_stats(uint64_t frame_start, uint64_t frame_end)
{
	double milliseconds = (double)(frame_end - frame_start) * 1000.0 / (double)perf_frequency;
	double sorted[600];
	double sum = 0.0;
	double max = 0.0;
	int count;
	int i;

	frame_times[frame_time_count++ % 600] = milliseconds;
	if (!stats || frame_time_count % 300 != 0)
		return;
	count = frame_time_count < 600 ? frame_time_count : 600;
	memcpy(sorted, frame_times, (size_t)count * sizeof(double));
	qsort(sorted, (size_t)count, sizeof(double), compare_double);
	for (i = 0; i < count; ++i)
	{
		sum += sorted[i];
		if (sorted[i] > max)
			max = sorted[i];
	}
	fprintf(stderr,
	        "ddonpach: stats frame=%d avg=%.2fms (%.2ffps) p99=%.2fms max=%.2fms queue=%u\n",
	        frame_time_count, sum / count, 1000.0 * count / sum,
	        sorted[(count * 99) / 100], max,
	        audio_device ? (unsigned)SDL_GetQueuedAudioSize(audio_device) : 0u);
}

static void frame_callback(cave_machine *machine)
{
	SDL_Event event;
	uint64_t frame_start = SDL_GetPerformanceCounter();
	uint64_t frame_end;

	while (SDL_PollEvent(&event))
	{
		if (event.type == SDL_QUIT)
			running = 0;
		if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
			running = 0;
		if (event.type == SDL_CONTROLLERDEVICEADDED && !controller)
		{
			controller = SDL_GameControllerOpen(event.cdevice.which);
			if (controller)
				fprintf(stderr, "ddonpach: controller %s\n",
				        SDL_GameControllerName(controller));
		}
		if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller)
		{
			SDL_Joystick *joystick = SDL_GameControllerGetJoystick(controller);
			if (joystick && event.cdevice.which == SDL_JoystickInstanceID(joystick))
			{
				SDL_GameControllerClose(controller);
				controller = NULL;
				fprintf(stderr, "ddonpach: controller removed\n");
			}
		}
	}
	if (!running)
	{
		ddp_program_request_stop();
		return;
	}

	set_inputs(machine);
	cave_video_render(machine, machine->framebuffer);
	rotate_to_display(machine->framebuffer, display_pixels);

	frame_number++;
	if (exit_after > 0 && frame_number >= exit_after)
		running = 0;

	/* The board's early boot/attract phase is black (and later solid white) for several
	   thousand frames. Run it at full speed until the first picture with actual content,
	   then present normally. Any input cancels the fast-forward. */
	if (boot_fast && !screenshot_path)
	{
		if (frame_number < 12000 && !picture_ready(machine))
			return;
		boot_fast = 0;
	}

	SDL_UpdateTexture(texture, NULL, display_pixels,
	                  DDP_SCREEN_HEIGHT * (int)sizeof(uint32_t));
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);

	if (screenshot_path && frame_number == screenshot_frame)
	{
		int width = 0, height = 0;
		void *pixels;
		SDL_Surface *surface;
		SDL_GetRendererOutputSize(renderer, &width, &height);
		pixels = malloc((size_t)width * (size_t)height * 4u);
		if (pixels &&
		    SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
		                         pixels, width * 4) == 0)
		{
			surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels, width, height, 32,
			                                             width * 4, SDL_PIXELFORMAT_ARGB8888);
			if (surface)
			{
				int saved = SDL_SaveBMP(surface, screenshot_path);
				fprintf(stderr, "ddonpach: screenshot frame %d -> %s (%d)\n",
				        frame_number, screenshot_path, saved);
				SDL_FreeSurface(surface);
			}
		}
		free(pixels);
	}

	SDL_RenderPresent(renderer);

	/* Queue one video frame's worth of YMZ280B output. Keep the queue near real time so a
	   slow or fast host does not build up seconds of latency. */
	if (audio_device && !boot_fast && !fast_mode && !screenshot_path &&
	    SDL_GetQueuedAudioSize(audio_device) < (Uint32)audio_rate)
	{
		double needed = (double)audio_rate / 57.55 + audio_carry;
		int frames = (int)needed;
		audio_carry = needed - (double)frames;
		if (frames > (int)(sizeof(audio_buffer) / (2 * sizeof(int16_t))))
			frames = (int)(sizeof(audio_buffer) / (2 * sizeof(int16_t)));
		if (frames > 0)
		{
			static int reported;
			cave_ymz280b_render(&machine->ymz, audio_buffer, (size_t)frames);
			if (audio_wav_file)
			{
				fwrite(audio_buffer, sizeof(int16_t), (size_t)frames * 2u, audio_wav_file);
				audio_wav_frames += (uint32_t)frames;
			}
			SDL_QueueAudio(audio_device, audio_buffer,
			               (Uint32)(frames * 2 * (int)sizeof(int16_t)));
			if (!reported)
			{
				reported = 1;
				fprintf(stderr, "ddonpach: audio queue started (%d frames)\n", frames);
			}
		}
	}

	/* Preserve the board's 57.55 Hz with an absolute high-resolution deadline. Do not wait on
	   vsync as well: doing both halves the actual frame rate. The deadline is not restarted
	   from each frame, so a late frame does not permanently lower the average rate. */
	if (!fast_mode)
	{
		double ticks_per_frame = (double)perf_frequency / 57.55;
		double now = (double)SDL_GetPerformanceCounter();
		if (next_frame_deadline == 0.0 ||
		    now - next_frame_deadline > ticks_per_frame * 2.0)
			next_frame_deadline = now + ticks_per_frame;
		while ((double)SDL_GetPerformanceCounter() < next_frame_deadline)
		{
			double remain = next_frame_deadline - (double)SDL_GetPerformanceCounter();
			if (remain > ticks_per_frame * 0.25)
				SDL_Delay(1);
		}
		next_frame_deadline += ticks_per_frame;
	}
	frame_end = SDL_GetPerformanceCounter();
	report_frame_stats(frame_start, frame_end);
}

int main(int argc, char **argv)
{
	const char *rom_path = "rom/ddonpach.zip";
	char error[512];
	ddp_rom_set rom;
	int i;

	for (i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc)
			rom_path = argv[++i];
		else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc)
			screenshot_path = argv[++i];
		else if (strcmp(argv[i], "--screenshot-frame") == 0 && i + 1 < argc)
			screenshot_frame = atoi(argv[++i]);
		else if (strcmp(argv[i], "--fast") == 0)
			fast_mode = 1;
		else if (strcmp(argv[i], "--no-fast-boot") == 0)
			boot_fast = 0;
		else if (strcmp(argv[i], "--stats") == 0)
			stats = 1;
		else if (strcmp(argv[i], "--exit-after") == 0 && i + 1 < argc)
			exit_after = atoi(argv[++i]);
		else if (strcmp(argv[i], "--auto-start") == 0)
			auto_start = 1;
		else if (strcmp(argv[i], "--audio-wav") == 0 && i + 1 < argc)
		{
			uint8_t placeholder[44] = { 0 };
			audio_wav_path = argv[++i];
			audio_wav_file = fopen(audio_wav_path, "wb");
			if (audio_wav_file)
				fwrite(placeholder, 1, sizeof(placeholder), audio_wav_file);
		}
		else if (strcmp(argv[i], "--audio-device") == 0 && i + 1 < argc)
			audio_device_name = argv[++i];
		else if (strcmp(argv[i], "--list-audio") == 0)
			list_audio = 1;
	}

	SDL_SetMainReady();
	perf_frequency = SDL_GetPerformanceFrequency();
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO |
	             SDL_INIT_GAMECONTROLLER) != 0)
	{
		fprintf(stderr, "ddonpach: SDL_Init: %s\n", SDL_GetError());
		return 1;
	}

	if (list_audio)
	{
		int devices = SDL_GetNumAudioDevices(0);
		int device;
		printf("audio driver: %s\n", SDL_GetCurrentAudioDriver());
		for (device = 0; device < devices; ++device)
			printf("%d: %s\n", device, SDL_GetAudioDeviceName(device, 0));
		SDL_Quit();
		return 0;
	}

	{
		int joystick_count = SDL_NumJoysticks();
		int joystick;
		for (joystick = 0; joystick < joystick_count; ++joystick)
		{
			if (SDL_IsGameController(joystick))
			{
				controller = SDL_GameControllerOpen(joystick);
				if (controller)
				{
					fprintf(stderr, "ddonpach: controller %s\n",
					        SDL_GameControllerName(controller));
					break;
				}
			}
		}
	}

	memset(&rom, 0, sizeof(rom));
	if (ddp_rom_load(rom_path, &rom, error, sizeof(error)) != 0)
	{
		fprintf(stderr, "ddonpach: %s\n", error);
		SDL_Quit();
		return 2;
	}

	window = SDL_CreateWindow("DoDonPachi",
	                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                          DDP_SCREEN_HEIGHT * WINDOW_SCALE,
	                          DDP_SCREEN_WIDTH * WINDOW_SCALE,
	                          SDL_WINDOW_SHOWN);
	if (!window)
	{
		fprintf(stderr, "ddonpach: SDL_CreateWindow: %s\n", SDL_GetError());
		ddp_rom_free(&rom);
		SDL_Quit();
		return 1;
	}

	/* No PRESENTVSYNC: the board runs at 57.55 Hz, and waiting for a 60 Hz vsync plus a
	   frame delay would halve the effective rate. The high-resolution limiter owns pacing. */
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer)
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (!renderer)
	{
		fprintf(stderr, "ddonpach: SDL_CreateRenderer: %s\n", SDL_GetError());
		SDL_DestroyWindow(window);
		ddp_rom_free(&rom);
		SDL_Quit();
		return 1;
	}

	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
	                            SDL_TEXTUREACCESS_STREAMING,
	                            DDP_SCREEN_HEIGHT, DDP_SCREEN_WIDTH);
	if (!texture)
	{
		fprintf(stderr, "ddonpach: SDL_CreateTexture: %s\n", SDL_GetError());
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		ddp_rom_free(&rom);
		SDL_Quit();
		return 1;
	}

	cave_machine_init(&ddp_machine, &rom);
	cave_machine_set_inputs(&ddp_machine, 0xFFFFu, 0xFFFFu);

	{
		SDL_AudioSpec wanted;
		SDL_AudioSpec obtained;
		SDL_zero(wanted);
		wanted.freq = 44100;
		wanted.format = AUDIO_S16SYS;
		wanted.channels = 2;
		wanted.samples = 1024;
		audio_device = SDL_OpenAudioDevice(audio_device_name, 0, &wanted, &obtained, 0);
		if (!audio_device && audio_device_name)
		{
			fprintf(stderr, "ddonpach: audio device '%s': %s; using the default device\n",
			        audio_device_name, SDL_GetError());
			audio_device_name = NULL;
			audio_device = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
		}
		if (audio_device)
		{
			const char *audio_driver = SDL_GetCurrentAudioDriver();
			const char *audio_name = audio_device_name;
			if (!audio_name && SDL_GetNumAudioDevices(0) > 0)
				audio_name = SDL_GetAudioDeviceName(0, 0);
			audio_rate = obtained.freq;
			ddp_machine.ymz.sample_rate = audio_rate;
			SDL_PauseAudioDevice(audio_device, 0);
			fprintf(stderr, "ddonpach: audio driver %s, device %s, %d Hz, %d channels\n",
			        audio_driver ? audio_driver : "unknown",
			        audio_name ? audio_name : "default",
			        obtained.freq, obtained.channels);
			if (audio_name &&
			    (strstr(audio_name, "Streaming") || strstr(audio_name, "Virtual")))
				fprintf(stderr,
				        "ddonpach: warning: %s looks like a streaming/virtual endpoint; "
				        "use --list-audio and --audio-device for physical speakers\n",
				        audio_name);
		}
		else
			fprintf(stderr, "ddonpach: audio device: %s\n", SDL_GetError());
	}

	ddp_program_set_frame_callback(frame_callback);
	ddp_program_run(&ddp_machine, 0);

	if (ddp_program_faulted())
		fprintf(stderr, "ddonpach: %s\n", ddp_program_fault_message());

	if (audio_wav_file)
	{
		write_audio_wav_header(audio_wav_file, audio_wav_frames);
		fclose(audio_wav_file);
		audio_wav_file = NULL;
	}
	if (audio_device)
		SDL_CloseAudioDevice(audio_device);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	if (controller)
	{
		SDL_GameControllerClose(controller);
		controller = NULL;
	}
	ddp_rom_free(&rom);
	SDL_Quit();
	return ddp_program_faulted() ? 1 : 0;
}
