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

class Frodo 
{
    public:
	    C64 *TheC64;
	    char prefs_path[256];	// Pathname of current preferences file
        volatile bool running;

    public:
	    Frodo();
	    ~Frodo();

    public:
	    bool initialize(int argc, char **argv);
        void run();
        void shutdown();
        void emulationLoop();

    private:
	    bool loadRomFiles();
        void handleEvent(SDL_Event* event);
        void doStep();
};

class InputHandler
{
    public:
        virtual void handleMouseEvent(int x, int y, bool press) = 0;
        virtual void handleKeyEvent(int key, int sym, bool press) = 0;
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
