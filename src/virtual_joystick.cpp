/*
 *  virtual_joystick.cpp - Virtual Joystick
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#include "sysdeps.h"

#include "C64.h"
#include "Display.h"
#include "main.h"
#include "Prefs.h"
#include "ndir.h"
#include "virtual_joystick.h"

VirtualJoystick::VirtualJoystick()
{
    init();
}

VirtualJoystick::~VirtualJoystick()
{
    cleanup();
}

void VirtualJoystick::init()
{
    state = 0;
    visible = true;

    deadZoneWidth = deadZoneHeight = 40;

    windowRect.x = windowRect.y = windowRect.w = windowRect.h = 0;
}

void VirtualJoystick::create(C64* the_c64, C64Display* display)
{
    this->the_c64 = the_c64;
    this->display = display;
}

void VirtualJoystick::cleanup()
{
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

void VirtualJoystick::layout(int w, int h)
{
    windowRect.x = 0;
    windowRect.y = 0;
    windowRect.w = w;
    windowRect.h = h;

    int centerX = 90;
    int centerY = windowRect.h / 2 - 20;

    rectDeadZone.x = centerX - deadZoneWidth / 2;
    rectDeadZone.y = centerY - deadZoneHeight / 2;
    rectDeadZone.w = deadZoneWidth;
    rectDeadZone.h = deadZoneHeight;


}

void VirtualJoystick::draw(SDL_Surface* surface, Uint32 fg, Uint32 bg, Uint32 border)
{
    if (!visible) return;

    layout(surface->w, surface->h);

    if (0 == state) return;

    SDL_FillRect(surface, &rectDeadZone,  bg);
}

int VirtualJoystick::getState()
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

int VirtualJoystick::updateState(int buttons, int x, int y)
{
    int newState = 0;

    if (0 == (buttons & 0x1)) 
    {
        state = 0;
        return state;
    }

    if (x < rectDeadZone.x)                     newState |= INPUT_LEFT;
    if (x >= rectDeadZone.x+rectDeadZone.w)     newState |= INPUT_RIGHT;
    if (y < rectDeadZone.y)                     newState |= INPUT_UP;
    if (y >= rectDeadZone.y+rectDeadZone.h)     newState |= INPUT_DOWN;

    state = newState;
    return state;
}
