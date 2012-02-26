/*
 *  Display.cpp - C64 graphics display, emulator window handling
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#include "sysdeps.h"
#include "Version.h"

#include "main.h"
#include "Display.h"
#include "Input.h"
#include "C64.h"
#include "Prefs.h"
#include "virtual_joystick.h"

extern int toolbarHeight;
extern int toolbarWidgetWidth;

/*
  C64 keyboard matrix:

    Bit 7   6   5   4   3   2   1   0
  0    CUD  F5  F3  F1  F7 CLR RET DEL
  1    SHL  E   S   Z   4   A   W   3
  2     X   T   F   C   6   D   R   5
  3     V   U   H   B   8   G   Y   7
  4     N   O   K   M   0   J   I   9
  5     ,   @   :   .   -   L   P   +
  6     /   ^   =  SHR HOM  ;   *   ?
  7    R/S  Q   C= SPC  2  CTL  <-  1
*/


static const int key_queue_size = 512;
static int key_queue_elements = 0;
static int key_queue_inptr = 0;
static int key_queue_outptr = 0;
static int key_queue[key_queue_size];

int SEQUENCE_LOAD[] = { 108, -108, 111, -111, 97, -97, 100, -100, 32, -32, 304, 50, -50, 52, -52, 50, -50, -304, 44, -44, 56, -56, 13, -13 };
int SEQUENCE_LIST[] = { 108, -108, 105, -105, 115, -115, 116, -116, 13, -13 };
int SEQUENCE_LOADPGM[] = { 273, -273, 273, -273, 273, -273, 108, -108, 111, -111, 97, -97, 100, -100, 275, -275, 275, -275, 275, -275, 275, -275,
 275, -275, 275, -275, 275, -275, 275, -275, 275, -275, 275, -275, 275, -275, 275, -275, 275, -275, 275, -275, 275, -275
 , 275, -275, 275, -275, 275, -275, 275, -275, 275, -275, 44, -44, 56, -56, 44, -44, 49, -49, 13, -13 };
int SEQUENCE_RUN[] = { 114, -114, 117, -117, 110, -110, 13, -13 };
int SEQUENCE_LOADFIRST[] = { 108, -108, 111, -111, 97, 100, -97, -100, 32, -32, 304, 50, -50, -304, 93, -93, 304, 50, -50, -304, 44, -44, 56, -56, 44, -44, 49, -49, 13, -13 };


#define MATRIX(a,b) (((a) << 3) | (b))

C64Input::C64Input(C64 *the_c64, C64Display* the_display)
{
    TheC64 = the_c64;
    TheDisplay = the_display;

	quit_requested = false;
    enableVirtualKeyboard = false;

    inputLock = SDL_CreateMutex();
}

C64Input::~C64Input()
{
    if (NULL != inputLock)
    {
        SDL_DestroyMutex(inputLock);
        inputLock = NULL;
    }
}

bool C64Input::isQuitRequested() const
{
    return quit_requested;
}

void C64Input::pushKey(int key)
{
    // printf(", %d", key); if (key == -13) printf("\n");

    if (0 == SDL_LockMutex(inputLock))
    {

        if (key_queue_elements < key_queue_size)
        {
            key_queue_elements++;
            key_queue[key_queue_inptr++] = key;
            if (key_queue_inptr >= key_queue_size) key_queue_inptr = 0;
        }

        SDL_UnlockMutex(inputLock);
    }

}

void C64Input::pushKeyPress(int key)
{
    pushKey(key);
    pushKey(-key);
}

void C64Input::pushKeySequence(int* keys, int count)
{
    for (int i=0; i<count; i++)
   {
        pushKey(keys[i]);
    }
}

int C64Input::popKey()
{
    //if (0 != key_queue_elements) printf("%d\n", key_queue_elements);

    int sym = 0;

    if (0 == SDL_LockMutex(inputLock))
    {

        if (key_queue_elements > 0)
        {

            key_queue_elements--;

            sym = key_queue[key_queue_outptr++];
            if (key_queue_outptr >= key_queue_size) key_queue_outptr = 0;
        }

        SDL_UnlockMutex(inputLock);
    }

    return sym;
}

void C64Input::translateKey(SDLKey key,
                            bool key_up,
                            uint8 *key_matrix,
                            uint8 *rev_matrix)
{
	int c64_key = -1;

    printf("KEY: %d\n", key);

	switch (key) 
    {
		case SDLK_a: c64_key = MATRIX(1,2); break;
		case SDLK_b: c64_key = MATRIX(3,4); break;
		case SDLK_c: c64_key = MATRIX(2,4); break;
		case SDLK_d: c64_key = MATRIX(2,2); break;
		case SDLK_e: c64_key = MATRIX(1,6); break;
		case SDLK_f: c64_key = MATRIX(2,5); break;
		case SDLK_g: c64_key = MATRIX(3,2); break;
		case SDLK_h: c64_key = MATRIX(3,5); break;
		case SDLK_i: c64_key = MATRIX(4,1); break;
		case SDLK_j: c64_key = MATRIX(4,2); break;
		case SDLK_k: c64_key = MATRIX(4,5); break;
		case SDLK_l: c64_key = MATRIX(5,2); break;
		case SDLK_m: c64_key = MATRIX(4,4); break;
		case SDLK_n: c64_key = MATRIX(4,7); break;
		case SDLK_o: c64_key = MATRIX(4,6); break;
		case SDLK_p: c64_key = MATRIX(5,1); break;
		case SDLK_q: c64_key = MATRIX(7,6); break;
		case SDLK_r: c64_key = MATRIX(2,1); break;
		case SDLK_s: c64_key = MATRIX(1,5); break;
		case SDLK_t: c64_key = MATRIX(2,6); break;
		case SDLK_u: c64_key = MATRIX(3,6); break;
		case SDLK_v: c64_key = MATRIX(3,7); break;
		case SDLK_w: c64_key = MATRIX(1,1); break;
		case SDLK_x: c64_key = MATRIX(2,7); break;
		case SDLK_y: c64_key = MATRIX(3,1); break;
		case SDLK_z: c64_key = MATRIX(1,4); break;

		case SDLK_0: c64_key = MATRIX(4,3); break;
		case SDLK_1: c64_key = MATRIX(7,0); break;
		case SDLK_2: c64_key = MATRIX(7,3); break;
		case SDLK_3: c64_key = MATRIX(1,0); break;
		case SDLK_4: c64_key = MATRIX(1,3); break;
		case SDLK_5: c64_key = MATRIX(2,0); break;
		case SDLK_6: c64_key = MATRIX(2,3); break;
		case SDLK_7: c64_key = MATRIX(3,0); break;
		case SDLK_8: c64_key = MATRIX(3,3); break;
		case SDLK_9: c64_key = MATRIX(4,0); break;

		case SDLK_SPACE: c64_key = MATRIX(7,4); break;
		case SDLK_BACKQUOTE: c64_key = MATRIX(7,1); break;
		case SDLK_BACKSLASH: c64_key = MATRIX(6,6); break;
		case SDLK_COMMA: c64_key = MATRIX(5,7); break;
		case SDLK_PERIOD: c64_key = MATRIX(5,4); break;
		case SDLK_MINUS: c64_key = MATRIX(5,0); break;
		case SDLK_EQUALS: c64_key = MATRIX(6,5); break;
		case SDLK_LEFTBRACKET: c64_key = MATRIX(5,6); break;
		case SDLK_RIGHTBRACKET: c64_key = MATRIX(6,1); break;
		case SDLK_SEMICOLON: c64_key = MATRIX(5,5); break;
		case SDLK_QUOTE: c64_key = MATRIX(6,2); break;
		case SDLK_SLASH: c64_key = MATRIX(6,7); break;
		case SDLK_QUESTION: c64_key = (int) (MATRIX(6,7) | 0x80); break;

		case SDLK_ESCAPE: c64_key = MATRIX(7,7); break;
		case SDLK_RETURN: c64_key = MATRIX(0,1); break;
		case SDLK_BACKSPACE: case SDLK_DELETE: c64_key = MATRIX(0,0); break;
		case SDLK_INSERT: c64_key = MATRIX(6,3); break;
		case SDLK_HOME: c64_key = MATRIX(6,3); break;
		case SDLK_END: c64_key = MATRIX(6,0); break;
		case SDLK_PAGEUP: c64_key = MATRIX(6,0); break;
		case SDLK_PAGEDOWN: c64_key = MATRIX(6,5); break;

		case SDLK_LCTRL: case SDLK_TAB: c64_key = MATRIX(7,2); break;
		case SDLK_RCTRL: c64_key = MATRIX(7,5); break;
		case SDLK_LSHIFT: c64_key = MATRIX(1,7); break;
		case SDLK_RSHIFT: c64_key = MATRIX(6,4); break;
		case SDLK_LALT: case SDLK_LMETA: c64_key = MATRIX(7,5); break;
		case SDLK_RALT: case SDLK_RMETA: c64_key = MATRIX(7,5); break;

		case SDLK_UP: c64_key = MATRIX(0,7)| 0x80; break;
		case SDLK_DOWN: c64_key = MATRIX(0,7); break;
		case SDLK_LEFT: c64_key = MATRIX(0,2) | 0x80; break;
		case SDLK_RIGHT: c64_key = MATRIX(0,2); break;

		case SDLK_F1: c64_key = MATRIX(0,4); break;
		case SDLK_F2: c64_key = MATRIX(0,4) | 0x80; break;
		case SDLK_F3: c64_key = MATRIX(0,5); break;
		case SDLK_F4: c64_key = MATRIX(0,5) | 0x80; break;
		case SDLK_F5: c64_key = MATRIX(0,6); break;
		case SDLK_F6: c64_key = MATRIX(0,6) | 0x80; break;
		case SDLK_F7: c64_key = MATRIX(0,3); break;
		case SDLK_F8: c64_key = MATRIX(0,3) | 0x80; break;

		case SDLK_KP0: case SDLK_KP5: c64_key = 0x10 | 0x40; break;
		case SDLK_KP1: c64_key = 0x06 | 0x40; break;
		case SDLK_KP2: c64_key = 0x02 | 0x40; break;
		case SDLK_KP3: c64_key = 0x0a | 0x40; break;
		case SDLK_KP4: c64_key = 0x04 | 0x40; break;
		case SDLK_KP6: c64_key = 0x08 | 0x40; break;
		case SDLK_KP7: c64_key = 0x05 | 0x40; break;
		case SDLK_KP8: c64_key = 0x01 | 0x40; break;
		case SDLK_KP9: c64_key = 0x09 | 0x40; break;

		case SDLK_KP_DIVIDE: c64_key = MATRIX(6,7); break;
		case SDLK_KP_ENTER: c64_key = MATRIX(0,1); break;
	}

    // if (c64_key < 0)
    {
        if (key == SDLK_QUOTEDBL)
        {
            c64_key = (int) (0x3b | 0x80);
        }
        else if (key == SDLK_DOLLAR)
        {
            c64_key = (int) (0xb | 0x80);
        }
    }

    //printf("C64KEY: %x\n", c64_key);

	if (c64_key < 0)
    {
		return;
    }

	// Handle other keys
	bool shifted = c64_key & 0x80;
	int c64_byte = (c64_key >> 3) & 7;
	int c64_bit  = c64_key & 7;

	if (key_up)
    {
		if (shifted) 
        {
			key_matrix[6]    |= 0x10;
			rev_matrix[4]    |= 0x40;
		}
		key_matrix[c64_byte] |= (1 << c64_bit);
		rev_matrix[c64_bit]  |= (1 << c64_byte);
	}
    else
    {
		if (shifted) 
        {
			key_matrix[6]    &= 0xef;
			rev_matrix[4]    &= 0xbf;
		}
		key_matrix[c64_byte] &= ~(1 << c64_bit);
		rev_matrix[c64_bit]  &= ~(1 << c64_byte);
	}

    /*
    printf("\nTRANSLATE: %d %s - BIT: %d BYTE: %d\n",
           (int) key, key_up ? "up" : "down", c64_bit, c64_byte);

    for (int i=0; i<8; i++) { printf("%02x ", key_matrix[i]); }
    printf("\n");

    for (int i=0; i<8; i++) { printf("%02x ", rev_matrix[i]); }
    printf("\n");
    */
}

void C64Input::toggleVirtualKeyboard()
{
    enableVirtualKeyboard = enableVirtualKeyboard ? false : true;

	#ifdef WEBOS
        PDL_SetKeyboardState(enableVirtualKeyboard ? PDL_TRUE : PDL_FALSE);
	#endif
}

bool C64Input::isVirtualKeyboardEnabled()
{
    return enableVirtualKeyboard;
}

void C64Input::getState(uint8 *key_matrix, uint8 *rev_matrix)
{
    int keyInput = popKey();
    if (0 != keyInput)
    {
        if (keyInput > 0)
        {
            translateKey((SDLKey) keyInput, false, key_matrix, rev_matrix);
        }
        else
        {
            translateKey((SDLKey) -keyInput, true, key_matrix, rev_matrix);
        }
    }
}

static void swap(int& a, int& b)
{
    int c = a;
    a = b;
    b = c;
}

void C64Input::handleKeyEvent(int key, int mod, bool press)
{
    TheDisplay->closeAbout();

    if (press)
    {
        pushKey((int) key);
    }
    else
    {
        #ifdef WEBOS
            if (enableVirtualKeyboard && key == PDLK_GESTURE_DISMISS_KEYBOARD)
            {
                toggleVirtualKeyboard();
                return;
            }
        #endif

        pushKey(- ((int) key));
    }
}

void C64Input::handleMouseEvent(int x, int y, bool press)
{
    TheDisplay->closeAbout();

    if (press)
    {
        /* */
    }
    else
    {
        if (y < toolbarHeight)
        {
            if (x < toolbarWidgetWidth)
            {
                pushKeyPress(27); // ESC = RUN/STOP
            }
            else if (x > TheDisplay->getWidth() - toolbarWidgetWidth)
            {
                TheC64->Reset(); pushKey(0);pushKey(0);pushKey(0);pushKey(0);pushKey(0);
                pushKeySequence(SEQUENCE_LOADFIRST, sizeof(SEQUENCE_LOADFIRST)/sizeof(SEQUENCE_LOADFIRST[0]));
                pushKeySequence(SEQUENCE_RUN, sizeof(SEQUENCE_RUN)/sizeof(SEQUENCE_RUN[0]));
                if (enableVirtualKeyboard)
                {
                    toggleVirtualKeyboard();
                }
            }
            else
            {
                TheDisplay->openOsd();
            }
        }
        else if (y > TheDisplay->getHeight() - toolbarHeight)
        {
            if (x < toolbarWidgetWidth)
            {
                TheDisplay->toggleStatusBar();
            }
            else if (x > TheDisplay->getWidth() - toolbarWidgetWidth)
            {
                toggleVirtualKeyboard();
                TheDisplay->invalidateBackground();
            }
            else
            {
                pushKeyPress(32); // SPACE
            }
        }
    }
}
