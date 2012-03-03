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
#include "renderer.h"
#include "texture.h"
#include "font.h"
#include "resources.h"

extern bool run_async_emulation;
extern bool limitFramerate;

static bool swapBuffers = true;

int initialWidth = 1024;
int initialHeight = 768;
//int initialWidth = 350;
//int initialHeight = 250;
int bitsPerPixel = 8;
int zoom_factor = 1;

#define MMIN(a, b) (((a) <= (b)) ? (a) : (b))
#define MMAX(a, b) (((a) >= (b)) ? (a) : (b))

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
uint32 rgba_palette[PALETTE_SIZE];

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

    // opengl renderer
    renderer = NULL;
    lastDrawTime = 0;

    memset(&res, 0, sizeof(res));
    res.initialized = false;

	// LEDs off
	for (int i=0; i<4; i++)
    {
		led_state[i] = old_led_state[i] = LED_OFF;
    }

	// Start timer for LED error blinking
	c64_disp = this;

    osd = NULL;

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
    if (NULL != renderer)
    {
        doFreeGL();
        renderer->shutdown();
        delete renderer;
        renderer = NULL;
    }

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

        #if USE_OPENGL
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	        physicalScreen = SDL_SetVideoMode(0, 0, 0, SDL_OPENGL);
        #else
	        physicalScreen = SDL_SetVideoMode(0, 0, bitsPerPixel, 0);
        #endif

    #else
        SDL_WM_SetCaption(VERSION_STRING, "Frodo");

        #if USE_OPENGL
    	    physicalScreen = SDL_SetVideoMode(initialWidth,
                                              initialHeight,
                                              32,
                                              SDL_HWSURFACE|SDL_GL_DOUBLEBUFFER|SDL_OPENGL);
        #else
    	    physicalScreen = SDL_SetVideoMode(initialWidth,
                                              initialHeight,
                                              bitsPerPixel,
                                              SDL_HWSURFACE|SDL_DOUBLEBUF|SDL_RESIZABLE);
        #endif

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

    viewX = 0.0f; viewY = 0.0f; viewW = -1.0f; viewH = -1.0f;

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

    statusText = "";
    statusTextTimeout = 0.0f;

    InitColors(NULL);

    #if USE_OPENGL
        renderer = new Renderer();
        renderer->init(physicalScreen->w, physicalScreen->h);
        doInitGL();
    #endif

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
}

void C64Display::redraw()
{
    if (!invalidated && limitFramerate)
    {
        return;
    }

    if (invalidated && swapBuffers)
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

    #if USE_OPENGL
        doRedrawGL();
    #else
        doRedraw();
    #endif
    
}

void C64Display::doInitGL()
{
    res.screenTexture           = new Texture(bufferWidth, bufferHeight, 32);
    res.backgroundTexture       = new Texture(RES_BACKGROUND);
    res.buttonTexture           = new Texture(RES_BUTTON);
    res.buttonPressedTexture    = new Texture(RES_BUTTON_PRESSED);
    res.tapZonesTexture         = new Texture(RES_TAP_ZONES);
    res.stickTexture            = new Texture(RES_STICK);

    res.iconDiskActivity        = new Texture(RES_ICON_DISK_ACTIVITY);
    res.iconDiskError           = new Texture(RES_ICON_DISK_ERROR);
    res.iconDisk                = new Texture(RES_ICON_DISK);
    res.iconFolder              = new Texture(RES_ICON_FOLDER);
    res.iconClose               = new Texture(RES_ICON_CLOSE);

    res.fontTiny                = new Font(RES_FONT, 12.0f);
    res.fontSmall               = new Font(RES_FONT, 14.0f);
    res.fontNormal              = new Font(RES_FONT, 18.0f);
    res.fontLarge               = new Font(RES_FONT, 28.0f);

    res.initialized = true;

    if (enabledOSD)
    {
        osd = new OSD();
        osd->create(renderer, TheC64, this);

        //osd->show(true);
    }

    renderer->setFont(res.fontNormal);

    setStatusMessage("Welcome!");
}

void C64Display::doFreeGL()
{
    if (res.initialized)
    {
        res.initialized = false;

        delete res.screenTexture;
        delete res.backgroundTexture;
        delete res.buttonTexture;
        delete res.buttonPressedTexture;
        delete res.tapZonesTexture;
        delete res.stickTexture;

        delete res.iconDiskActivity;
        delete res.iconDiskError;
        delete res.iconDisk;
        delete res.iconFolder;
        delete res.iconClose;

        delete res.fontTiny;
        delete res.fontSmall;
        delete res.fontNormal;
        delete res.fontLarge;
    }
}

void C64Display::doRedrawGL()
{
    frameCounter++;

    renderer->beginDraw();

    uint32 currentTicks = SDL_GetTicks();
    uint32 elapsedTicks = (lastDrawTime > 0) ? currentTicks-lastDrawTime : 0;
    lastDrawTime = currentTicks;
    elapsedTime = (float) elapsedTicks / 1000.0f;

    if (!enableTitle)
    {
        drawScreen();

        if (!osd->isShown())
        {
            drawVirtualJoystick();
        }
    }

    if (NULL != osd && osd->isShown())
    {
        osd->draw(elapsedTime, &res);
        drawStatusBar(); // TO BE REMOVED
    }
    else
    {
        drawStatusBar();
    }

    if (enableTitle)
    {
        drawAbout();
    }

    renderer->endDraw();
}

void C64Display::drawScreen()
{
    int screenWidth = getWidth();
    int screenHeight = getHeight();

	int clientWidth = screenWidth;
	int clientHeight = screenHeight;

    if (NULL != osd && osd->isShown())
    {
        clientWidth -= osd->getWidth();
    }
    #ifdef WEBOS
        if (TheC64->TheInput->isVirtualKeyboardEnabled())
        {
            clientHeight -= 330;
        }
    #endif

	zoom_factor = MMIN(clientWidth / (DISPLAY_X-44), clientHeight / (DISPLAY_Y-80));
	if (zoom_factor < 1)
    {
        zoom_factor = 1;
    }
	else if (zoom_factor > 4)
    {
        zoom_factor = 4;
    }

    /*
    if (TheC64->TheInput->isVirtualKeyboardEnabled() && zoom_factor > 2)
    {
        zoom_factor = 2;
    }
    */

    int outW = bufferWidth   * zoom_factor;
    int outH = bufferHeight  * zoom_factor;
    int outX = (clientWidth  - outW) / 2;
    int outY = (clientHeight - outH) / 2;

    if (viewH < 0.0f || viewW < 0.0f)
    {
        viewX = (float) outX;
        viewY = (float) outY;
        viewW = (float) outW;
        viewH = (float) outH;
    }
    else
    {
        viewX += (outX-viewX)*elapsedTime*10;
        viewY += (outY-viewY)*elapsedTime*10;
        viewW += (outW-viewW)*elapsedTime*10;
        viewH += (outH-viewH)*elapsedTime*10;
    }

    res.screenTexture->bind();

    if (res.screenTexture->getBitsPerPixel() != bufferBitsPerPixel)
    {
        res.screenTexture->updateData(front_buffer, bufferBitsPerPixel, rgba_palette);
    }
    else
    {
        res.screenTexture->updateData(front_buffer);
    }

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    //renderer->fillRectangle(outX, outY, outW, outH);
    renderer->fillRectangle(viewX, viewY, viewW, viewH);

    res.screenTexture->unbind();
}

void C64Display::drawVirtualJoystick()
{
    TheC64->TheJoystick->draw(renderer, &res);
}

void C64Display::setStatusMessage(const std::string& message, float timeOut)
{
    statusText = message;

    if (timeOut <= 0.0f) timeOut = 3.0f;
    statusTextTimeout = timeOut;
}

void C64Display::drawStatusBar()
{
    int width = getWidth();
    int height = getHeight();

    renderer->setFont(res.fontTiny);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    int textPos = 8;

    { // disk state
        int disk_state = LED_OFF;
	    for (int i=0; i<4; i++)
        {
            if (led_state[i] != LED_OFF)
            {
                disk_state = led_state[i];
                break;
            }
	    }

        if (disk_state == LED_ON)
        {
            renderer->drawTexture(res.iconDiskActivity, width-34, height-32);
        }
        else if (disk_state == LED_ERROR_ON)
        {
            renderer->drawTexture(res.iconDiskError, width-34, height-32);
        }
    }

    if (enabledStatusBar)
    {
        renderer->drawText(textPos, height-6,
                           speedometer_string,
                           Renderer::ALIGN_BOTTOM);

        textPos += 100;
    }

    if (statusTextTimeout > 0.0f)
    {
        statusTextTimeout -= elapsedTime;
        renderer->setFont(res.fontNormal);
        renderer->drawText(textPos, height-6, statusText.c_str(), Renderer::ALIGN_BOTTOM);
    }
}

void C64Display::drawAbout()
{
    int width = res.tapZonesTexture->getWidth() + 40;
    int height = res.tapZonesTexture->getHeight() + 220;

    int clientWidth = getWidth();
    if (NULL != osd && osd->isShown())
    {
        clientWidth -= osd->getWidth();
    }

    int ofs = (clientWidth - width) / 2;

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    renderer->drawTiledTexture(res.backgroundTexture, ofs, (getHeight()-height)/2, width, height);

    float y = (getHeight()-height)/2 + 40.0f;
    float x = ofs + 40;

    renderer->setFont(res.fontLarge);
    renderer->drawText(x, y, "C64 for WebOS"); y += renderer->getFont()->getHeight();
    renderer->setFont(res.fontNormal);
    renderer->drawText(x, y, "Ported by Roland Schabenberger"); y += renderer->getFont()->getHeight();
    renderer->drawText(x, y, "Original Frodo by Christian Bauer"); y += renderer->getFont()->getHeight();

    y += 30.0f;
    renderer->setFont(res.fontLarge);
    renderer->drawText(x, y, "Tap Corners for Control"); y += renderer->getFont()->getHeight();
    y += 10.0f;

    renderer->drawTexture(res.tapZonesTexture, ofs+20, y);

    //renderer->setFont(res.fontTiny);
    //renderer->drawText(ofs+20, (getHeight()-height)/2 + height - 5 - res.fontTiny->getHeight(), "v1.0.4", Renderer::ALIGN_BOTTOM);
}

void C64Display::showAbout(bool show)
{
    enableTitle = show;
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

    // printf("SPEED: %s\n", speedometer_string);
}

uint8* C64Display::BitmapBase(void)
{
    uint8* ptr = NULL;

    SDL_LockMutex(bufferLock);
	ptr = (uint8*) back_buffer;
    SDL_UnlockMutex(bufferLock);

    return ptr;
}

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

    for (int i=0; i<PALETTE_SIZE; i++)
    {
        // precompute openGl rgba palette
        rgba_palette[i] = 0xff000000|((uint32)palette[i].b<<16) | ((uint32)palette[i].g << 8) | ((uint32)palette[i].r);
    }
    
    #if !USE_OPENGL
        if (physicalScreen->format->BitsPerPixel == 8)
        {
	        SDL_SetColors(physicalScreen, palette, 0, PALETTE_SIZE);
        }
    #endif

    if (NULL != colors)
    {
	    for (int i=0; i<256; i++)
        {
		    colors[i] = i & 0x0f;
        }
    }
}

void C64Display::resize(int w, int h)
{
    #if USE_OPENGL
	    SDL_Surface* newPhysicalSurface = SDL_SetVideoMode(w, h, 32, SDL_HWSURFACE|SDL_GL_DOUBLEBUFFER|SDL_OPENGL);
    #else
	    SDL_Surface* newPhysicalSurface = SDL_SetVideoMode(w, h, bitsPerPixel, SDL_HWSURFACE|SDL_DOUBLEBUF|SDL_RESIZABLE);
    #endif

	if (NULL != newPhysicalSurface) 
    {
		physicalScreen = newPhysicalSurface;

        #if !USE_OPENGL
		    SDL_SetColors(physicalScreen, palette, 0, PALETTE_SIZE);
        #endif
	}

    if (NULL != renderer)
    {
        renderer->resize(w, h);
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

bool C64Display::isAboutActive() const
{
    return enableTitle;
}