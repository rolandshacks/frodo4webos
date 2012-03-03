/*
 *  virtual_joystick.cpp - Virtual Joystick
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#include "sysdeps.h"

#include "renderer.h"
#include "texture.h"
#include "font.h"
#include "resources.h"

#include "virtual_joystick.h"

extern int toolbarHeight;
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
    lastActivity = 0;
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

    // not visible, no input or just button
    if (!visible) return; // || 0xff == state || 0xef == state) return;

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

    uint32 currentTicks = SDL_GetTicks();

    int leftArea = windowRect.w/2;
    int borderTop = toolbarHeight;
    int borderBottom = toolbarHeight;

	int mouseX = 0;
	int mouseY = 0;
	int mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);

    if (mouseButtons != 0 && (mouseY <= windowRect.y + borderTop || mouseY >= windowRect.y + windowRect.h - borderBottom))
    {
        // toolbar area
        mouseButtons = 0;
    }

    int mouseX2 = 0;
    int mouseY2 = 0;

    #ifdef WEBOS
        int mouseButtons2 = SDL_GetMultiMouseState(1, &mouseX2, &mouseY2);
        if (mouseY2 <= windowRect.y + borderTop || mouseY2 >= windowRect.y+windowRect.h - borderBottom)
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

    if (state != 0xff)
    {
        lastActivity = currentTicks;
    }
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