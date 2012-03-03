/*
 *  Display.h - C64 graphics display, emulator window handling
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#ifndef _INPUT_H
#define _INPUT_H

class C64;
class Prefs;
class VirtualJoystick;

// Class for C64 graphics display
class C64Input : public InputHandler
{
    private:
        int joystickMode;
        bool enableVirtualKeyboard;

        SDL_mutex* inputLock;

    public:
	    C64 *TheC64;
        C64Display* TheDisplay;
	    bool quit_requested;

    public:
	    C64Input(C64* the_c64, C64Display* the_display);
	    ~C64Input();
	    void getState(uint8 *key_matrix, uint8 *rev_matrix);

        void toggleVirtualKeyboard();
        bool isVirtualKeyboardEnabled();
        bool isQuitRequested() const;

    public: // implements InputHandler
        void handleMouseEvent(int x, int y, int eventType);
        void handleKeyEvent(int key, int sym, int eventType);

    public:
        void pushKey(int key);
        void pushKeyPress(int key);
        void pushKeySequence(int* keys, int count);

    private:
        int popKey();
        void translateKey(SDLKey key, bool key_up, uint8 *key_matrix, uint8 *rev_matrix);
};

#endif /* _INPUT_H */
