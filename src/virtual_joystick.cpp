/*
 *  virtual_joystick.cpp - Virtual Joystick
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#include "sysdeps.h"

#include "virtual_joystick.h"

static void swap(int& a, int& b);

VirtualJoystick::VirtualJoystick()
{
    init();
}

VirtualJoystick::~VirtualJoystick()
{
}


void VirtualJoystick::init()
{
    state = 0xff;
    visible = true;

    deadZoneWidth = deadZoneHeight = 40;

    windowRect.x = windowRect.y = windowRect.w = windowRect.h = 0;
}

int VirtualJoystick::getMode() const
{
    return mode;
}

void VirtualJoystick::setMode(int mode)
{
    this->mode = mode;
}

void VirtualJoystick::show(bool doShow)
{
    if (doShow != visible)
    {
        visible = doShow;
    }
}

bool VirtualJoystick::isShown()
{
    return visible;
}

void VirtualJoystick::layout(int x, int y, int w, int h)
{
    windowRect.x = x;
    windowRect.y = y;
    windowRect.w = w;
    windowRect.h = h;

    int centerX = 90;
    int centerY = windowRect.y + windowRect.h / 2 - 20;

    rectDeadZone.x = centerX - deadZoneWidth / 2;
    rectDeadZone.y = centerY - deadZoneHeight / 2;
    rectDeadZone.w = deadZoneWidth;
    rectDeadZone.h = deadZoneHeight;
}

void VirtualJoystick::draw(SDL_Surface* surface, int y, int h, Uint32 fg, Uint32 bg, Uint32 border)
{
    if (MODE_MOUSE != mode)
    {
        return;
    }

    layout(0, y, surface->w, h);

    // not visible, no input or just button
    if (!visible || 0xff == state || 0xef == state) return;

    SDL_FillRect(surface, &rectDeadZone,  bg);
}

uint8 VirtualJoystick::getState()
{
    return state;
}

bool insideRect(const SDL_Rect& rect, int x, int y)
{
    return (x >= rect.x &&
            y >= rect.y &&
            x < rect.x + rect.w &&
            y < rect.y + rect.h);
}

static void swap(int& a, int& b)
{
    int c = a;
    a = b;
    b = c;
}

void VirtualJoystick::update()
{
    if (MODE_MOUSE != mode)
    {
        if (MODE_DISABLED == mode)
        {
            state = 0xff;
        }

        return;
    }

    int leftArea = windowRect.w/2;

	int mouseX = 0;
	int mouseY = 0;
	int mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);

    if (mouseY <= windowRect.y || mouseY >= windowRect.y + windowRect.h)
    {
        // toolbar area
        mouseButtons = 0;
    }

    int mouseX2 = 0;
    int mouseY2 = 0;

    #ifdef WEBOS
        int mouseButtons2 = SDL_GetMultiMouseState(1, &mouseX2, &mouseY2);
        if (mouseY2 <= windowRect.y || mouseY2 >= windowRect.y+windowRect.h)
        {
            mouseButtons2 = 0;
        }

        if (mouseButtons == 0 && mouseButtons2 == 0)
        {
            // no touch input
            state = 0xff;
            return;
        }

    #else
        int mouseButtons2 = 0;
    #endif

    if (mouseButtons == 0 && mouseButtons2 == 0)
    {
        state = 0xff;
        return;
    }

    #ifdef WEBOS

        if (mouseButtons != 0 && mouseButtons2 != 0 && mouseX > mouseX2)
        {
            swap(mouseX, mouseX2);
            swap(mouseY, mouseY2);
            swap(mouseButtons, mouseButtons2);
        }
        else if (mouseX > leftArea)
        {
            mouseX2 = mouseX; mouseX = 0;
            mouseY2 = mouseY; mouseY = 0;
            mouseButtons2 = mouseButtons; mouseButtons = 0;
        }

    #else

        if (mouseX > leftArea)
        {
            mouseX2 = mouseX; mouseX = 0;
            mouseY2 = mouseY; mouseY = 0;
            mouseButtons2 = mouseButtons; mouseButtons = 0;
        }

    #endif

    /*
	printf("LEFT: %d %d %d  / RIGHT: %d %d %d\n", mouseX, mouseY, mouseButtons,
	                                              mouseX2, mouseY2, mouseButtons2);
    */

    uint8 newState = 0xff;

    if (mouseButtons & 0x1)
    {
        if (mouseX <  rectDeadZone.x)                    newState &= 0xfb; // Left
        if (mouseX >= rectDeadZone.x+rectDeadZone.w)     newState &= 0xf7; // Right
        if (mouseY <  rectDeadZone.y)                    newState &= 0xfe; // Up
        if (mouseY >= rectDeadZone.y+rectDeadZone.h)     newState &= 0xfd; // Down
    }

    if (mouseButtons2 & 0x1)
    {
        newState &= 0xef; // Button
    }

    state = newState;

}

void VirtualJoystick::keyInput(int key, bool press)
{
    if (mode != MODE_KEYBOARD) 
    {
        return;
    }

    bool key_up = !press;

	if (SDLK_LCTRL == key) {

		if (key_up) 
        {
			state |= ~0xef; // Button
		}
		else 
        {
			state &= 0xef; // Button
		}

	}
	else if (SDLK_LEFT == key) {

		if (key_up) {
			state |= ~0xfb; // Left
		}
		else {
			state &= 0xfb; // Left
		}
	}
	else if (SDLK_RIGHT == key) {

		if (key_up) {
			state |= ~0xf7; // Right
		}
		else {
			state &= 0xf7; // Right
		}
	}
	else if (SDLK_UP == key) {

		if (key_up) {
			state |= ~0xfe; // Up
		}
		else {
			state &= 0xfe; // Up
		}
	}
	else if (SDLK_DOWN == key) {

		if (key_up) {
			state |= ~0xfd; // Down
		}
		else {
			state &= 0xfd; // Down
		}

	}
}