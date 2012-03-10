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

class VirtualJoystick : public InputHandler
{
    public:
        typedef enum
        {
            MODE_DISABLED = 0,
            MODE_KEYBOARD = 1,
            MODE_MOUSE    = 2
        } stickmode_t;

    private:
	    C64 *TheC64;
        bool visible;
        int mode;

        SDL_Rect windowRect;
        SDL_Rect rectDeadZone;
        int deadZoneWidth;
        int deadZoneHeight;

        int stickBits;
        uint8 state;
        uint32 lastActivity;

    public:
        VirtualJoystick(C64 *the_c64);
        virtual ~VirtualJoystick();
        int getMode() const;
        void setMode(int mode);
        void update();
        uint8 getState();

    public: // implements InputHandler
        void handleMouseEvent(int x, int y, int eventType);
        void handleKeyEvent(int key, int mod, int eventType);

    public:
        void create();
        void show(bool doShow);
        bool isShown();
        void draw(Renderer* renderer, resource_list_t* res);
        //int updateState(int buttons, int x, int y);
        void keyInput(int key);

    private:
        void init();
        void cleanup();
        void layout(int x, int y, int w, int h);
        int getStickState(int mouseButtons, int mouseX, int mouseY,
                          int mouseButtons2, int mouseX2, int mouseY2);

};

#endif /* _VIRTUAL_JOYSTICK_H */
