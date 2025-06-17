#include "pal.h"
#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

#ifdef _WIN32
#include "win32_platform.h"
#elif __LINUX__
#include "linux_x11_platform.h"
#endif

PALAPI void pal_init() {
	platform_init_sound();
	get_device_handle();
}

/*

###########################################
	       WINDOW & MONITOR STUFF
###########################################

*/

PALAPI pal_window* pal_create_window(int width, int height, const char* windowTitle) {
	 return platform_init_window(width, height, windowTitle);
}

PALAPI uint8_t pal_set_window_title(pal_window* window, const char* string) {
	return platform_set_window_title(window, string);
}

PALAPI pal_bool pal_make_window_fullscreen(pal_window* window) {
	return platform_make_window_fullscreen(window);
}

PALAPI pal_bool pal_make_window_fullscreen_ex(pal_window* window, int width, int height, int refreshrate) {
	return platform_make_window_fullscreen_ex(window, width, height, refreshrate);
}

PALAPI pal_bool pal_make_window_fullscreen_windowed(pal_window* window) {
	return platform_make_window_fullscreen_windowed(window);
}

PALAPI pal_bool pal_make_window_windowed(pal_window* window) {
	return platform_make_window_windowed(window);
}

PALAPI void pal_set_window_icon(pal_window* window, const char* image_path) {
	(void)platform_set_window_icon(window, image_path);
}

PALAPI void pal_set_window_icon_legacy(pal_window* window, const char* image_path) {
	(void)platform_set_window_icon_legacy(window, image_path);
}

PALAPI void pal_set_taskbar_icon(pal_window* taskbar, const char* image_path) {
	(void)platform_set_taskbar_icon(taskbar, image_path);
}

PALAPI void pal_set_taskbar_icon_legacy(pal_window* taskbar, const char* image_path) {
	(void)platform_set_taskbar_icon_legacy(taskbar, image_path);
}

PALAPI void pal_set_cursor(pal_window* window, const char* image_path, int size) {
	(void)platform_set_cursor(window, image_path, size);
}

PALAPI void pal_window_hint(int type, int value) {
	(void)platform_set_window_hint(type, value);
}

PALAPI VideoMode* pal_get_video_mode(pal_monitor* monitor) {
	return platform_get_video_mode(monitor);
}
PALAPI pal_monitor* pal_get_primary_monitor(void) {
	return platform_get_primary_monitor();
}

PALAPI void* gl_get_proc_address(const unsigned char* proc) {
	return platform_gl_get_proc_address(proc);
}

PALAPI int register_input_devices(pal_window* window) {
	return platform_register_raw_input_devices(window);
}

// Keyboard input
PALAPI uint8_t is_key_pressed(int key) {

	if (is_key_down(key) && !is_key_processed(key)) {
		set_key_processed(key);
		return 1;
	}
	else {
		return 0;
	}

}

PALAPI uint8_t is_key_down(int key) {
	return input.keys[key];

}

PALAPI uint8_t is_key_processed(int key) {
	return input.keys_processed[key];
}

PALAPI void set_key_processed(int key) {
	input.keys_processed[key] = 1;  // Mark as processed
}

// Mouse input
PALAPI v2 get_mouse_position(pal_window* window) {
	return platform_get_mouse_position(window);

}

PALAPI uint8_t is_mouse_pressed(int button) {

	if (is_mouse_down(button) && !is_mouse_pressed(button)) {
		set_mouse_processed(button);
		return 1;
	}
	else {
		return 0;
	}
}

PALAPI uint8_t is_mouse_down(int button) {
	return input.mouse_buttons[button];
}

PALAPI uint8_t is_mouse_processed(int button) {
	return input.mouse_buttons_processed[button];
}

PALAPI void set_mouse_processed(int button) {
	input.mouse_buttons_processed[button] = 1; // Mark as processed
}

PALAPI uint8_t pal_poll_events(pal_event* event, pal_window* window) {
	return platform_poll_events(event, window);
}

PALAPI int make_context_current(pal_window* window) {
	return platform_make_context_current(window);
}

PALAPI void pal_gamepad_init(pal_window* window) {
	(void)platform_gamepad_init(window);
}
void pal_gamepad_shutdown() {
	(void)platform_gamepad_shutdown();
}
int pal_gamepad_get_count() {
	return platform_gamepad_get_count();
}
pal_bool pal_gamepad_get_state(int index, pal_gamepad_state* out_state) {
	return platform_gamepad_get_state(index, out_state);
}
pal_bool pal_gamepad_set_vibration(int index, float left_motor, float right_motor) {
	return platform_gamepad_set_vibration(index, left_motor, right_motor);
}
pal_bool pal_gamepad_load_mappings(const char* filename) {
    return platform_gamepad_load_mappings(filename);
}

/*

###########################################
		   RENDERING FUNCTIONS.
###########################################

*/

PALAPI void begin_drawing(void) {
	(void)platform_begin_drawing();
}

PALAPI void DrawTriangle() {

}

PALAPI void end_drawing(pal_window* window) {
	(void)platform_end_drawing(window);
}

/*

###########################################
		   SOUND FUNCTIONS.
###########################################

*/

PALAPI int play_sound(Sound* sound, float volume) {
	return platform_play_sound(sound, volume);
}

// TODO: @fix This loads uncompressed .wav files only!
static int load_wav(FILE* file, Sound* out) {
	static const int WAV_FMT_PCM = 0x0001, WAV_FMT_IEEE_FLOAT = 0x0003, WAV_FMT_EXTENSIBLE = 0xFFFE;

	static const uint8_t SUBFORMAT_PCM[16] = {
	0x01, 0x00, 0x00, 0x00, 0x10, 0x00,
	0x80, 0x00, 0x00, 0xAA, 0x00, 0x38,
	0x9B, 0x71, 0x00, 0x00
	};

	static const uint8_t SUBFORMAT_IEEE_FLOAT[16] = {
		0x03, 0x00, 0x00, 0x00, 0x10, 0x00,
		0x80, 0x00, 0x00, 0xAA, 0x00, 0x38,
		0x9B, 0x71, 0x00, 0x00
	};

	char riffHeader[4];
	uint32_t riffSize;
	char waveID[4];

	if (fread(riffHeader, 1, 4, file) != 4 || fread(&riffSize, 4, 1, file) != 1 || fread(waveID, 1, 4, file) != 4) {
		return 0;
	}

	if (memcmp(riffHeader, "RIFF", 4) != 0 || memcmp(waveID, "WAVE", 4) != 0) {
		return 0;
	}

	int audioFormat = 0;
	int numChannels = 0;
	int sampleRate = 0;
	int bitsPerSample = 0;
	int isFloat = 0;

	unsigned char* audioData = NULL;
	uint32_t dataSize = 0;

	while (!feof(file)) {
		char chunkID[4];
		uint32_t chunkSize;
		if (fread(chunkID, 1, 4, file) != 4) break;
		if (fread(&chunkSize, 4, 1, file) != 1) break;

		if (memcmp(chunkID, "fmt ", 4) == 0) {
			uint16_t formatTag;
			fread(&formatTag, sizeof(uint16_t), 1, file);
			fread(&numChannels, sizeof(uint16_t), 1, file);
			fread(&sampleRate, sizeof(uint32_t), 1, file);
			fseek(file, 6, SEEK_CUR); // skip byte rate + block align
			fread(&bitsPerSample, sizeof(uint16_t), 1, file);

			if (formatTag == WAV_FMT_EXTENSIBLE && chunkSize >= 40) {
				uint16_t cbSize;
				fread(&cbSize, sizeof(uint16_t), 1, file);
				if (cbSize < 22) return 0;

				fseek(file, 6, SEEK_CUR); // skip validBitsPerSample, channelMask
				uint8_t subFormat[16];
				if (fread(subFormat, 1, 16, file) != 16) return 0;

				if (memcmp(subFormat, SUBFORMAT_PCM, 16) == 0) {
					audioFormat = WAV_FMT_PCM;
					isFloat = 0;
				}
				else if (memcmp(subFormat, SUBFORMAT_IEEE_FLOAT, 16) == 0) {
					audioFormat = WAV_FMT_IEEE_FLOAT;
					isFloat = 1;
				}
				else {
					return -1;
				}
			}
			else {
				audioFormat = formatTag;
				if (audioFormat == WAV_FMT_PCM) {
					isFloat = 0;
				}
				else if (audioFormat == WAV_FMT_IEEE_FLOAT) {
					isFloat = 1;
				}
				else {
					return -1;
				}

				if (chunkSize > 16) {
					fseek(file, chunkSize - 16, SEEK_CUR);
				}
			}
		}
		else if (memcmp(chunkID, "data", 4) == 0) {
			audioData = (unsigned char*)malloc(chunkSize);
			if (!audioData || fread(audioData, 1, chunkSize, file) != chunkSize) {
				free(audioData);
				return -1;
			}
			dataSize = chunkSize;
		}
		else {
			fseek(file, chunkSize, SEEK_CUR); // skip unknown chunk
		}
	}

	if (!audioData || (audioFormat != WAV_FMT_PCM && audioFormat != WAV_FMT_IEEE_FLOAT)) {
		free(audioData);
		return -1;
	}

	out->data = audioData;
	out->dataSize = dataSize;
	out->sampleRate = sampleRate;
	out->channels = numChannels;
	out->bitsPerSample = bitsPerSample;
	out->isFloat = isFloat;

	return 0;
}

// --- Ogg Loader ---
static int load_ogg(const char* filename, Sound* out) {
	int channels, sample_rate;
	short* pcm_data;

	// Decode the entire .ogg file into PCM
	int samples = stb_vorbis_decode_filename(
		filename, &channels, &sample_rate, &pcm_data
	);

	if (samples <= 0) {
		printf("Failed to decode Ogg file.\n");
	}

	// Fill the Sound struct
	out->data = (unsigned char*)pcm_data;
	out->dataSize = samples * channels * sizeof(short);
	out->channels = channels;
	out->sampleRate = sample_rate;
	out->bitsPerSample = 16; // Ogg decodes to 16-bit PCM by default
	out->isFloat = 0;
	return 0;
}

// --- Unified Sound Loader ---
int load_sound(const char* filename, Sound* out) {
	FILE* file = fopen(filename, "rb");
	if (!file) return 0;

	char header[12];
	size_t read = fread(header, 1, sizeof(header), file);
	if (read < 12) {
		fclose(file);
		return 0;
	}

	rewind(file);

	if (memcmp(header, "RIFF", 4) == 0 && memcmp(header + 8, "WAVE", 4) == 0) {
		int result = load_wav(file, out);
		fclose(file);
		return result;
	}

	if (memcmp(header, "OggS", 4) == 0) {
		fclose(file);
		return load_ogg(filename, out);
	}

	fclose(file);
	return 0;
}

void free_sound(Sound* sound) {
	if (sound->data) free(sound->data);
	sound->data = NULL;
}

/*

###########################################
	     LIBRARY LOADING FUNCTIONS
###########################################

*/

void* load_dynamic_library(char* dll) {
	return platform_load_dynamic_library(dll);

}

void* load_dynamic_function(void* dll, char* func_name) {
	return platform_load_dynamic_function(dll, func_name);

}

uint8_t free_dynamic_library(void* dll) {
	return platform_free_dynamic_library(dll);

}

/*

###########################################
				 File I/O
###########################################

*/

uint8_t does_file_exist(const char* file_path) {

	assert(file_path != NULL);
	FILE* file = fopen(file_path, "rb");
	if (!file)
	{
		return 0;
	}
	fclose(file);

	return 1;
}

time_t get_file_timestamp(const char* file) {
	struct stat file_stat = { 0 };
	stat(file, &file_stat);
	return file_stat.st_mtime;
}

long get_file_size(const char* file_path) {
	assert(file_path != NULL);
	long fileSize = 0;
	FILE* file = fopen(file_path, "rb");
	if (!file)
	{
		fprintf(stderr, "ERROR: Failed to open file!\n");
		return 0;
	}

	fseek(file, 0, SEEK_END);
	fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);
	fclose(file);

	return fileSize;
}

char* read_file(const char* filePath, int* fileSize, char* buffer) {
	assert(filePath != NULL);
	assert(fileSize != NULL);
	assert(buffer != NULL);

	*fileSize = 0;
	FILE* file = fopen(filePath, "rb");
	if (!file)
	{
		fprintf(stderr, "ERROR: Failed to open file!\n");
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	*fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	memset(buffer, 0, *fileSize + 1);
	fread(buffer, sizeof(char), *fileSize, file);

	fclose(file);

	return buffer;
}

void write_file(const char* filePath, char* buffer, int size) {
	assert(filePath != NULL);
	assert(buffer != NULL);

	FILE* file = fopen(filePath, "wb");
	if (!file)
	{
		fprintf(stderr, "ERROR: Failed to open file!\n");
		return;
	}

	fwrite(buffer, sizeof(char), size, file);
	fclose(file);
}

uint8_t copy_file(const char* fileName, const char* outputName, char* buffer) {
	int fileSize = 0;
	char* data = read_file(fileName, &fileSize, buffer);

	FILE* outputFile = fopen(outputName, "wb");
	if (!outputFile)
	{
		fprintf(stderr, "ERROR: Failed to open file!\n");
		return 0;
	}

	size_t result = fwrite(data, sizeof(char), fileSize, outputFile);
	if (!result)
	{
		fprintf(stderr, "ERROR: Failed to open file!\n");
		return 0;
	}

	fclose(outputFile);

	return 1;
}

/*

###########################################
		   STRING PARSING HELPERS
###########################################

*/

PALAPI uint8_t is_upper_case(char ch) {
	return ((ch >= 'A') && (ch <= 'Z'));
}

PALAPI uint8_t is_lower_case(char ch) {
	return ((ch >= 'a') && (ch <= 'z'));
}

PALAPI uint8_t is_letter(char ch) {
	return (is_upper_case(ch) || is_lower_case(ch));
}

PALAPI uint8_t is_end_of_line(char ch) {
	return ((ch == '\r') || (ch == '\n'));
}

PALAPI uint8_t is_whitespace(char ch) {
	return ((ch == ' ') || (ch == '\t') || (ch == '\v') || (ch == '\f'));
}

PALAPI uint8_t is_number(char ch) {
	return ((ch >= '0') && (ch <= '9'));
}

PALAPI uint8_t is_underscore(char ch) {
	return (ch == '_');
}

PALAPI uint8_t is_hyphen(char ch) {
	return (ch == '-');
}

PALAPI uint8_t is_dot(char ch) {
	return (ch == '.');
}

PALAPI uint8_t are_chars_equal(char ch1, char ch2) {
	return (ch1 == ch2);
}

PALAPI uint8_t are_strings_equal(int count, char* str1, char* str2) {
	for (int i = 0; i < count; i++) {
		if (str1 == NULL || str2 == NULL)
			return 0;
		if (*str1 != *str2) {
			return 0;
		}
		str1++;
		str2++;
	}
	return 1;
}

int pal_sleep(double milliseconds) {
	return platform_sleep(milliseconds);
}
