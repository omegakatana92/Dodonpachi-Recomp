/**
 *    Name:        rom.c
 *    Description: load the player's ddonpach set from a .zip or a folder, check every file and
 *                 build the board's regions
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    license:BSD-3-Clause
 *
 *    Files are matched by CRC32 and SHA-1, never by name, so renamed archives still verify. The
 *    archive is read with miniz (public domain, runtime/third_party/miniz).
 */
#include "ddp/rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#include "miniz.h"

typedef struct
{
	const char *name;
	uint32_t crc;
	const char *sha1;
	size_t size;
} rom_file;

static const rom_file ROM_FILES[] =
{
	{ "b1.u27",              0xb5cdc8d3u, "58757b50e21a27e500a82c03f62cf02a85389926", 0x080000u },
	{ "b2.u26",              0x6bbb063au, "e5de64b9c3efc0a38a2e0e16b78ee393bff63558", 0x080000u },
	{ "u50.bin",             0x14b260ecu, "33bda210302428d5500115d0c7a839cdfcb67d17", 0x200000u },
	{ "u51.bin",             0xe7ba8cceu, "ad74a6b7d53760b19587c4a6dbea937daa7e87ce", 0x200000u },
	{ "u52.bin",             0x02492ee0u, "64d9cc64a4ad189a8b03cf6a749ddb732b4a0014", 0x200000u },
	{ "u53.bin",             0xcb4c10f0u, "a622e8bd0c938b5d38b392b247400b744d8be288", 0x200000u },
	{ "u60.bin",             0x903096a7u, "a243e903fef7c4a7b71383263e82e42acd869261", 0x200000u },
	{ "u61.bin",             0xd89b7631u, "a66bb4955ca58fab8973ca37a0f971e9a67ce017", 0x200000u },
	{ "u62.bin",             0x292bfb6bu, "11b385991ee990eb5ef36e136b988802b5f90fa4", 0x200000u },
	{ "u6.bin",              0x9dfdafafu, "f5cb450cdc78a20c3a74c6dac05c9ac3cba08327", 0x200000u },
	{ "u7.bin",              0x795b17d5u, "cbfc29f1df9600c82e0fdae00edd00da5b73e14c", 0x200000u },
	{ "eeprom-ddonpach.bin", 0x315fb546u, "7f597107d1610fc286413e0e93c794c80c0c554f", 0x000080u },
};

#define ROM_FILE_COUNT (sizeof(ROM_FILES) / sizeof(ROM_FILES[0]))
#define ROM_MAX_CANDIDATES 64

typedef struct
{
	uint8_t *data;
	size_t size;
	int matched;
} candidate;

/* ---- SHA-1 ---- */

typedef struct
{
	uint32_t state[5];
	uint64_t length;
	uint8_t block[64];
	size_t used;
} sha1_context;

static uint32_t rol32(uint32_t value, unsigned count)
{
	return (value << count) | (value >> (32 - count));
}

static void sha1_transform(sha1_context *context, const uint8_t block[64])
{
	uint32_t w[80];
	uint32_t a, b, c, d, e;
	unsigned i;

	for (i = 0; i < 16; ++i)
		w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
		       ((uint32_t)block[i * 4 + 2] << 8) | block[i * 4 + 3];
	for (i = 16; i < 80; ++i)
		w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

	a = context->state[0]; b = context->state[1]; c = context->state[2];
	d = context->state[3]; e = context->state[4];

	for (i = 0; i < 80; ++i)
	{
		uint32_t f, k, temp;
		if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999u; }
		else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
		else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
		else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
		temp = rol32(a, 5) + f + e + k + w[i];
		e = d; d = c; c = rol32(b, 30); b = a; a = temp;
	}

	context->state[0] += a; context->state[1] += b; context->state[2] += c;
	context->state[3] += d; context->state[4] += e;
}

static void sha1_init(sha1_context *context)
{
	context->state[0] = 0x67452301u;
	context->state[1] = 0xEFCDAB89u;
	context->state[2] = 0x98BADCFEu;
	context->state[3] = 0x10325476u;
	context->state[4] = 0xC3D2E1F0u;
	context->length = 0;
	context->used = 0;
}

static void sha1_update(sha1_context *context, const uint8_t *data, size_t length)
{
	context->length += (uint64_t)length;
	while (length--)
	{
		context->block[context->used++] = *data++;
		if (context->used == 64)
		{
			sha1_transform(context, context->block);
			context->used = 0;
		}
	}
}

static void sha1_final(sha1_context *context, uint8_t digest[20])
{
	uint64_t bits = context->length * 8u;
	unsigned i;

	context->block[context->used++] = 0x80;
	if (context->used > 56)
	{
		while (context->used < 64)
			context->block[context->used++] = 0;
		sha1_transform(context, context->block);
		context->used = 0;
	}
	while (context->used < 56)
		context->block[context->used++] = 0;
	for (i = 0; i < 8; ++i)
		context->block[56 + i] = (uint8_t)(bits >> (56 - i * 8));
	sha1_transform(context, context->block);

	for (i = 0; i < 20; ++i)
		digest[i] = (uint8_t)(context->state[i / 4] >> (24 - (i % 4) * 8));
}

static void sha1_hex(const uint8_t *data, size_t size, char out[41])
{
	static const char digits[] = "0123456789abcdef";
	sha1_context context;
	uint8_t digest[20];
	unsigned i;

	sha1_init(&context);
	sha1_update(&context, data, size);
	sha1_final(&context, digest);
	for (i = 0; i < 20; ++i)
	{
		out[i * 2] = digits[digest[i] >> 4];
		out[i * 2 + 1] = digits[digest[i] & 15];
	}
	out[40] = 0;
}

/* ---- file helpers ---- */

static void set_error(char *error, size_t size, const char *text)
{
	if (error && size)
	{
		strncpy(error, text, size - 1);
		error[size - 1] = 0;
	}
}

static const char *base_name(const char *path)
{
	const char *name = path;
	const char *p;
	for (p = path; *p; ++p)
	{
		if (*p == '/' || *p == '\\')
			name = p + 1;
	}
	return name;
}

static int is_directory(const char *path)
{
#ifdef _WIN32
	DWORD attributes = GetFileAttributesA(path);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
	struct stat status;
	return stat(path, &status) == 0 && S_ISDIR(status.st_mode);
#endif
}

static uint8_t *read_file(const char *path, size_t *size)
{
	FILE *file = fopen(path, "rb");
	long length;
	uint8_t *data;

	if (!file)
		return NULL;
	if (fseek(file, 0, SEEK_END) != 0)
	{
		fclose(file);
		return NULL;
	}
	length = ftell(file);
	if (length < 0 || fseek(file, 0, SEEK_SET) != 0)
	{
		fclose(file);
		return NULL;
	}
	data = (uint8_t *)malloc((size_t)length ? (size_t)length : 1);
	if (!data)
	{
		fclose(file);
		return NULL;
	}
	if ((size_t)length && fread(data, 1, (size_t)length, file) != (size_t)length)
	{
		free(data);
		fclose(file);
		return NULL;
	}
	fclose(file);
	*size = (size_t)length;
	return data;
}

static int load_folder(const char *path, candidate *candidates, size_t *count,
                       char *error, size_t error_size)
{
	size_t used = 0;

#if defined(_WIN32)
	{
		char pattern[1024];
		WIN32_FIND_DATAA entry;
		HANDLE handle;
		snprintf(pattern, sizeof(pattern), "%s\\*", path);
		handle = FindFirstFileA(pattern, &entry);
		if (handle == INVALID_HANDLE_VALUE)
		{
			set_error(error, error_size, "cannot enumerate the ROM folder");
			return -1;
		}
		do
		{
			char full[1024];
			size_t size;
			uint8_t *data;
			if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;
			if (used >= ROM_MAX_CANDIDATES)
			{
				FindClose(handle);
				set_error(error, error_size, "too many files in the ROM folder");
				return -1;
			}
			snprintf(full, sizeof(full), "%s\\%s", path, entry.cFileName);
			data = read_file(full, &size);
			if (!data)
				continue;
			candidates[used].data = data;
			candidates[used].size = size;
			candidates[used].matched = 0;
			used++;
		} while (FindNextFileA(handle, &entry));
		FindClose(handle);
	}
#else
	{
		DIR *directory = opendir(path);
		struct dirent *entry;
		if (!directory)
		{
			set_error(error, error_size, "cannot enumerate the ROM folder");
			return -1;
		}
		while ((entry = readdir(directory)) != NULL)
		{
			char full[1024];
			size_t size;
			uint8_t *data;
			if (entry->d_name[0] == '.')
				continue;
			if (used >= ROM_MAX_CANDIDATES)
			{
				closedir(directory);
				set_error(error, error_size, "too many files in the ROM folder");
				return -1;
			}
			snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
			if (is_directory(full))
				continue;
			data = read_file(full, &size);
			if (!data)
				continue;
			candidates[used].data = data;
			candidates[used].size = size;
			candidates[used].matched = 0;
			used++;
		}
		closedir(directory);
	}
#endif

	(void)base_name;
	*count = used;
	return 0;
}

static int load_zip(const char *path, candidate *candidates, size_t *count,
                    char *error, size_t error_size)
{
	mz_zip_archive archive;
	mz_uint files;
	mz_uint index;
	size_t used = 0;

	memset(&archive, 0, sizeof(archive));
	if (!mz_zip_reader_init_file(&archive, path, 0))
	{
		set_error(error, error_size, "cannot open the ROM .zip");
		return -1;
	}

	files = mz_zip_reader_get_num_files(&archive);
	for (index = 0; index < files; ++index)
	{
		char name[1024];
		size_t size = 0;
		void *data;

		if (mz_zip_reader_is_file_a_directory(&archive, index))
			continue;
		if (used >= ROM_MAX_CANDIDATES)
		{
			mz_zip_reader_end(&archive);
			set_error(error, error_size, "too many files in the ROM .zip");
			return -1;
		}
		if (!mz_zip_reader_get_filename(&archive, index, name, sizeof(name)))
			continue;
		data = mz_zip_reader_extract_to_heap(&archive, index, &size, 0);
		if (!data)
			continue;
		candidates[used].data = (uint8_t *)data;
		candidates[used].size = size;
		candidates[used].matched = 0;
		used++;
	}

	mz_zip_reader_end(&archive);
	*count = used;
	return 0;
}

/* ---- region builders ---- */

static uint8_t *alloc_region(size_t size)
{
	return (uint8_t *)calloc(1, size);
}

void ddp_rom_free(ddp_rom_set *set)
{
	if (!set)
		return;
	free(set->program);
	free(set->sprites);
	free(set->layers[0]);
	free(set->layers[1]);
	free(set->layers[2]);
	free(set->samples);
	memset(set, 0, sizeof(*set));
}

int ddp_rom_load(const char *path, ddp_rom_set *set, char *error, size_t error_size)
{
	candidate candidates[ROM_MAX_CANDIDATES];
	size_t candidate_count = 0;
	const uint8_t *files[ROM_FILE_COUNT];
	size_t i;
	int result = -1;

	memset(set, 0, sizeof(*set));
	memset(candidates, 0, sizeof(candidates));
	memset((void *)files, 0, sizeof(files));

	if (is_directory(path))
	{
		if (load_folder(path, candidates, &candidate_count, error, error_size) != 0)
			goto done;
	}
	else
	{
		if (load_zip(path, candidates, &candidate_count, error, error_size) != 0)
			goto done;
	}

	for (i = 0; i < candidate_count; ++i)
	{
		uint32_t crc = (uint32_t)mz_crc32(0, candidates[i].data, candidates[i].size);
		size_t f;
		for (f = 0; f < ROM_FILE_COUNT; ++f)
		{
			char digest[41];
			if (files[f] || ROM_FILES[f].crc != crc || ROM_FILES[f].size != candidates[i].size)
				continue;
			sha1_hex(candidates[i].data, candidates[i].size, digest);
			if (strcmp(digest, ROM_FILES[f].sha1) != 0)
				continue;
			files[f] = candidates[i].data;
			candidates[i].matched = 1;
			break;
		}
	}

	for (i = 0; i < ROM_FILE_COUNT; ++i)
	{
		if (!files[i])
		{
			char message[256];
			snprintf(message, sizeof(message),
			         "the ddonpach set is incomplete: %s is missing or does not match",
			         ROM_FILES[i].name);
			set_error(error, error_size, message);
			goto done;
		}
	}

	set->program = alloc_region(DDP_PROGRAM_SIZE);
	set->sprites = alloc_region(DDP_SPRITE_SIZE);
	set->layers[0] = alloc_region(DDP_LAYER_SIZE);
	set->layers[1] = alloc_region(DDP_LAYER_SIZE);
	set->layers[2] = alloc_region(DDP_LAYER_SIZE);
	set->samples = alloc_region(DDP_SAMPLE_SIZE);
	if (!set->program || !set->sprites || !set->layers[0] || !set->layers[1] ||
	    !set->layers[2] || !set->samples)
	{
		set_error(error, error_size, "out of memory loading the ROM regions");
		goto done;
	}

	/* maincpu: b1 supplies the even bytes, b2 the odd ones */
	for (i = 0; i < 0x80000u; ++i)
	{
		set->program[i * 2] = files[0][i];
		set->program[i * 2 + 1] = files[1][i];
	}

	/* sprites0: u50-u53, loaded as ROM_LOAD16_WORD_SWAP: swap every 16-bit word. */
	for (i = 0; i < 4; ++i)
		memcpy(set->sprites + i * 0x200000u, files[2 + i], 0x200000u);
	for (i = 0; i < DDP_SPRITE_SIZE; i += 2)
	{
		uint8_t temporary = set->sprites[i];
		set->sprites[i] = set->sprites[i + 1];
		set->sprites[i + 1] = temporary;
	}
	/* layers 0-2: u60-u62 */
	for (i = 0; i < 3; ++i)
		memcpy(set->layers[i], files[6 + i], 0x200000u);
	/* YMZ280B sample ROM: u6 then u7 */
	memcpy(set->samples, files[9], 0x200000u);
	memcpy(set->samples + 0x200000u, files[10], 0x200000u);
	/* 93C46 image */
	memcpy(set->eeprom, files[11], 128);

	result = 0;

done:
	for (i = 0; i < candidate_count; ++i)
		free(candidates[i].data);
	if (result != 0)
		ddp_rom_free(set);
	return result;
}

uint32_t ddp_rom_reset_sp(const ddp_rom_set *set)
{
	if (!set || !set->program)
		return 0x110000u;
	return ((uint32_t)set->program[0] << 24) | ((uint32_t)set->program[1] << 16) |
	       ((uint32_t)set->program[2] << 8) | set->program[3];
}

uint32_t ddp_rom_reset_pc(const ddp_rom_set *set)
{
	if (!set || !set->program)
		return 0x0052EAu;
	return ((uint32_t)set->program[4] << 24) | ((uint32_t)set->program[5] << 16) |
	       ((uint32_t)set->program[6] << 8) | set->program[7];
}
