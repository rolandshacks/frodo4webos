/*
 *  main.h - Main program
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#ifndef _MAIN_H
#define _MAIN_H

class C64;

// Global variables
extern char AppDirPath[1024];	// Path of application directory

class Frodo {
public:
	Frodo();
	~Frodo();
	void ArgvReceived(int argc, char **argv);
	void ReadyToRun();
	void RunPrefsEditor();

	C64 *TheC64;
	char prefs_path[256];	// Pathname of current preferences file

private:
	bool load_rom_files();
};

// Global variables
extern Frodo *TheApp;	// Pointer to Frodo object

// Command line options.
extern bool full_screen;

#if defined(DEBUG) || defined(_DEBUG)

inline void Debug(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	char tmp[512];
	vsprintf(tmp, format, args);
	va_end(args);
	printf("DEBUG: %s", tmp);
}

#else

inline void Debug(const char *format, ...)
{
}

#endif

#define DebugResult(message, val) \
	Debug("%s: 0x%x (%d)\n", message, val, HRESULT_CODE(val))

#endif
