/*
 *  virtual_joystick.cpp - Virtual Joystick
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#include "sysdeps.h"

#include "main.h"
#include "renderer.h"
#include "texture.h"
#include "font.h"
#include "resources.h"
#include "Display.h"
#include "Input.h"
#include "C64.h"

#include "virtual_joystick.h"

extern int toolbarHeight;
static void swap(int& a, int& b);

VirtualJoystick::VirtualJoystick(C64 *the_c64)
{
    this->TheC64 = the_c64;

    init();
}

VirtualJoystick::~VirtualJoystick()
{
}

void VirtualJoystick::init()
{
    lastActivity = 0;
    state = 0xff;
    stickBits = 0;
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
        lastActivity = 0;
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

void VirtualJoystick::draw(Renderer* renderer, resource_list_t* res)
{
    if (MODE_MOUSE != mode)
    {
        return;
    }

    layout(0, 0, renderer->getWidth(), renderer->getHeight());

    if (!visible) return;

    uint32 currentTicks = SDL_GetTicks();
    uint32 elapsedTicks = currentTicks - lastActivity;

    if (lastActivity > 0 && elapsedTicks > 5000)
    {
        return;
    }

    float x = (float) (rectDeadZone.x + rectDeadZone.w/2 - res->stickTexture->getWidth() / 2);
    float y = (float) (rectDeadZone.y + rectDeadZone.h/2 - res->stickTexture->getHeight() / 2);
    float alpha = 0.0f;
    
    if (elapsedTicks < 350) alpha = 0.4f;                                                   // 0,4
    else if (elapsedTicks < 4000) alpha = 0.4f - (float) (elapsedTicks-350) / 20000.0f;     // 0,4..0,2
    else alpha = 0.2f - (float) (elapsedTicks-4000) / 5000.0f;                              // 0,2..0
    if (alpha < 0.0f) alpha = 0.0f;

    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    renderer->drawTexture(res->stickTexture, x, y);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
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

int VirtualJoystick::getStickState(int mouseButtons, int mouseX, int mouseY,
                                   int mouseButtons2, int mouseX2, int mouseY2)
{
    int leftArea = windowRect.w/2;
    int borderTop = toolbarHeight;
    int borderBottom = toolbarHeight;

    if (mouseButtons != 0 && (mouseY <= windowRect.y + borderTop || mouseY >= windowRect.y + windowRect.h - borderBottom))
    {
        // toolbar area
        mouseButtons = 0;
    }

    if (mouseButtons2 != 0 && (mouseY2 <= windowRect.y + borderTop || mouseY2 >= windowRect.y+windowRect.h - borderBottom))
    {
        // toolbar area
        mouseButtons2 = 0;
    }

    if (mouseButtons == 0 && mouseButtons2 == 0)
    {
        // no input
        return 0x0;
    }

    if (mouseButtons != 0 && mouseButtons2 != 0)
    {
        if (mouseX > mouseX2)
        {
            swap(mouseX, mouseX2);
            swap(mouseY, mouseY2);
            swap(mouseButtons, mouseButtons2);
        }
    }
    else if (mouseX > leftArea)
    {
        mouseX2 = mouseX; mouseX = 0;
        mouseY2 = mouseY; mouseY = 0;
        mouseButtons2 = mouseButtons; mouseButtons = 0;
    }

    /*
	printf("LEFT: %d %d %d  / RIGHT: %d %d %d\n", mouseX, mouseY, mouseButtons,
	                                              mouseX2, mouseY2, mouseButtons2);
    */

    int stick = 0;

    if (mouseButtons & 0x1)
    {
        if (mouseX <  rectDeadZone.x)                    stick |= 0x1; // Left
        if (mouseX >= rectDeadZone.x+rectDeadZone.w)     stick |= 0x2; // Right
        if (mouseY <  rectDeadZone.y)                    stick |= 0x4; // Up
        if (mouseY >= rectDeadZone.y+rectDeadZone.h)     stick |= 0x8; // Down
    }

    if (mouseButtons2 & 0x1)
    {
        stick |= 0x10; // Button
    }

    return stick;
}

void VirtualJoystick::update()
{
    if (TheC64->TheInput->isVirtualKeyboardEnabled())
    {
        state = 0xff;
        stickBits = 0;
        return;
    }

    if (MODE_MOUSE != mode)
    {
        if (MODE_DISABLED == mode)
        {
            state = 0xff;
            stickBits = 0;
        }

        return;
    }

    uint8 oldState = state;
    int lastStickBits = stickBits;

    uint32 currentTicks = SDL_GetTicks();

	int mouseX = 0;
	int mouseY = 0;
	int mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);

    int mouseButtons2 = 0;
    int mouseX2 = 0;
    int mouseY2 = 0;

    #ifdef WEBOS
        mouseButtons2 = SDL_GetMultiMouseState(1, &mouseX2, &mouseY2);
    #endif

    if (mouseButtons == 0 && mouseButtons2 == 0)
    {
        state = 0xff;
        stickBits = 0;
        return;
    }

    stickBits = getStickState(mouseButtons, mouseX, mouseY,
                              mouseButtons2, mouseX2, mouseY2);

    if (stickBits != lastStickBits)
    {
        lastActivity = currentTicks;
    }

    uint8 newState = 0xff;

    if (stickBits & 0x1)     newState &= 0xfb; // Left
    if (stickBits & 0x2)     newState &= 0xf7; // Right
    if (stickBits & 0x4)     newState &= 0xfe; // Up
    if (stickBits & 0x8)     newState &= 0xfd; // Down
    if (stickBits & 0x10)    newState &= 0xef; // Button

    state = newState;
}

void VirtualJoystick::handleMouseEvent(int x, int y, int eventType)
{
    if (!TheC64->TheInput->isVirtualKeyboardEnabled() ||
        InputHandler::EVENT_Down != eventType)
    {
        return;
    }

    int lastStickBits = stickBits;

    stickBits = getStickState(1, x, y, 0, 0, 0);

    if (stickBits != lastStickBits)
    {
        lastActivity = SDL_GetTicks();
    }

    if (0 != stickBits && 0 == lastStickBits) // just when pressing down
    {
             if (stickBits & 0x1)     TheC64->TheInput->pushKeyPress(SDLK_LEFT);  // Left
        else if (stickBits & 0x2)     TheC64->TheInput->pushKeyPress(SDLK_RIGHT); // Right
        else if (stickBits & 0x4)     TheC64->TheInput->pushKeyPress(SDLK_UP);    // Up
        else if (stickBits & 0x8)     TheC64->TheInput->pushKeyPress(SDLK_DOWN);  // Down
    }
}

void VirtualJoystick::handleKeyEvent(int key, int mod, int eventType)
{
    if (mode != MODE_KEYBOARD) 
    {
        return;
    }

    bool key_up = (InputHandler::EVENT_Up == eventType);

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