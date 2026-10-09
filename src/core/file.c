/**
 * Raven - core/file.c
 * Whole-file reading with UTF-8 paths on every platform.
 */

#include "base.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static FILE* rvn_fopen_read(const char* path) {
	wchar_t wide[1024];
	if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, (int)RVN_COUNT_OF(wide)) <= 0) return NULL;
	return _wfopen(wide, L"rb");
}
#else
static FILE* rvn_fopen_read(const char* path) {
	return fopen(path, "rb");
}
#endif

char* rvn_read_file(const char* path, size_t* size) {
	FILE* file;
	long length;
	char* data;
	size_t read;

	if (size) *size = 0;
	if (!path) return NULL;
	file = rvn_fopen_read(path);
	if (!file) return NULL;
	if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return NULL;
	}
	data = (char*)rvn_alloc((size_t)length + 1);
	read = fread(data, 1, (size_t)length, file);
	fclose(file);
	if (read != (size_t)length) {
		rvn_free(data);
		return NULL;
	}
	data[length] = '\0';
	if (size) *size = (size_t)length;
	return data;
}
