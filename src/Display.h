/*
 *  Display.h - C64 graphics display, emulator window handling
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#ifndef _DISPLAY_H
#define _DISPLAY_H

struct SDL_Surface;

// Display dimensions
#if defined(SMALL_DISPLAY)
const int DISPLAY_X = 0x168;
const int DISPLAY_Y = 0x110;
#else
const int DISPLAY_X = 0x180;
const int DISPLAY_Y = 0x110;
#endif

class C64Window;
class C64Screen;
class C64;
class Prefs;
class OSD;
class VirtualJoystick;

#if USE_OPENGL
class Renderer;
class Texture;
class Font;
#endif

#include <string>
#include "resources.h"

// Class for C64 graphics display
class C64Display
{
    public:
	    C64 *TheC64;
	    volatile bool quit_requested;
        volatile bool backgroundInvalid;
        volatile bool invalidated;

    public:
        resource_list_t res;

    private:
        // use tripple buffering to make
        // input and display fully independent
        uint8* back_buffer;
        uint8* mid_buffer;
        uint8* front_buffer;
        SDL_mutex* bufferLock;

        int bufferSize;
        int bufferWidth;
        int bufferHeight;
        int bufferBitsPerPixel;
        int bufferPitch;

        float viewX;
        float viewY;
        float viewW;
        float viewH;

        uint32 lastDrawTime;
        float elapsedTime;

    private:
	    int led_state[4];
	    int old_led_state[4];
        OSD* osd;
	    char speedometer_string[16];		// Speedometer text
        int framesPerSecond;
        int frameCounter;
        bool antialiasing;

    public:
	    C64Display(C64 *the_c64);
	    ~C64Display();

        void invalidateBackground();
        void resize(int w, int h);

    public:
        bool init();
	    void Update(void);

        void redraw();

	    void UpdateLEDs(int l0, int l1, int l2, int l3);
	    void Speedometer(int speed);
	    uint8 *BitmapBase(void);
	    int BitmapXMod(void);
	    void InitColors(uint8 *colors);
	    void NewPrefs(Prefs *prefs);

        void setAntialiasing(bool antialiasing);
        bool getAntialiasing() const;

    public:
        int getWidth();
        int getHeight();

    public:
        void openOsd();
        void closeOsd();
        bool isOsdActive();
        void updateOsd(SDL_Event* event);
        OSD* getOsd();

        void toggleStatusBar();
        void openAbout();
        void closeAbout();
        bool isAboutActive() const;

    public:
        void showAbout(bool show);
        void update_led_blinking();

    private:
        void doRedrawGL();
        void doInitGL();
        void doFreeGL();

    private:
        void drawScreen();
        void drawAbout();
        void drawVirtualJoystick();

    private:
        std::string statusText;
        float statusTextTimeout;
        void drawStatusBar();

    public:
        void setStatusMessage(const std::string& message, float timeOut=0.0f);


    #if USE_OPENGL
        Renderer* renderer;
    #endif
};

#endif
