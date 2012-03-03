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
    public:
        typedef enum
        {
            MODE_DISABLED = 0,
            MODE_KEYBOARD = 1,
            MODE_MOUSE    = 2
        } stickmode_t;

    private:
        bool visible;
        int mode;

        SDL_Rect windowRect;
        SDL_Rect rectDeadZone;
        int deadZoneWidth;
        int deadZoneHeight;

        uint8 state;
        uint32 lastActivity;

    public:
        VirtualJoystick();
        virtual ~VirtualJoystick();
        int getMode() const;
        void setMode(int mode);
        void update();
        uint8 getState();
        void keyInput(int key, bool press);

    public:
        void create();
        void show(bool doShow);
        bool isShown();
        void draw(Renderer* renderer, resource_list_t* res);
        int updateState(int buttons, int x, int y);
        void keyInput(int key);

    private:
        void init();
        void cleanup();
        void layout(int x, int y, int w, int h);

};

#endif /* _VIRTUAL_JOYSTICK_H */
