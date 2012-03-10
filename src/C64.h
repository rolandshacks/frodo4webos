/*
 *  C64.h - Put the pieces together
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#ifndef _C64_H
#define _C64_H

#include <SDL.h>

// false: Frodo, true: FrodoSC
extern bool IsFrodoSC;

class Prefs;
class C64Display;
class C64Input;
class MOS6510;
class MOS6569;
class MOS6581;
class MOS6526_1;
class MOS6526_2;
class IEC;
class REU;
class MOS6502_1541;
class Job1541;
class CmdPipe;
class VirtualJoystick;

class C64 {
    public:
	    C64();
	    ~C64();

        bool init(void);
        void shutdown();
        void doStep();
        bool isCancelled();

    public:
	    void Quit(void);
	    void Pause(void);
	    void Resume(void);
	    void Reset(void);
	    void NMI(void);
	    void VBlank(bool draw_frame);
	    void NewPrefs(Prefs *prefs);
	    void PatchKernal(bool fast_reset, bool emul_1541_proc);
	    void SaveRAM(char *filename);
	    void SaveSnapshot(const char *filename);
	    bool LoadSnapshot(const char *filename);
	    int SaveCPUState(FILE *f);
	    int Save1541State(FILE *f);
	    bool Save1541JobState(FILE *f);
	    bool SaveVICState(FILE *f);
	    bool SaveSIDState(FILE *f);
	    bool SaveCIAState(FILE *f);
	    bool LoadCPUState(FILE *f);
	    bool Load1541State(FILE *f);
	    bool Load1541JobState(FILE *f);
	    bool LoadVICState(FILE *f);
	    bool LoadSIDState(FILE *f);
	    bool LoadCIAState(FILE *f);
        bool isPaused();

        int ShowRequester(const char* text, const char* button1=NULL, const char* button2=NULL);
        void soundSync();

    public:
	    uint8 *RAM, *Basic, *Kernal,
		      *Char, *Color;		// C64
	    uint8 *RAM1541, *ROM1541;	// 1541

	    C64Display* TheDisplay;
        C64Input*   TheInput;
        VirtualJoystick* TheJoystick;

	    MOS6510 *TheCPU;			// C64
	    MOS6569 *TheVIC;
	    MOS6581 *TheSID;
	    MOS6526_1 *TheCIA1;
	    MOS6526_2 *TheCIA2;
	    IEC *TheIEC;
	    REU *TheREU;

	    MOS6502_1541 *TheCPU1541;	// 1541
	    Job1541 *TheJob1541;
        
    private:
        bool loadRomFiles();
	    void emulationStep(void);
        void sync(bool init=false);

	    bool quit_thyself;		// Emulation thread shall quit
	    bool have_a_break;		// Emulation thread shall pause
        bool paused;            // Emulation thread paused

	    int joy_minx, joy_maxx, joy_miny, joy_maxy; // For dynamic joystick calibration

	    uint8 orig_kernal_1d84,	// Original contents of kernal locations $1d84 and $1d85
		      orig_kernal_1d85;	// (for undoing the Fast Reset patch)

        uint32 ticksPerFrame;
        uint32 startTime;
        uint32 speedometerUpdateTime;
        uint32 nextVBlankTime;
	    uint8 joy_state;			// Current state of joystick
	    bool state_change;

        SDL_Joystick *joystick1;     // joystick 1
        SDL_Joystick *joystick2;     // joystick 2


    public:
        #ifdef FRODO_SC
    	    uint32 CycleCounter;
    	    void EmulateCycles();
        #endif

        
};

#endif
