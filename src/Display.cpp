/*
 *  Display.cpp - C64 graphics display, emulator window handling
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#include "sysdeps.h"
#include "Version.h"

#include "main.h"
#include "C64.h"
#include "Display.h"
#include "Input.h"
#include "Prefs.h"
#include "SAM.h"
#include "osd.h"
#include "virtual_joystick.h"

extern bool run_async_emulation;
static bool swapBuffers = true;

int initialWidth = 1024;
int initialHeight = 768;
//int initialWidth = 350;
//int initialHeight = 250;
int bitsPerPixel = 8;
int zoom_factor = 1;

#ifdef WEBOS
  bool enableTitle = true;
#else
  bool enableTitle = false;
#endif

bool enableScaling = true;
bool enabledStatusBar = false;
bool enabledOSD = true;

int toolbarHeight = 40;
int toolbarWidgetWidth = 50;

// Display surface
static SDL_Surface *physicalScreen = NULL;

// For LED error blinking
static C64Display *c64_disp;
static Uint32 lastLEDUpdate = 0;


// LED states
enum {
	LED_OFF,		// LED off
	LED_ON,			// LED on (green)
	LED_ERROR_ON,	// LED blinking (red), currently on
	LED_ERROR_OFF	// LED blinking, currently off
};

#undef USE_THEORETICAL_COLORS

#ifdef USE_THEORETICAL_COLORS

// C64 color palette (theoretical values)
const uint8 palette_red[16] = {
	0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0x80, 0xff, 0x40, 0x80, 0x80, 0x80, 0xc0
};

const uint8 palette_green[16] = {
	0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x80, 0x40, 0x80, 0x40, 0x80, 0xff, 0x80, 0xc0
};

const uint8 palette_blue[16] = {
	0x00, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0x80, 0x40, 0x80, 0x80, 0xff, 0xc0
};

#else

// C64 color palette (more realistic looking colors)
const uint8 palette_red[16] = {
	0x00, 0xff, 0x99, 0x00, 0xcc, 0x44, 0x11, 0xff, 0xaa, 0x66, 0xff, 0x40, 0x80, 0x66, 0x77, 0xc0
};

const uint8 palette_green[16] = {
	0x00, 0xff, 0x00, 0xff, 0x00, 0xcc, 0x00, 0xff, 0x55, 0x33, 0x66, 0x40, 0x80, 0xff, 0x77, 0xc0
};

const uint8 palette_blue[16] = {
	0x00, 0xff, 0x00, 0xcc, 0xcc, 0x44, 0x99, 0x00, 0x00, 0x00, 0x66, 0x40, 0x80, 0x66, 0xff, 0xc0
};

#endif

// Colors for speedometer/drive LEDs
enum {
	black = 0,
	white = 1,
	fill_gray = 16,
	shine_gray = 17,
	shadow_gray = 18,
	red = 19,
	green = 20,
	PALETTE_SIZE = 21
};

SDL_Color palette[PALETTE_SIZE];

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

#define MATRIX(a,b) (((a) << 3) | (b))

/*
 *  Display constructor
 */

C64Display::C64Display(C64 *the_c64) : TheC64(the_c64)
{
	quit_requested = false;
	speedometer_string[0] = 0;

	// LEDs off
	for (int i=0; i<4; i++)
		led_state[i] = old_led_state[i] = LED_OFF;

	// Start timer for LED error blinking
	c64_disp = this;

    if (enabledOSD)
    {
        osd = new OSD();
        osd->create(the_c64, this);
    }
    else
    {
        osd = NULL;
    }

    framesPerSecond = 0;
    frameCounter = 0;

    backgroundInvalid = true;
    invalidated = true;
}

/*
 *  Display destructor
 */

C64Display::~C64Display()
{
    if (NULL != back_buffer)
    {
        delete [] back_buffer;
        back_buffer = NULL;
    }

    if (NULL != mid_buffer)
    {
        delete [] mid_buffer;
        mid_buffer = NULL;
    }

    if (NULL != front_buffer)
    {
        delete [] front_buffer;
        front_buffer = NULL;
    }

    if (NULL != bufferLock)
    {
        SDL_DestroyMutex(bufferLock);
        bufferLock = NULL;
    }

    if (NULL != osd)
    {
        delete osd;
        osd = NULL;
    }

}

bool C64Display::init()
{
	// Open window
    
    #ifdef WEBOS
	    physicalScreen = SDL_SetVideoMode(0, 0, bitsPerPixel, 0);
    #else
        SDL_WM_SetCaption(VERSION_STRING, "Frodo");

        if (enableScaling)
    		physicalScreen = SDL_SetVideoMode(initialWidth,
                                              initialHeight,
                                              bitsPerPixel,
                                              SDL_DOUBLEBUF|SDL_RESIZABLE);
        else
            physicalScreen = SDL_SetVideoMode(DISPLAY_X,
                                              DISPLAY_Y,
                                              bitsPerPixel,
                                              SDL_DOUBLEBUF);
	#endif

    if (NULL == physicalScreen) 
    {
        fprintf(stderr, "Couldn't initialize physical SDL screen (%s)\n", SDL_GetError());
        return 0;
    }

    bufferWidth         = DISPLAY_X;
    bufferHeight        = DISPLAY_Y;
    bufferBitsPerPixel  = bitsPerPixel;
    bufferPitch         = DISPLAY_X * bitsPerPixel / 8;
    bufferSize          = bufferPitch * bufferHeight;

    front_buffer        = new uint8[bufferSize];
    back_buffer         = new uint8[bufferSize];
    mid_buffer          = new uint8[bufferSize];

    if (NULL == front_buffer || NULL == back_buffer || NULL == mid_buffer) 
    {
        fprintf(stderr, "Couldn't initialize buffers\n");
        return false;
    }

    bufferLock = SDL_CreateMutex();
    if (NULL == bufferLock)
    {
        fprintf(stderr, "Couldn't initialize buffer lock (%s)\n", SDL_GetError());
        return false;
    }

    memset(front_buffer, 0, bufferSize);
    memset(back_buffer,  0, bufferSize);
    memset(mid_buffer,   0, bufferSize);

    return true;
}


/*
 *  Prefs may have changed
 */

void C64Display::NewPrefs(Prefs *prefs)
{
}

/*
 *  Redraw bitmap
 */

void C64Display::Update()
{
    //if (invalidated) printf("STILL INVALID!\n");
    if (run_async_emulation)
    {
        if (swapBuffers)
        { // make mid buffer to back buffer for input
            if (-1 != SDL_LockMutex(bufferLock))
            {
                uint8* swap_ptr = back_buffer;
                back_buffer = mid_buffer;
                mid_buffer = swap_ptr;

                SDL_UnlockMutex(bufferLock);
            }
        }

        invalidated = true;
    }
    else
    {
        doRedraw();
    }
}

void C64Display::redraw()
{
    if (!invalidated)
    {
        return;
    }

    if (swapBuffers)
    { // make mid buffer to front buffer for display
        if (-1 != SDL_LockMutex(bufferLock))
        {
            uint8* swap_ptr = front_buffer;
            front_buffer = mid_buffer;
            mid_buffer = swap_ptr;

            SDL_UnlockMutex(bufferLock);
        }
    }

    invalidated = false;

    doRedraw();
    
}

void C64Display::doRedraw()
{
    frameCounter++;

    if (backgroundInvalid)
    {
        backgroundInvalid = false;
        SDL_FillRect(physicalScreen, NULL, 0x0);
    }

    SDL_Surface* out = physicalScreen;
    uint8* source_buffer = (run_async_emulation && swapBuffers) ? front_buffer : back_buffer;

    if (NULL != osd && osd->isShown())
    {
        osd->draw(out, black, fill_gray, shadow_gray, shine_gray);
    	SDL_Flip(out);
        return;
    }

	Uint32 currentTicks = SDL_GetTicks();
	if (currentTicks - lastLEDUpdate > 400 || lastLEDUpdate == 0) {
		lastLEDUpdate = currentTicks;
		update_led_blinking();
	}

	int screenWidth = out->w;
	int screenHeight = out->h;

	int barH = 15;
	int barW = screenWidth;
	int barY = out->h-barH;

	zoom_factor = out->w / 340;

	if (zoom_factor < 1) zoom_factor = 1;
	else if (zoom_factor > 4) zoom_factor = 4;

    if (TheC64->TheInput->isVirtualKeyboardEnabled() && zoom_factor > 2)
    {
        zoom_factor = 2;
    }

    int ofsX = (physicalScreen->w - bufferWidth*zoom_factor) / 2;
    int sourceArea = bufferHeight *zoom_factor;
    int clientArea = physicalScreen->h;

	#ifdef WEBOS   
	    int ofsY = TheC64->TheInput->isVirtualKeyboardEnabled() ? -55 : (clientArea - sourceArea) / 2;
	#else
        int ofsY = (clientArea - sourceArea) / 2;
	#endif

    {
		Uint8* srcBuffer  = (Uint8*) source_buffer;
		Uint8* destBuffer = ((Uint8*) out->pixels);

		int yStart = 0;
		if (ofsY < 0)
		{
			yStart += (-ofsY/zoom_factor);
			ofsY = 0;
		}

		int xStart = 0;
		if (ofsX < 0)
		{
			xStart += -(ofsX/zoom_factor);
			ofsX = 0;
		}

		Uint8* srcLine    = srcBuffer  + bufferPitch * yStart + xStart;
		Uint8* destLine   = destBuffer + out->pitch * ofsY + ofsX;
		Uint8* destLine2  = destLine   + out->pitch;
		Uint8* destLine3  = destLine2  + out->pitch;
		Uint8* destLine4  = destLine3  + out->pitch;

		int y2    = ofsY;
		int yEnd  = bufferHeight - (enabledStatusBar ? barH : 0);
		int yEnd2 = out->h - (enabledStatusBar ? barH : 0);

		for (int y=yStart; y<yEnd; y++)
		{
			if (y2+zoom_factor-1 >= yEnd2) break;
		
			Uint8* src    = srcLine;
			Uint8* dest   = destLine;
			Uint8* dest2  = destLine2;
			Uint8* dest3  = destLine3;
			Uint8* dest4  = destLine4;
			Uint8 c;

			int x2 = ofsX;

			for (int x=0; x<bufferWidth; x++)
			{
				if (x2+zoom_factor-1 >= out->w) break;

				c = *(src++);

				if (1 == zoom_factor)
				{
					*(dest++) = c;
				}
				else if (2 == zoom_factor)
				{
					*(dest++)  = c;
					*(dest++)  = c;
					*(dest2++) = c;
					*(dest2++) = c;
				}
				else if (3 == zoom_factor)
				{
					*(dest++)  = c;
					*(dest++)  = c;
					*(dest++)  = c;
					*(dest2++) = c;
					*(dest2++) = c;
					*(dest2++) = c;
					*(dest3++) = c;
					*(dest3++) = c;
					*(dest3++) = c;
				}
				else if (4 == zoom_factor)
				{
					*(dest++)  = c;
					*(dest++)  = c;
					*(dest++)  = c;
					*(dest++)  = c;
					*(dest2++) = c;
					*(dest2++) = c;
					*(dest2++) = c;
					*(dest2++) = c;
					*(dest3++) = c;
					*(dest3++) = c;
					*(dest3++) = c;
					*(dest3++) = c;
					*(dest4++) = c;
					*(dest4++) = c;
					*(dest4++) = c;
					*(dest4++) = c;
				}

				x2 += zoom_factor;
			}

			srcLine   += bufferPitch;
			destLine  += out->pitch * zoom_factor;
			destLine2 += out->pitch * zoom_factor;
			destLine3 += out->pitch * zoom_factor;
			destLine4 += out->pitch * zoom_factor;

			y2 += zoom_factor;
		}
    }

    if (enabledStatusBar)
    {
	    // Draw speedometer/LEDs
	    SDL_Rect r = {0, barY, screenWidth, barH};

	    SDL_FillRect(out, &r, fill_gray);
	    r.h = 1;
	    SDL_FillRect(out, &r, shine_gray);
	    r.y = barY + barH-1;
	    SDL_FillRect(out, &r, shadow_gray);

	    r.w = 16;
	    for (int i=2; i<6; i++)
        {
		    r.x = barW * i/5 - 24; r.y = barY + 4;
		    SDL_FillRect(out, &r, shadow_gray);
		    r.y = barY + 10;
		    SDL_FillRect(out, &r, shine_gray);
	    }
	    r.y = barY; r.w = 1; r.h = barH;
	    for (int i=0; i<5; i++)
        {
		    r.x = barW * i / 5;
		    SDL_FillRect(out, &r, shine_gray);
		    r.x = barW * (i+1) / 5 - 1;
		    SDL_FillRect(out, &r, shadow_gray);
	    }
	    r.y = barY + 4; r.h = 7;
	    for (int i=2; i<6; i++)
        {
		    r.x = barW * i/5 - 24;
		    SDL_FillRect(out, &r, shadow_gray);
		    r.x = barW * i/5 - 9;
		    SDL_FillRect(out, &r, shine_gray);
	    }
	    r.y = barY + 5; r.w = 14; r.h = 5;
	    for (int i=0; i<4; i++)
        {
		    r.x = barW * (i+2) / 5 - 23;
		    int c;
		    switch (led_state[i]) {
			    case LED_ON:
				    c = green;
				    break;
			    case LED_ERROR_ON:
				    c = red;
				    break;
			    default:
				    c = black;
				    break;
		    }
		    SDL_FillRect(out, &r, c);
	    }

	    draw_string(out, barW * 1/5 + 8, barY + 4, "D\x12 8", black, fill_gray);
	    draw_string(out, barW * 2/5 + 8, barY + 4, "D\x12 9", black, fill_gray);
	    draw_string(out, barW * 3/5 + 8, barY + 4, "D\x12 10", black, fill_gray);
	    draw_string(out, barW * 4/5 + 8, barY + 4, "D\x12 11", black, fill_gray);
	    draw_string(out, 24, barY + 4, speedometer_string, black, fill_gray);
    }

    TheC64->TheJoystick->draw(out,
                              toolbarHeight, out->h - toolbarHeight*2, 
                              black, fill_gray, shine_gray);

    if (enableTitle)
    {
        int titleWidth = 340; if (titleWidth > out->w) titleWidth = out->w;
        int titleHeight = 160;

        int titleX = (out->w - titleWidth) / 2;
        int titleY = (out->h - titleHeight) / 2;

        draw_window(out, titleX, titleY, titleWidth, titleHeight,
                    "FRODO for webOS",
                    white, fill_gray, shadow_gray);

        int x = titleX + 5;
        int y = titleY + 25;

        uint8 fg = black;
        uint8 bg = fill_gray;

        draw_string(out, x, y, "(C) by Christian Bauer", fg, bg); y += 10;
        draw_string(out, x, y, "webOS Version by Roland Schabenberger", fg, bg); y += 10;

        y += 20;
        draw_string(out, x, y, "CONTROL AREAS:", fg, bg); y += 10;

        draw_string(out, x, y, "top-left: RUN/STOP", fg, bg); y += 10;
        draw_string(out, x, y, "top-middle: CONTROL PANEL", fg, bg); y += 10;
        draw_string(out, x, y, "top-right: AUTORUN", fg, bg); y += 10;
        draw_string(out, x, y, "bottom-left: TOGGLE LED", fg, bg); y += 10;
        draw_string(out, x, y, "bottom-middle: SPACE", fg, bg); y += 10;
        draw_string(out, x, y, "bottom-right: VIRTUAL KEYS", fg, bg); y += 10;
        draw_string(out, x, y, "middle-left: STICK INPUT", fg, bg); y += 10;
        draw_string(out, x, y, "middle-right: BUTTON INPUT", fg, bg); y += 10;
    }

	// Update display
	SDL_Flip(physicalScreen);
}

void C64Display::showAbout(bool show)
{
    enableTitle = show;
}

void C64Display::draw_window(SDL_Surface *s, int x, int y, int w, int h, const char *str, uint8 text_color, uint8 front_color, uint8 back_color)
{
    if (x < 0) x = (s->w - w) / 2;
    if (y < 0) y = (s->h - h) / 2;

    SDL_Rect win;

    win.x = x;
    win.y = y;
    win.w = w;
    win.h = 20;

    SDL_FillRect(s, &win, back_color);

    win.y += win.h;
    win.h = h - 20;

    SDL_FillRect(s, &win, front_color);

    win.y = y;

    x += 4;
    y = win.y + 6;

    draw_string(s, x, y, str, text_color, back_color);
}

/*
 *  Draw string into surface using the C64 ROM font
 */

void C64Display::draw_string(SDL_Surface *s, int x, int y, const char *str, uint8 front_color, uint8 back_color)
{
    if (x<0 || y<0) return;
    if (y+7 >= s->h) return;

	uint8 *pb = (uint8 *)s->pixels + s->pitch*y + x;
	char c;
	while ((c = *str++) != 0) 
    {
        if (x+7 >= s->w) break;

        if (c>='a' && c<='z') c -= 'a'-'A';
        if (c=='\\') c = '/';

        if ((c>='A' && c<= 'Z') ||
            (c>='0' && c<= '9') ||
            c == '-' || c == '.' || c == ':' || c == ' ' || c == '/' ||
            c == '(' || c == ')' || c == '%')
        {

		    uint8 *q = TheC64->Char + c*8 + 0x800;
		    uint8 *p = pb;
		    for (int y=0; y<8; y++) 
            {
			    uint8 v = *q++;
			    p[0] = (v & 0x80) ? front_color : back_color;
			    p[1] = (v & 0x40) ? front_color : back_color;
			    p[2] = (v & 0x20) ? front_color : back_color;
			    p[3] = (v & 0x10) ? front_color : back_color;
			    p[4] = (v & 0x08) ? front_color : back_color;
			    p[5] = (v & 0x04) ? front_color : back_color;
			    p[6] = (v & 0x02) ? front_color : back_color;
			    p[7] = (v & 0x01) ? front_color : back_color;
			    p += s->pitch;
		    }
		    pb += 8;

            x += 8;
        }
	}
}


/*
 *  Update drive LED display (deferred until Update())
 */

void C64Display::UpdateLEDs(int l0, int l1, int l2, int l3)
{
	led_state[0] = l0;
	led_state[1] = l1;
	led_state[2] = l2;
	led_state[3] = l3;
}

/*
 *  LED error blink
 */
void C64Display::update_led_blinking()
{
	for (int i=0; i<4; i++)
    {
		switch (c64_disp->led_state[i]) 
        {
			case LED_ERROR_ON:
				c64_disp->led_state[i] = LED_ERROR_OFF;
				break;
			case LED_ERROR_OFF:
				c64_disp->led_state[i] = LED_ERROR_ON;
				break;
            default:
                break;
		}
    }
}

/*
 *  Draw speedometer
 */

void C64Display::Speedometer(int speed)
{
    // assumes speedometer is updated every second
    framesPerSecond = frameCounter;
    frameCounter = 0;

	sprintf(speedometer_string, "%d%% %dfps", speed, framesPerSecond);
}


/*
 *  Return pointer to bitmap data
 */

uint8* C64Display::BitmapBase(void)
{
    uint8* ptr = NULL;

    SDL_LockMutex(bufferLock);
	ptr = (uint8*) back_buffer;
    SDL_UnlockMutex(bufferLock);

    return ptr;
}


/*
 *  Return number of bytes per row
 */

int C64Display::BitmapXMod(void)
{
	return bufferPitch;
}

void C64Display::invalidateBackground()
{
    backgroundInvalid = true;
}

int C64Display::getWidth()
{
    return physicalScreen->w;
}

int C64Display::getHeight()
{
    return physicalScreen->h;
}

/*
 *  Allocate C64 colors
 */

void C64Display::InitColors(uint8 *colors)
{
	for (int i=0; i<16; i++) 
    {
		palette[i].r = palette_red[i];
		palette[i].g = palette_green[i];
		palette[i].b = palette_blue[i];
	}

	palette[fill_gray].r    = palette[fill_gray].g      = palette[fill_gray].b   = 0xd0;
	palette[shine_gray].r   = palette[shine_gray].g     = palette[shine_gray].b  = 0xf0;
	palette[shadow_gray].r  = palette[shadow_gray].g    = palette[shadow_gray].b = 0x80;
	palette[red].r          = 0xf0;
	palette[red].g          = palette[red].b            = 0;
	palette[green].g        = 0xf0;
	palette[green].r        = palette[green].b          = 0;
    
    if (physicalScreen->format->BitsPerPixel == 8)
    {
	    SDL_SetColors(physicalScreen, palette, 0, PALETTE_SIZE);
    }

	for (int i=0; i<256; i++)
    {
		colors[i] = i & 0x0f;
    }
}

void C64Display::resize(int w, int h)
{
	SDL_Surface* newPhysicalSurface = SDL_SetVideoMode(w, h, bitsPerPixel, SDL_DOUBLEBUF|SDL_RESIZABLE);

	if (NULL != newPhysicalSurface) 
    {
		physicalScreen = newPhysicalSurface;
		SDL_SetColors(physicalScreen, palette, 0, PALETTE_SIZE);
	}

    invalidateBackground();
}

void C64Display::openOsd()
{
    osd->show(true);
    invalidateBackground();
}

void C64Display::closeOsd()
{
    osd->show(false);
    invalidateBackground();
}

bool C64Display::isOsdActive()
{
    return osd->isShown();
}

OSD* C64Display::getOsd()
{
    return osd;
}

void C64Display::toggleStatusBar()
{
    enabledStatusBar = !enabledStatusBar;
    invalidateBackground();
}

void C64Display::openAbout()
{
    if (!enableTitle)
    {
        enableTitle = true;
        invalidateBackground();
    }
}

void C64Display::closeAbout()
{
    if (enableTitle)
    {
        enableTitle = false;
        invalidateBackground();
    }
}
