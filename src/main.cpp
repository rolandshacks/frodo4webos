/*
 *  main.cpp - Main program
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */
#include "sysdeps.h"

#include "Version.h"


#include "main.h"
#include "C64.h"
#include "Display.h"
#include "Prefs.h"
#include "SAM.h"

// Global variables
char AppDirPath[1024];	// Path of application directory

// The application.
Frodo *TheApp = NULL;

// ROM file names
#define BASIC_ROM_FILE	"resources/Basic.ROM"
#define KERNAL_ROM_FILE	"resources/Kernal.ROM"
#define CHAR_ROM_FILE	"resources/Char.ROM"
#define FLOPPY_ROM_FILE	"resources/1541.ROM"

extern int init_graphics(void);

/*
 *  Constructor: Initialize member variables
 */

Frodo::Frodo()
{
	TheC64 = NULL;
}

Frodo::~Frodo()
{
}

/*
 *  Process command line arguments
 */

void Frodo::ArgvReceived(int argc, char **argv)
{
	if (argc == 2)
		strncpy(prefs_path, argv[1], 255);
}


/*
 *  Arguments processed, run emulation
 */

void Frodo::ReadyToRun(void)
{
	getcwd(AppDirPath, 256);

	// Load preferences
	if (!prefs_path[0]) {
		char *home = getenv("HOME");
		if (home != NULL && strlen(home) < 240) {
			strncpy(prefs_path, home, 200);
			strcat(prefs_path, "/");
		}
		strcat(prefs_path, ".frodorc");
	}
	ThePrefs.Load(prefs_path);

	// Create and start C64
	TheC64 = new C64;
	if (load_rom_files())
		TheC64->Run();
	delete TheC64;
}

bool Frodo::load_rom_files(void)
{
	FILE *file;

	// Load Basic ROM
	if ((file = fopen(BASIC_ROM_FILE, "rb")) != NULL) {
		if (fread(TheC64->Basic, 1, 0x2000, file) != 0x2000) {
			ShowRequester("Can't read 'Basic ROM'.", "Quit");
			return false;
		}
		fclose(file);
	} else {
		ShowRequester("Can't find 'Basic ROM'.", "Quit");
		return false;
	}

	// Load Kernal ROM
	if ((file = fopen(KERNAL_ROM_FILE, "rb")) != NULL) {
		if (fread(TheC64->Kernal, 1, 0x2000, file) != 0x2000) {
			ShowRequester("Can't read 'Kernal ROM'.", "Quit");
			return false;
		}
		fclose(file);
	} else {
		ShowRequester("Can't find 'Kernal ROM'.", "Quit");
		return false;
	}

	// Load Char ROM
	if ((file = fopen(CHAR_ROM_FILE, "rb")) != NULL) {
		if (fread(TheC64->Char, 1, 0x1000, file) != 0x1000) {
			ShowRequester("Can't read 'Char ROM'.", "Quit");
			return false;
		}
		fclose(file);
	} else {
		ShowRequester("Can't find 'Char ROM'.", "Quit");
		return false;
	}

	// Load 1541 ROM
	if ((file = fopen(FLOPPY_ROM_FILE, "rb")) != NULL) {
		if (fread(TheC64->ROM1541, 1, 0x4000, file) != 0x4000) {
			ShowRequester("Can't read '1541 ROM'.", "Quit");
			return false;
		}
		fclose(file);
	} else {
		ShowRequester("Can't find '1541 ROM'.", "Quit");
		return false;
	}

	return true;
}

/*
 *  Create application object and start it
 */

int main(int argc, char **argv)
{
	// Init SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK ) < 0) {
		fprintf(stderr, "Couldn't initialize SDL (%s)\n", SDL_GetError());
		return 0;
	}

    #ifdef WEBOS
        PDL_Init(0);
        atexit(PDL_Quit);
    #endif

    srand( (unsigned)time( NULL ) );

	printf("%s by Christian Bauer\n", VERSION_STRING);
	if (!init_graphics())
    {
    	SDL_Quit();
		return 0;
    }

    #ifdef FRODO_SC
        printf("Mode: single-cycle emulation\n");
    #else
        printf("Mode: line-based emulation\n");
    #endif

	fflush(stdout);

	TheApp = new Frodo();
	TheApp->ArgvReceived(argc, argv);
	TheApp->ReadyToRun();
	delete TheApp;
    TheApp = NULL;

	SDL_Quit();

	return 0;
}
