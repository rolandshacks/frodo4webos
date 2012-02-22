/*
 *  virtual_joystick.h - Virtual Joystick
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#ifndef _VIRTUAL_JOYSTICK_H
#define _VIRTUAL_JOYSTICK_H

#define INPUT_BUTTON	0x10
#define INPUT_LEFT		0x1
#define INPUT_RIGHT		0x2
#define INPUT_UP		0x4
#define INPUT_DOWN		0x8

class VirtualJoystick
{
    private:
        C64* the_c64;
        C64Display* display;
        bool visible;

        SDL_Rect windowRect;

        SDL_Rect rectDeadZone;
        int deadZoneWidth;
        int deadZoneHeight;

        int state;

    public:
        VirtualJoystick();
        virtual ~VirtualJoystick();

    public:
        void create(C64 *the_c64, C64Display* display);
        void show(bool doShow);
        bool isShown();
        void draw(SDL_Surface* surface, Uint32 fg, Uint32 bg, Uint32 border);
        int updateState(int buttons, int x, int y);
        int getState();

    private:
        void init();
        void cleanup();
        void layout(int w, int h);

};

#endif /* _VIRTUAL_JOYSTICK_H */
