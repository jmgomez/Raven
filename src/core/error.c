/**
 * HUI - core/error.c
 * Last-error record and error callback. Usable before HUI_Init and from any thread.
 */

#include "thread.h"

#define HUI_ERROR_TEXT_SIZE 512

static hui_atomic_t g_error_lock;
static HUI_EError g_error_code = HUI_NO_ERROR;
static HUI_ESeverity g_error_severity = HUI_SEVERITY_WEAK_WARNING;
static char g_error_detail[HUI_ERROR_TEXT_SIZE];
static char g_error_returned[HUI_ERROR_TEXT_SIZE];
static HUI_CErrorCallback g_error_callback;

/* A spin lock: errors are rare and this must work with no initialization. */
static void hui_error_lock(void) {
	while (hui_atomic_exchange(&g_error_lock, 1) != 0) {}
}

static void hui_error_unlock(void) {
	hui_atomic_store(&g_error_lock, 0);
}

void hui_error(HUI_EError code, HUI_ESeverity severity, const char* format, ...) {
	char text[HUI_ERROR_TEXT_SIZE];
	HUI_CErrorCallback callback;
	va_list args;

	va_start(args, format);
	vsnprintf(text, sizeof(text), format, args);
	va_end(args);

	hui_error_lock();
	g_error_code = code;
	g_error_severity = severity;
	memcpy(g_error_detail, text, sizeof(text));
	callback = g_error_callback;
	hui_error_unlock();

	if (callback) callback(code, severity, text);
}

HUI_EError HUI_GetLastError(const char** detail, HUI_ESeverity* severity) {
	HUI_EError code;
	hui_error_lock();
	code = g_error_code;
	if (severity) *severity = g_error_severity;
	memcpy(g_error_returned, g_error_detail, sizeof(g_error_returned));
	g_error_code = HUI_NO_ERROR;
	g_error_detail[0] = '\0';
	hui_error_unlock();
	if (detail) *detail = g_error_returned;
	return code;
}

void HUI_RegisterErrorCallback(HUI_CErrorCallback callback) {
	hui_error_lock();
	g_error_callback = callback;
	hui_error_unlock();
}

const char* HUI_ErrorEnumToString(HUI_EError error) {
	switch (error) {
		case HUI_NO_ERROR: return "HUI_NO_ERROR";
		case HUI_ERROR_INVALID_ID: return "HUI_ERROR_INVALID_ID";
		case HUI_ERROR_INVALID_ENUM: return "HUI_ERROR_INVALID_ENUM";
		case HUI_ERROR_INVALID_VALUE: return "HUI_ERROR_INVALID_VALUE";
		case HUI_ERROR_INVALID_OPERATION: return "HUI_ERROR_INVALID_OPERATION";
		case HUI_ERROR_BACKEND: return "HUI_ERROR_BACKEND";
		case HUI_ERROR_PLATFORM: return "HUI_ERROR_PLATFORM";
		case HUI_ERROR_OUT_OF_MEMORY: return "HUI_ERROR_OUT_OF_MEMORY";
		case HUI_ERROR_INVALID_FILE_FORMAT: return "HUI_ERROR_INVALID_FILE_FORMAT";
		case HUI_ERROR_FILE_NOT_FOUND: return "HUI_ERROR_FILE_NOT_FOUND";
		case HUI_ERROR_XML: return "HUI_ERROR_XML";
		case HUI_ERROR_WRONG_THREAD: return "HUI_ERROR_WRONG_THREAD";
	}
	return "HUI_ERROR_UNKNOWN";
}

const char* HUI_SeverityEnumToString(HUI_ESeverity severity) {
	switch (severity) {
		case HUI_SEVERITY_WEAK_WARNING: return "HUI_SEVERITY_WEAK_WARNING";
		case HUI_SEVERITY_WARNING: return "HUI_SEVERITY_WARNING";
		case HUI_SEVERITY_ERROR: return "HUI_SEVERITY_ERROR";
		case HUI_SEVERITY_FATAL: return "HUI_SEVERITY_FATAL";
	}
	return "HUI_SEVERITY_UNKNOWN";
}
