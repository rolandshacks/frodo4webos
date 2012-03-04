/*
 *  C64.cpp - Put the pieces together
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */
#include "sysdeps.h"
#include "main.h"
#include "C64.h"
#include "CPUC64.h"
#include "CPU1541.h"
#include "VIC.h"
#include "SID.h"
#include "CIA.h"
#include "REU.h"
#include "IEC.h"
#include "1541job.h"
#include "Display.h"
#include "Input.h"
#include "Prefs.h"
#include "virtual_joystick.h"

// ROM file names
#define BASIC_ROM_FILE	"resources/Basic.ROM"
#define KERNAL_ROM_FILE	"resources/Kernal.ROM"
#define CHAR_ROM_FILE	"resources/Char.ROM"
#define FLOPPY_ROM_FILE	"resources/1541.ROM"


#ifdef FRODO_SC
bool IsFrodoSC = true;
#else
bool IsFrodoSC = false;
#endif

#define FRAME_INTERVAL		(1000/SCREEN_FREQ)	// in milliseconds
#ifdef FRODO_SC
#define SPEEDOMETER_INTERVAL	4000			// in milliseconds
#else
#define SPEEDOMETER_INTERVAL	1000			// in milliseconds
#endif
#define JOYSTICK_SENSITIVITY	40			// % of live range
#define JOYSTICK_MIN		0x0000			// min value of range
#define JOYSTICK_MAX		0xffff			// max value of range
#define JOYSTICK_RANGE		(JOYSTICK_MAX - JOYSTICK_MIN)


/*
 *  Constructor: Allocate objects and memory
 */

C64::C64()
{
}


/*
 *  Destructor: Delete all objects
 */

C64::~C64()
{
    shutdown();
}


/*
 * Init emulation
 */
bool C64::init()
{
	int i,j;
	uint8 *p;

	// The thread is not yet running
	quit_thyself = false;
	have_a_break = false;

	// Initialize joystick variables.
	joy_state = 0xff;

	// No need to check for state change.
	state_change = false;

	// Open display
	TheDisplay = new C64Display(this);
    if (false == TheDisplay->init())
    {
        delete TheDisplay;
        TheDisplay = NULL;
        return false;
    }

    TheInput = new C64Input(this, TheDisplay);

	// Allocate RAM/ROM memory
	RAM = new uint8[0x10000];
	Basic = new uint8[0x2000];
	Kernal = new uint8[0x2000];
	Char = new uint8[0x1000];
	Color = new uint8[0x0400];
	RAM1541 = new uint8[0x0800];
	ROM1541 = new uint8[0x4000];

	// Create the chips
	TheCPU = new MOS6510(this, RAM, Basic, Kernal, Char, Color);
	TheJob1541 = new Job1541(RAM1541);
	TheCPU1541 = new MOS6502_1541(this, TheJob1541, TheDisplay, RAM1541, ROM1541);
	TheVIC = TheCPU->TheVIC = new MOS6569(this, TheDisplay, TheCPU, RAM, Char, Color);
	TheSID = TheCPU->TheSID = new MOS6581(this);
	TheCIA1 = TheCPU->TheCIA1 = new MOS6526_1(TheCPU, TheVIC);
	TheCIA2 = TheCPU->TheCIA2 = TheCPU1541->TheCIA2 = new MOS6526_2(TheCPU, TheVIC, TheCPU1541);
	TheIEC = TheCPU->TheIEC = new IEC(TheDisplay);
	TheREU = TheCPU->TheREU = new REU(TheCPU);

	// Initialize RAM with powerup pattern
	for (i=0, p=RAM; i<512; i++) {
		for (j=0; j<64; j++)
			*p++ = 0;
		for (j=0; j<64; j++)
			*p++ = 0xff;
	}

	// Initialize color RAM with random values
	for (i=0, p=Color; i<1024; i++)
    {
		*p++ = rand() & 0x0f;
    }

	// Clear 1541 RAM
	memset(RAM1541, 0, 0x800);

    joystick1 = joystick2 = NULL;

    TheJoystick = new VirtualJoystick();
    #ifdef WEBOS
        TheJoystick->setMode(VirtualJoystick::MODE_MOUSE);
    #else
        TheJoystick->setMode(VirtualJoystick::MODE_DISABLED);
    #endif

    #ifdef FRODO_SC
	    CycleCounter = 0;
    #endif

	// System-dependent things

    if (!loadRomFiles())
    {
        return false;
    }

	// Reset chips
	TheCPU->Reset();
	TheSID->Reset();
	TheCIA1->Reset();
	TheCIA2->Reset();
	TheCPU1541->Reset();

	// Patch kernal IEC routines
	orig_kernal_1d84 = Kernal[0x1d84];
	orig_kernal_1d85 = Kernal[0x1d85];
	PatchKernal(ThePrefs.FastReset, ThePrefs.Emul1541Proc);

    ThePrefs.SkipFrames = 2;

    ticksPerFrame = 1000/SCREEN_FREQ;

    sync(true);

    return true;
}

/*
 *  Run emulation
 */

void C64::doStep()
{
    if (!have_a_break)
    {
	    emulationStep();
    }
}

void C64::shutdown()
{
	delete TheJob1541;
	delete TheREU;
	delete TheIEC;
	delete TheCIA2;
	delete TheCIA1;
	delete TheSID;
	delete TheVIC;
	delete TheCPU1541;
	delete TheCPU;
    delete TheInput;
	delete TheDisplay;
    delete TheJoystick;

	delete[] RAM;
	delete[] Basic;
	delete[] Kernal;
	delete[] Char;
	delete[] Color;
	delete[] RAM1541;
	delete[] ROM1541;
}

/*
 *  Reset C64
 */

void C64::Reset(void)
{
	TheCPU->AsyncReset();
	TheCPU1541->AsyncReset();
	TheSID->Reset();
	TheCIA1->Reset();
	TheCIA2->Reset();
	TheIEC->Reset();
}


/*
 *  NMI C64
 */

void C64::NMI(void)
{
	TheCPU->AsyncNMI();
}


/*
 *  The preferences have changed. prefs is a pointer to the new
 *   preferences, ThePrefs still holds the previous ones.
 *   The emulation must be in the paused state!
 */

void C64::NewPrefs(Prefs *prefs)
{
	PatchKernal(prefs->FastReset, prefs->Emul1541Proc);

	TheDisplay->NewPrefs(prefs);

	TheIEC->NewPrefs(prefs);
	TheJob1541->NewPrefs(prefs);

	TheREU->NewPrefs(prefs);
	TheSID->NewPrefs(prefs);

	// Reset 1541 processor if turned on
	if (!ThePrefs.Emul1541Proc && prefs->Emul1541Proc)
    {
		TheCPU1541->AsyncReset();
    }
}


/*
 *  Patch kernal IEC routines
 */

void C64::PatchKernal(bool fast_reset, bool emul_1541_proc)
{
	if (fast_reset) {
		Kernal[0x1d84] = 0xa0;
		Kernal[0x1d85] = 0x00;
	} else {
		Kernal[0x1d84] = orig_kernal_1d84;
		Kernal[0x1d85] = orig_kernal_1d85;
	}

	if (emul_1541_proc) {
		Kernal[0x0d40] = 0x78;
		Kernal[0x0d41] = 0x20;
		Kernal[0x0d23] = 0x78;
		Kernal[0x0d24] = 0x20;
		Kernal[0x0d36] = 0x78;
		Kernal[0x0d37] = 0x20;
		Kernal[0x0e13] = 0x78;
		Kernal[0x0e14] = 0xa9;
		Kernal[0x0def] = 0x78;
		Kernal[0x0df0] = 0x20;
		Kernal[0x0dbe] = 0xad;
		Kernal[0x0dbf] = 0x00;
		Kernal[0x0dcc] = 0x78;
		Kernal[0x0dcd] = 0x20;
		Kernal[0x0e03] = 0x20;
		Kernal[0x0e04] = 0xbe;
	} else {
		Kernal[0x0d40] = 0xf2;	// IECOut
		Kernal[0x0d41] = 0x00;
		Kernal[0x0d23] = 0xf2;	// IECOutATN
		Kernal[0x0d24] = 0x01;
		Kernal[0x0d36] = 0xf2;	// IECOutSec
		Kernal[0x0d37] = 0x02;
		Kernal[0x0e13] = 0xf2;	// IECIn
		Kernal[0x0e14] = 0x03;
		Kernal[0x0def] = 0xf2;	// IECSetATN
		Kernal[0x0df0] = 0x04;
		Kernal[0x0dbe] = 0xf2;	// IECRelATN
		Kernal[0x0dbf] = 0x05;
		Kernal[0x0dcc] = 0xf2;	// IECTurnaround
		Kernal[0x0dcd] = 0x06;
		Kernal[0x0e03] = 0xf2;	// IECRelease
		Kernal[0x0e04] = 0x07;
	}

	// 1541
	ROM1541[0x2ae4] = 0xea;		// Don't check ROM checksum
	ROM1541[0x2ae5] = 0xea;
	ROM1541[0x2ae8] = 0xea;
	ROM1541[0x2ae9] = 0xea;
	ROM1541[0x2c9b] = 0xf2;		// DOS idle loop
	ROM1541[0x2c9c] = 0x00;
	ROM1541[0x3594] = 0x20;		// Write sector
	ROM1541[0x3595] = 0xf2;
	ROM1541[0x3596] = 0xf5;
	ROM1541[0x3597] = 0xf2;
	ROM1541[0x3598] = 0x01;
	ROM1541[0x3b0c] = 0xf2;		// Format track
	ROM1541[0x3b0d] = 0x02;
}


/*
 *  Save RAM contents
 */

void C64::SaveRAM(char *filename)
{
	FILE *f;

	if ((f = fopen(filename, "wb")) == NULL)
    {
		ShowRequester("RAM save failed.", "OK", NULL);
    }
	else 
    {
		fwrite((void*)RAM, 1, 0x10000, f);
		fwrite((void*)Color, 1, 0x400, f);
		if (ThePrefs.Emul1541Proc)
			fwrite((void*)RAM1541, 1, 0x800, f);
		fclose(f);
	}
}


/*
 *  Save CPU state to snapshot
 *
 *  0: Error
 *  1: OK
 *  -1: Instruction not completed
 */

int C64::SaveCPUState(FILE *f)
{
	MOS6510State state;
	TheCPU->GetState(&state);

	if (!state.instruction_complete)
		return -1;

	int i = fwrite(RAM, 0x10000, 1, f);
	i += fwrite(Color, 0x400, 1, f);
	i += fwrite((void*)&state, sizeof(state), 1, f);

	return i == 3;
}


/*
 *  Load CPU state from snapshot
 */

bool C64::LoadCPUState(FILE *f)
{
	MOS6510State state;

	int i = fread(RAM, 0x10000, 1, f);
	i += fread(Color, 0x400, 1, f);
	i += fread((void*)&state, sizeof(state), 1, f);

	if (i == 3) 
    {
		TheCPU->SetState(&state);
		return true;
	} else
		return false;
}


/*
 *  Save 1541 state to snapshot
 *
 *  0: Error
 *  1: OK
 *  -1: Instruction not completed
 */

int C64::Save1541State(FILE *f)
{
	MOS6502State state;
	TheCPU1541->GetState(&state);

	if (!state.idle && !state.instruction_complete)
		return -1;

	int i = fwrite(RAM1541, 0x800, 1, f);
	i += fwrite((void*)&state, sizeof(state), 1, f);

	return i == 2;
}


/*
 *  Load 1541 state from snapshot
 */

bool C64::Load1541State(FILE *f)
{
	MOS6502State state;

	int i = fread(RAM1541, 0x800, 1, f);
	i += fread((void*)&state, sizeof(state), 1, f);

	if (i == 2) {
		TheCPU1541->SetState(&state);
		return true;
	} else
		return false;
}


/*
 *  Save VIC state to snapshot
 */

bool C64::SaveVICState(FILE *f)
{
	MOS6569State state;
	TheVIC->GetState(&state);
	return fwrite((void*)&state, sizeof(state), 1, f) == 1;
}


/*
 *  Load VIC state from snapshot
 */

bool C64::LoadVICState(FILE *f)
{
	MOS6569State state;

	if (fread((void*)&state, sizeof(state), 1, f) == 1) {
		TheVIC->SetState(&state);
		return true;
	} else
		return false;
}


/*
 *  Save SID state to snapshot
 */

bool C64::SaveSIDState(FILE *f)
{
	MOS6581State state;
	TheSID->GetState(&state);
	return fwrite((void*)&state, sizeof(state), 1, f) == 1;
}


/*
 *  Load SID state from snapshot
 */

bool C64::LoadSIDState(FILE *f)
{
	MOS6581State state;

	if (fread((void*)&state, sizeof(state), 1, f) == 1) {
		TheSID->SetState(&state);
		return true;
	} else
		return false;
}


/*
 *  Save CIA states to snapshot
 */

bool C64::SaveCIAState(FILE *f)
{
	MOS6526State state;
	TheCIA1->GetState(&state);

	if (fwrite((void*)&state, sizeof(state), 1, f) == 1) {
		TheCIA2->GetState(&state);
		return fwrite((void*)&state, sizeof(state), 1, f) == 1;
	} else
		return false;
}


/*
 *  Load CIA states from snapshot
 */

bool C64::LoadCIAState(FILE *f)
{
	MOS6526State state;

	if (fread((void*)&state, sizeof(state), 1, f) == 1) {
		TheCIA1->SetState(&state);
		if (fread((void*)&state, sizeof(state), 1, f) == 1) {
			TheCIA2->SetState(&state);
			return true;
		} else
			return false;
	} else
		return false;
}


/*
 *  Save 1541 GCR state to snapshot
 */

bool C64::Save1541JobState(FILE *f)
{
	Job1541State state;
	TheJob1541->GetState(&state);
	return fwrite((void*)&state, sizeof(state), 1, f) == 1;
}


/*
 *  Load 1541 GCR state from snapshot
 */

bool C64::Load1541JobState(FILE *f)
{
	Job1541State state;

	if (fread((void*)&state, sizeof(state), 1, f) == 1) {
		TheJob1541->SetState(&state);
		return true;
	} else
		return false;
}


#define SNAPSHOT_HEADER "FrodoSnapshot"
#define SNAPSHOT_1541 1

#define ADVANCE_CYCLES	\
	TheVIC->EmulateCycle(); \
	TheCIA1->EmulateCycle(); \
	TheCIA2->EmulateCycle(); \
	TheCPU->EmulateCycle(); \
	if (ThePrefs.Emul1541Proc) { \
		TheCPU1541->CountVIATimers(1); \
		if (!TheCPU1541->Idle) \
			TheCPU1541->EmulateCycle(); \
	}


/*
 *  Save snapshot (emulation must be paused and in VBlank)
 *
 *  To be able to use SC snapshots with SL, SC snapshots are made thus that no
 *  partially dealt with instructions are saved. Instead all devices are advanced
 *  cycle by cycle until the current instruction has been finished. The number of
 *  cycles this takes is saved in the snapshot and will be reconstructed if the
 *  snapshot is loaded into FrodoSC again.
 */

void C64::SaveSnapshot(char *filename)
{
	FILE *f;
	uint8 flags;
	uint8 delay;
	int stat;

	if ((f = fopen(filename, "wb")) == NULL) {
		ShowRequester("Unable to open snapshot file", "OK", NULL);
		return;
	}

	fprintf(f, "%s%c", SNAPSHOT_HEADER, 10);
	fputc(0, f);	// Version number 0
	flags = 0;
	if (ThePrefs.Emul1541Proc)
		flags |= SNAPSHOT_1541;
	fputc(flags, f);
	SaveVICState(f);
	SaveSIDState(f);
	SaveCIAState(f);

    #ifdef FRODO_SC
	    delay = 0;
	    do {
		    if ((stat = SaveCPUState(f)) == -1) {	// -1 -> Instruction not finished yet
			    ADVANCE_CYCLES;	// Advance everything by one cycle
			    delay++;
		    }
	    } while (stat == -1);
	    fputc(delay, f);	// Number of cycles the saved CPUC64 lags behind the previous chips
    #else
	    SaveCPUState(f);
	    fputc(0, f);		// No delay
    #endif

	if (ThePrefs.Emul1541Proc) 
    {
		fwrite(ThePrefs.DrivePath[0], 256, 1, f);
        #ifdef FRODO_SC
		    delay = 0;
		    do {
			    if ((stat = Save1541State(f)) == -1) {
				    ADVANCE_CYCLES;
				    delay++;
			    }
		    } while (stat == -1);
		    fputc(delay, f);
        #else
		    Save1541State(f);
		    fputc(0, f);	// No delay
        #endif
		Save1541JobState(f);
	}
	fclose(f);

}


/*
 *  Load snapshot (emulation must be paused and in VBlank)
 */

bool C64::LoadSnapshot(char *filename)
{
	FILE *f;

	if ((f = fopen(filename, "rb")) != NULL) 
    {
		char Header[] = SNAPSHOT_HEADER;
		char *b = Header, c = 0;
		uint8 delay, i;

		// For some reason memcmp()/strcmp() and so forth utterly fail here.
		while (*b > 32) 
        {
			if ((c = fgetc(f)) != *b++) 
            {
				b = NULL;
				break;
			}
		}

		if (b != NULL) 
        {
			uint8 flags;
			bool error = false;
            #ifndef FRODO_SC
			    long vicptr;	// File offset of VIC data
            #endif

			while (c != 10)
				c = fgetc(f);	// Shouldn't be necessary
			if (fgetc(f) != 0) {
				ShowRequester("Unknown snapshot format", "OK", NULL);
				fclose(f);
				return false;
			}
			flags = fgetc(f);
            #ifndef FRODO_SC
    			vicptr = ftell(f);
            #endif

			error |= !LoadVICState(f);
			error |= !LoadSIDState(f);
			error |= !LoadCIAState(f);
			error |= !LoadCPUState(f);

			delay = fgetc(f);	// Number of cycles the 6510 is ahead of the previous chips
            #ifdef FRODO_SC
			    // Make the other chips "catch up" with the 6510
			    for (i=0; i<delay; i++) {
				    TheVIC->EmulateCycle();
				    TheCIA1->EmulateCycle();
				    TheCIA2->EmulateCycle();
			    }
            #endif
			if ((flags & SNAPSHOT_1541) != 0) {
				Prefs *prefs = new Prefs(ThePrefs);
	
				// First switch on emulation
				error |= (fread(prefs->DrivePath[0], 256, 1, f) != 1);
				prefs->Emul1541Proc = true;
				NewPrefs(prefs);
				ThePrefs = *prefs;
				delete prefs;
	
				// Then read the context
				error |= !Load1541State(f);
	
				delay = fgetc(f);	// Number of cycles the 6502 is ahead of the previous chips
                #ifdef FRODO_SC
				    // Make the other chips "catch up" with the 6502
				    for (i=0; i<delay; i++) {
					    TheVIC->EmulateCycle();
					    TheCIA1->EmulateCycle();
					    TheCIA2->EmulateCycle();
					    TheCPU->EmulateCycle();
				    }
                #endif
				Load1541JobState(f);
			} else if (ThePrefs.Emul1541Proc) {	// No emulation in snapshot, but currently active?
				Prefs *prefs = new Prefs(ThePrefs);
				prefs->Emul1541Proc = false;
				NewPrefs(prefs);
				ThePrefs = *prefs;
				delete prefs;
			}

            #ifndef FRODO_SC
			    fseek(f, vicptr, SEEK_SET);
			    LoadVICState(f);	// Load VIC data twice in SL (is REALLY necessary sometimes!)
            #endif
			fclose(f);
	
			if (error) {
				ShowRequester("Error reading snapshot file", "OK", NULL);
				Reset();
				return false;
			} else
				return true;
		} else {
			fclose(f);
			ShowRequester("Not a Frodo snapshot file", "OK", NULL);
			return false;
		}
	} else {
		ShowRequester("Can't open snapshot file", "OK", NULL);
		return false;
	}
}

bool C64::loadRomFiles()
{
	FILE *file;

	// Load Basic ROM
	if ((file = fopen(BASIC_ROM_FILE, "rb")) != NULL) {
		if (fread(Basic, 1, 0x2000, file) != 0x2000) {
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
		if (fread(Kernal, 1, 0x2000, file) != 0x2000) {
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
		if (fread(Char, 1, 0x1000, file) != 0x1000) {
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
		if (fread(ROM1541, 1, 0x4000, file) != 0x4000) {
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

bool C64::isCancelled()
{
    return quit_thyself;
}


/*
 *  Stop emulation
 */

void C64::Quit()
{
	// Ask the thread to quit itself if it is running
	quit_thyself = true;
	state_change = true;
}


/*
 *  Pause emulation
 */

void C64::Pause()
{
	TheSID->PauseSound();
	have_a_break = true;
	state_change = true;
}

bool C64::isPaused()
{
    return have_a_break;
}

/*
 *  Resume emulation
 */

void C64::Resume()
{
	TheSID->ResumeSound();
	have_a_break = false;
}

/*
 *  Vertical blank: Poll keyboard and joysticks, update window
 */

void C64::VBlank(bool draw_frame)
{
    //Debug("C64::VBlank\n");

    TheJoystick->update();

	// Poll keyboard
	TheInput->getState(TheCIA1->KeyMatrix, TheCIA1->RevMatrix);

    // Poll joystick
    uint8 joykey = TheJoystick->getState();

	if (!ThePrefs.JoystickSwap) 
    {
        TheCIA1->Joystick1  = 0xff;
		TheCIA1->Joystick2  = joykey;
	}
    else
    {
		TheCIA1->Joystick1  = joykey;
        TheCIA1->Joystick2  = 0xff;
    }

	// Count TOD clocks.
	TheCIA1->CountTOD();
	TheCIA2->CountTOD();

	if (have_a_break)
    {
		return;
    }

    sync();

	if (draw_frame) {
	    // Perform the actual screen update exactly at the
	    // beginning of an interval for the smoothest video.

		TheDisplay->Update();
	}

}


#define ABSOLUTE_TIMING 1
void C64::sync(bool init)
{
    if (init)
    {
        startTime = SDL_GetTicks();
        nextVBlankTime = startTime + ticksPerFrame;
        speedometerUpdateTime = 0;
        return;
    }

    // measure elapsed time
    uint32 currentTime = SDL_GetTicks();

    uint32 elapsedTime = currentTime - startTime;

    //printf("ELAPSED: %d\n", elapsedTime);

	int speed_index = elapsedTime <= 0 ? 999 : ticksPerFrame * 100 / ((int) elapsedTime + 1);

	// limiting the speed to 100%
	if (ThePrefs.LimitSpeed)
    {
        if (currentTime <= nextVBlankTime) 
        {
            uint32 waitTime = nextVBlankTime - currentTime;
            SDL_Delay(waitTime);
        }

        #if ABSOLUTE_TIMING
            nextVBlankTime += ticksPerFrame;
            if (nextVBlankTime < currentTime)
            {
                nextVBlankTime = currentTime;
            }
        #else
            nextVBlankTime = currentTime + ticksPerFrame;
        #endif

        if (speed_index > 100)
        {
            speed_index = 100;
        }
    }

    if (speedometerUpdateTime == 0 || currentTime - speedometerUpdateTime >= 1000)
    {
        TheDisplay->Speedometer((int) speed_index);
        speedometerUpdateTime = currentTime;
    }

    startTime = SDL_GetTicks();
}

/*
 * One emulation step
 */

void C64::emulationStep()
{
    if (quit_thyself)
    {
        return;
    }

    #ifdef FRODO_SC

		EmulateCycles();
		state_change = false;

    #else
		// The order of calls is important here
		int cycles = TheVIC->EmulateLine();
		TheSID->EmulateLine();

        #if !PRECISE_CIA_CYCLES
		    TheCIA1->EmulateLine(ThePrefs.CIACycles);
		    TheCIA2->EmulateLine(ThePrefs.CIACycles);
        #endif

		if (ThePrefs.Emul1541Proc) 
        {
			int cycles_1541 = ThePrefs.FloppyCycles;
			TheCPU1541->CountVIATimers(cycles_1541);

			if (!TheCPU1541->Idle) 
            {
				// 1541 processor active, alternately execute
				//  6502 and 6510 instructions until both have
				//  used up their cycles
				while (cycles >= 0 || cycles_1541 >= 0)
                {
					if (cycles > cycles_1541)
                    {
						cycles -= TheCPU->EmulateLine(1);
                    }
					else
                    {
						cycles_1541 -= TheCPU1541->EmulateLine(1);
                    }
                }
			} 
            else
            {
				TheCPU->EmulateLine(cycles);
            }
		}
        else
        {
			// 1541 processor disabled, only emulate 6510
			TheCPU->EmulateLine(cycles);
        }
    #endif
}

void C64::soundSync()
{
}

int C64::ShowRequester(const char* text, const char* button1, const char* button2)
{
    printf("%s\n", text);

    return 1;
}

#ifdef FRODO_SC

void C64::EmulateCycles()
{
    bool emul1541 = ThePrefs.Emul1541Proc;
    bool vicCycleFinished = false;

	while (!state_change) 
    {

		// The order of calls is important here
        vicCycleFinished = TheVIC->EmulateCycle();

		if (vicCycleFinished)
        {
			TheSID->EmulateLine();
        }

		TheCIA1->CheckIRQs();
		TheCIA2->CheckIRQs();

        #ifndef BATCH_CIA_CYCLES
		    TheCIA1->EmulateCycle();
		    TheCIA2->EmulateCycle();
        #endif

		TheCPU->EmulateCycle();

        if (emul1541)
        {
		    TheCPU1541->CountVIATimers(1);
		    if (!TheCPU1541->Idle)
            {
			    TheCPU1541->EmulateCycle();
            }
        }

		CycleCounter++;
	}

}

#endif
