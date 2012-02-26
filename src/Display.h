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

// Class for C64 graphics display
class C64Display
{
    public:
	    C64 *TheC64;
	    volatile bool quit_requested;
        volatile bool backgroundInvalid;
        volatile bool invalidated;

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

    private:
	    int led_state[4];
	    int old_led_state[4];
        OSD* osd;
	    char speedometer_string[16];		// Speedometer text
        int framesPerSecond;
        int frameCounter;

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

    public:
        void showAbout(bool show);
	    void draw_string(SDL_Surface *s, int x, int y, const char *str, uint8 front_color, uint8 back_color);
        void draw_window(SDL_Surface *s, int x, int y, int w, int h, const char *str, uint8 text_color, uint8 front_color, uint8 back_color);
        void update_led_blinking();         // Update LED

    private:
        void doRedraw();
};

#endif
