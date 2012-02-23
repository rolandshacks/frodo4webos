/*
 *  Display_SDL.i - C64 graphics display, emulator window handling,
 *                  SDL specific stuff
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#include "C64.h"
#include "SAM.h"
#include "Version.h"
#include "osd.h"
#include "virtual_joystick.h"

//#define USEGL 1

int initialWidth = 1024;
int initialHeight = 768;
//int initialWidth = 350;
//int initialHeight = 250;
int bitsPerPixel = 8;
int zoom_factor = 1;

// JOYSTICK EMULATION: 0=no emulation, 1=keyboard, 2=mouse
#ifdef WEBOS
  int emulateJoystick = 2;
  bool enableTitle = true;
#else
  int emulateJoystick = 0;
  bool enableTitle = false;
#endif

bool enableScaling = true;
bool enabledStatusBar = false;
bool enableVirtualKeyboard = false;
bool enabledOSD = true;

bool invalidateBackground = true;

int mousePosX = 0;
int mousePosY = 0;
bool mousePressed = false;
int mouseState = 0;
int lastMouseState = 0;
int mouseZone = 25;

int toolbarHeight = 40;
int toolbarWidgetWidth = 50;

// Display surface
static SDL_Surface *physicalScreen = NULL;
static SDL_Surface *screen = NULL;

// Keyboard
static bool num_locked = false;

// For LED error blinking
static C64Display *c64_disp;
static Uint32 lastLEDUpdate = 0;


int joyButtonWidth = 40;
int joyButtonHeight = 40;

SDL_Rect joyButtonLeft;

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
  6     /   ^   =  SHR HOM  ;   *   �
  7    R/S  Q   C= SPC  2  CTL  <-  1
*/

#define MATRIX(a,b) (((a) << 3) | (b))

/*
 *  Open window
 */

int init_graphics(void)
{
    
	// Open window
    
    #ifdef WEBOS
	    physicalScreen = SDL_SetVideoMode(0, 0, bitsPerPixel, 0);
    #else
        SDL_WM_SetCaption(VERSION_STRING, "Frodo");

        if (enableScaling)
    		physicalScreen = SDL_SetVideoMode(initialWidth, initialHeight, bitsPerPixel, SDL_DOUBLEBUF|SDL_RESIZABLE);
        else
            physicalScreen = SDL_SetVideoMode(DISPLAY_X, DISPLAY_Y, bitsPerPixel, SDL_DOUBLEBUF);
	        //physicalScreen = SDL_SetVideoMode(DISPLAY_X, DISPLAY_Y + 17, bitsPerPixel, SDL_DOUBLEBUF);
	#endif

    if (NULL == physicalScreen) {
        fprintf(stderr, "Couldn't initialize physical SDL screen (%s)\n", SDL_GetError());
        return 0;
    }

    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
        Uint32  rmask = 0xff000000;
        Uint32  gmask = 0x00ff0000;
        Uint32  bmask = 0x0000ff00;
        Uint32  amask = 0x000000ff;
    #else
        Uint32  rmask = 0x000000ff;
        Uint32  gmask = 0x0000ff00;
        Uint32  bmask = 0x00ff0000;
        Uint32  amask = 0xff000000;
    #endif

    if (enableScaling)
    {
        SDL_Surface* surface = SDL_CreateRGBSurface(SDL_SWSURFACE, DISPLAY_X, DISPLAY_Y, bitsPerPixel, rmask, gmask, bmask, amask);
        screen = SDL_DisplayFormat(surface);
        SDL_FreeSurface(surface);

        if (NULL == screen) {
            fprintf(stderr, "Couldn't initialize SDL screen (%s)\n", SDL_GetError());
            return 0;
        }
    }
    else
    {
        screen = physicalScreen;
    }

    if (enableVirtualKeyboard)
    {
        #ifdef WEBOS
            PDL_SetKeyboardState(PDL_TRUE);
        #endif
    }

    ThePrefs.SkipFrames = 3;
    
	return 1;
}


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

    virtual_joystick = new VirtualJoystick();
    virtual_joystick->create(the_c64, this);


}

/*
 *  Display destructor
 */

C64Display::~C64Display()
{

    if (NULL != screen && screen != physicalScreen) {
        SDL_FreeSurface(screen);
        screen = NULL;
    }

    if (NULL != osd)
    {
        delete osd;
        osd = NULL;
    }

    if (NULL != virtual_joystick)
    {
        delete virtual_joystick;
        virtual_joystick = NULL;
    }
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

void C64Display::Update(void)
{
    if (invalidateBackground)
    {
        invalidateBackground = false;
        SDL_FillRect(physicalScreen, NULL, 0x0);
    }

    if (NULL != osd && osd->isShown())
    {
        osd->draw(physicalScreen, black, fill_gray, shine_gray);
    	SDL_Flip(physicalScreen);
        return;
    }

	Uint32 currentTicks = SDL_GetTicks();
	if (currentTicks - lastLEDUpdate > 400 || lastLEDUpdate == 0) {
		lastLEDUpdate = currentTicks;
		update_led_blinking();
	}

	SDL_Surface* out = physicalScreen;

	int screenWidth = out->w;
	int screenHeight = out->h;

	int barH = 15;
	int barW = screenWidth;
	int barY = out->h-barH;

	zoom_factor = out->w / 340;

	if (zoom_factor < 1) zoom_factor = 1;
	else if (zoom_factor > 4) zoom_factor = 4;

    #ifdef WEBOS
        if (enableVirtualKeyboard && zoom_factor > 2)
        {
            zoom_factor = 2;
        }
    #endif

    if (physicalScreen != screen) {

        int ofsX = (physicalScreen->w - screen->w*zoom_factor) / 2;
        int sourceArea = screen->h *zoom_factor;
        int clientArea = physicalScreen->h;

		#ifdef WEBOS   
	    	int ofsY = enableVirtualKeyboard ? -55 : (clientArea - sourceArea) / 2;
		#else
            int ofsY = (clientArea - sourceArea) / 2;
		#endif

        if (1 == zoom_factor)
        {
            SDL_Rect srcRect;
            srcRect.x = 0;
            srcRect.y = 0;
            srcRect.w = screen->w;
            srcRect.h = screen->h;

            SDL_Rect destRect;
            destRect.x = ofsX;
            destRect.y = ofsY;
            destRect.w = 0;
            destRect.h = 0;

            SDL_BlitSurface(screen, NULL, physicalScreen, &destRect);
        }
        else
        {
		    Uint8* srcBuffer  = (Uint8*) screen->pixels;
		    Uint8* destBuffer = ((Uint8*) physicalScreen->pixels);

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

		    Uint8* srcLine   = srcBuffer + screen->pitch * yStart + xStart;
		    Uint8* destLine  = destBuffer + physicalScreen->pitch * ofsY + ofsX;
		    Uint8* destLine2  = destLine + physicalScreen->pitch;
		    Uint8* destLine3  = destLine2 + physicalScreen->pitch;
		    Uint8* destLine4  = destLine3 + physicalScreen->pitch;

		    int y2 = ofsY;
		    int yEnd = screen->h - (enabledStatusBar ? barH : 0);
		    int yEnd2 = physicalScreen->h - (enabledStatusBar ? barH : 0);

		    for (int y=yStart; y<yEnd; y++)
		    {
			    if (y2+zoom_factor-1 >= yEnd2) break;
		
			    Uint8* src   = srcLine;
			    Uint8* dest  = destLine;
			    Uint8* dest2  = destLine2;
			    Uint8* dest3  = destLine3;
			    Uint8* dest4  = destLine4;
			    Uint8 c;

			    int x2 = ofsX;

			    for (int x=0; x<screen->w; x++)
			    {
				    if (x2+zoom_factor-1 >= physicalScreen->w) break;

				    c = *(src++);

				    if (1 == zoom_factor)
				    {
					    *(dest++) = c;
				    }
				    else if (2 == zoom_factor)
				    {
					    *(dest++) = c;
					    *(dest++) = c;
					    *(dest2++) = c;
					    *(dest2++) = c;
				    }
				    else if (3 == zoom_factor)
				    {
					    *(dest++) = c;
					    *(dest++) = c;
					    *(dest++) = c;
					    *(dest2++) = c;
					    *(dest2++) = c;
					    *(dest2++) = c;
					    *(dest3++) = c;
					    *(dest3++) = c;
					    *(dest3++) = c;
				    }
				    else if (4 == zoom_factor)
				    {
					    *(dest++) = c;
					    *(dest++) = c;
					    *(dest++) = c;
					    *(dest++) = c;
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

			    srcLine   += screen->pitch;
			    destLine  += physicalScreen->pitch * zoom_factor;
			    destLine2 += physicalScreen->pitch * zoom_factor;
			    destLine3 += physicalScreen->pitch * zoom_factor;
			    destLine4 += physicalScreen->pitch * zoom_factor;

			    y2 += zoom_factor;
		    }
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
	    for (int i=2; i<6; i++) {
		    r.x = barW * i/5 - 24; r.y = barY + 4;
		    SDL_FillRect(out, &r, shadow_gray);
		    r.y = barY + 10;
		    SDL_FillRect(out, &r, shine_gray);
	    }
	    r.y = barY; r.w = 1; r.h = barH;
	    for (int i=0; i<5; i++) {
		    r.x = barW * i / 5;
		    SDL_FillRect(out, &r, shine_gray);
		    r.x = barW * (i+1) / 5 - 1;
		    SDL_FillRect(out, &r, shadow_gray);
	    }
	    r.y = barY + 4; r.h = 7;
	    for (int i=2; i<6; i++) {
		    r.x = barW * i/5 - 24;
		    SDL_FillRect(out, &r, shadow_gray);
		    r.x = barW * i/5 - 9;
		    SDL_FillRect(out, &r, shine_gray);
	    }
	    r.y = barY + 5; r.w = 14; r.h = 5;
	    for (int i=0; i<4; i++) {
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

    if (NULL != virtual_joystick && 
        virtual_joystick->isShown() &&
        2 == emulateJoystick)
    {
        virtual_joystick->draw(physicalScreen, black, fill_gray, shine_gray);
    }

    if (enableTitle)
    {
        int titleWidth = 340; if (titleWidth > physicalScreen->w) titleWidth = physicalScreen->w;
        int titleHeight = 160;

        int titleX = (physicalScreen->w - titleWidth) / 2;
        int titleY = (physicalScreen->h - titleHeight) / 2;

        draw_window(physicalScreen, titleX, titleY, titleWidth, titleHeight,
                    "FRODO for webOS",
                    white, fill_gray, shadow_gray);

        int x = titleX + 5;
        int y = titleY + 25;

        uint8 fg = black;
        uint8 bg = fill_gray;

        draw_string(physicalScreen, x, y, "(C) by Christian Bauer", fg, bg); y += 10;
        draw_string(physicalScreen, x, y, "webOS Version by Roland Schabenberger", fg, bg); y += 10;

        y += 20;
        draw_string(physicalScreen, x, y, "CONTROL AREAS:", fg, bg); y += 10;

        draw_string(physicalScreen, x, y, "top-left: RUN/STOP", fg, bg); y += 10;
        draw_string(physicalScreen, x, y, "top-middle: CONTROL PANEL", fg, bg); y += 10;
        draw_string(physicalScreen, x, y, "top-right: AUTORUN", fg, bg); y += 10;
        draw_string(physicalScreen, x, y, "bottom-left: TOGGLE LED", fg, bg); y += 10;
        draw_string(physicalScreen, x, y, "bottom-middle: SPACE", fg, bg); y += 10;
        draw_string(physicalScreen, x, y, "bottom-right: VIRTUAL KEYS", fg, bg); y += 10;
        draw_string(physicalScreen, x, y, "middle-left: STICK INPUT", fg, bg); y += 10;
        draw_string(physicalScreen, x, y, "middle-right: BUTTON INPUT", fg, bg); y += 10;
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

void pushKey(int key)
{
    // printf(", %d", key); if (key == -13) printf("\n");

    if (key_queue_elements < key_queue_size)
    {
        key_queue_elements++;
        key_queue[key_queue_inptr++] = key;
        if (key_queue_inptr >= key_queue_size) key_queue_inptr = 0;
    }
}

void pushKeyPress(int key)
{
    pushKey(key);
    pushKey(-key);
}


void pushKeySequence(int* keys, int count)
{
    for (int i=0; i<count; i++)
    {
        pushKey(keys[i]);
    }
}

int popKey()
{
    if (key_queue_elements < 1) return 0; 

    key_queue_elements--;

    int sym = key_queue[key_queue_outptr++];
    if (key_queue_outptr >= key_queue_size) key_queue_outptr = 0;

    return sym;
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
	while ((c = *str++) != 0) {

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
		    for (int y=0; y<8; y++) {
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
 *  LED error blink
 */
void C64Display::update_led_blinking()
{
	for (int i=0; i<4; i++)
		switch (c64_disp->led_state[i]) {
			case LED_ERROR_ON:
				c64_disp->led_state[i] = LED_ERROR_OFF;
				break;
			case LED_ERROR_OFF:
				c64_disp->led_state[i] = LED_ERROR_ON;
				break;
		}
}

/*
 *  Draw speedometer
 */

void C64Display::Speedometer(int speed)
{
	static int delay = 0;

	if (delay >= 20) {
		delay = 0;
		sprintf(speedometer_string, "%d%%", speed);
	} else
		delay++;
}


/*
 *  Return pointer to bitmap data
 */

uint8 *C64Display::BitmapBase(void)
{
	return (uint8 *)screen->pixels;
}


/*
 *  Return number of bytes per row
 */

int C64Display::BitmapXMod(void)
{
	return screen->pitch;
}


/*
 *  Poll the keyboard
 */

static void translate_key(SDLKey key, bool key_up, uint8 *key_matrix, uint8 *rev_matrix, uint8 *joystick)
{
	int c64_key = -1;
	switch (key) {
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
		case SDLK_EQUALS: c64_key = MATRIX(5,3); break;
		case SDLK_LEFTBRACKET: c64_key = MATRIX(5,6); break;
		case SDLK_RIGHTBRACKET: c64_key = MATRIX(6,1); break;
		case SDLK_SEMICOLON: c64_key = MATRIX(5,5); break;
		case SDLK_QUOTE: c64_key = MATRIX(6,2); break;
		case SDLK_SLASH: c64_key = MATRIX(6,7); break;

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
		return;

    #ifndef WEBOS
	    if (1 == emulateJoystick) { // KEYBOARD
		    if (c64_key == 0x3a) {

			    if (key_up) {
				    *joystick |= ~0xef; // Button
			    }
			    else {
				    *joystick &= 0xef; // Button
			    }

		    }
		    else if (c64_key == 0x82) {

			    if (key_up) {
				    *joystick |= ~0xfb; // Left
			    }
			    else {
				    *joystick &= 0xfb; // Left
			    }
		    }
		    else if (c64_key == 0x2) {

			    if (key_up) {
				    *joystick |= ~0xf7; // Right
			    }
			    else {
				    *joystick &= 0xf7; // Right
			    }
		    }
		    else if (c64_key == 0x87) {

			    if (key_up) {
				    *joystick |= ~0xfe; // Up
			    }
			    else {
				    *joystick &= 0xfe; // Up
			    }
		    }
		    else if (c64_key == 0x7) {

			    if (key_up) {
				    *joystick |= ~0xfd; // Down
			    }
			    else {
				    *joystick &= 0xfd; // Down
			    }

		    }

		    if (*joystick != 0xff) return;
	    }
    #endif // WEBOS

    //printf("key: %d\n", c64_key);

	// Handle joystick emulation
	if (c64_key & 0x40) {
		c64_key &= 0x1f;

		if (key_up)
			*joystick |= c64_key;
		else
			*joystick &= ~c64_key;
		return;
	}

	// Handle other keys
	bool shifted = c64_key & 0x80;
	int c64_byte = (c64_key >> 3) & 7;
	int c64_bit = c64_key & 7;
	if (key_up) {
		if (shifted) {
			key_matrix[6] |= 0x10;
			rev_matrix[4] |= 0x40;
		}
		key_matrix[c64_byte] |= (1 << c64_bit);
		rev_matrix[c64_bit] |= (1 << c64_byte);
	} else {
		if (shifted) {
			key_matrix[6] &= 0xef;
			rev_matrix[4] &= 0xbf;
		}
		key_matrix[c64_byte] &= ~(1 << c64_bit);
		rev_matrix[c64_bit] &= ~(1 << c64_byte);
	}

}

static void swap(int& a, int& b)
{
    int c = a;
    a = b;
    b = c;
}

void C64Display::updateMouse(uint8 *joystick)
{
	if (2 != emulateJoystick) {
		return;
	}

	*joystick = 0xff;

    int leftArea = physicalScreen->w/2;

	int mouseX = 0;
	int mouseY = 0;
	int mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);

    int mouseX2 = 0;
    int mouseY2 = 0;

    #ifdef WEBOS
        int mouseButtons2 = SDL_GetMultiMouseState(1, &mouseX2, &mouseY2);
    #else
        int mouseButtons2 = 0;
    #endif

    //if (0 == mouseButtons && 0 == mouseButtons2) return;

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
    lastMouseState = mouseState;
    mouseState = virtual_joystick->updateState(mouseButtons, mouseX, mouseY);

    if (enableVirtualKeyboard) // CURSOR CONTROL
    {
        if (0 == lastMouseState)
        {
		    if (mouseState & INPUT_LEFT) {
			    //printf("LEFT\n");
                pushKeyPress(276);
		    }
		    else if (mouseState & INPUT_RIGHT) {
			    //printf("RIGHT\n");
                pushKeyPress(275);
		    }
		    else if (mouseState & INPUT_UP) {
			    //printf("UP\n");
                pushKeyPress(273);
		    }
		    else if (mouseState & INPUT_DOWN) {
			    //printf("DOWN\n");
                pushKeyPress(274);
		    }
        }

    } else 	{ // JOYSTICK EMULATION

		if (mouseState & INPUT_LEFT) {
			//printf("LEFT\n");
			*joystick &= 0xfb; // Left
		}
		if (mouseState & INPUT_RIGHT) {
			//printf("RIGHT\n");
			*joystick &= 0xf7; // Right
		}
		if (mouseState & INPUT_UP) {
			//printf("UP\n");
			*joystick &= 0xfe; // Up
		}
		if (mouseState & INPUT_DOWN) {
			//printf("DOWN\n");
			*joystick &= 0xfd; // Down
		}

        if (mouseButtons2 & 0x1) {
		    *joystick &= 0xef; // Button
	    }
    }


    //if (0xff != *joystick) printf("JOYSTICK: %x\n", *joystick);
}

void printKeyInfo(const char* info, SDL_KeyboardEvent* key)
{
    /*
    Uint8 *keystate = SDL_GetKeyState(NULL);

    if ( keystate[SDLK_LSHIFT] ) printf("Left shift pressed.\n");

    printf("  %d MOD: %d\n", (int) (key->keysym.sym), (int) (key->keysym.mod));
    printf("  Scancode: 0x%02X\n", key->keysym.scancode );
    printf("  State: 0x%02X\n", key->state );
    printf("  Unicode: %s\n", key->keysym.unicode );
    printf("  Name: %s\n", SDL_GetKeyName( key->keysym.sym ) );
    */
}

void toggleVirtualKeyboard()
{
    enableVirtualKeyboard = enableVirtualKeyboard ? false : true;
    invalidateBackground = true;

	#ifdef WEBOS
        PDL_SetKeyboardState(enableVirtualKeyboard ? PDL_TRUE : PDL_FALSE);
	#endif
}

void C64Display::PollKeyboard(uint8 *key_matrix, uint8 *rev_matrix, uint8 *joystick)
{
    bool osdAction = false;

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_VIDEORESIZE:
			{
				SDL_ResizeEvent* r = &event.resize;

				SDL_Surface* newPhysicalSurface = SDL_SetVideoMode(r->w, r->h, 8, SDL_DOUBLEBUF|SDL_RESIZABLE);
				if (NULL != newPhysicalSurface) {
					physicalScreen = newPhysicalSurface;
					SDL_SetColors(physicalScreen, palette, 0, PALETTE_SIZE);
				}

                invalidateBackground = true;

				break;
			}
			case SDL_MOUSEBUTTONDOWN:
			{
				break;
			}

			case SDL_MOUSEBUTTONUP:
			{
                SDL_MouseButtonEvent* e = &event.button;

                if (enableTitle) enableTitle = false;

                if (osd->isShown())
                {
                    osdAction = osd->onClick(e->x, e->y);

                    if (!osd->isShown())
                    {
                        invalidateBackground = true;
                    }
                }
                else if (e->y < toolbarHeight)
                {
                    if (e->x < toolbarWidgetWidth)
                    {
                        pushKeyPress(27); // ESC = RUN/STOP
                    }
                    else if (e->x > physicalScreen->w - toolbarWidgetWidth)
                    {
                        TheC64->Reset(); pushKey(0);pushKey(0);pushKey(0);pushKey(0);pushKey(0);
                        pushKeySequence(SEQUENCE_LOADFIRST, sizeof(SEQUENCE_LOADFIRST)/sizeof(SEQUENCE_LOADFIRST[0]));
                        pushKeySequence(SEQUENCE_RUN, sizeof(SEQUENCE_RUN)/sizeof(SEQUENCE_RUN[0]));
                        if (enableVirtualKeyboard) toggleVirtualKeyboard();
                    }
                    else
                    {
                        osd->show(true);
                    }
                }
                else if (e->y > physicalScreen->h - toolbarHeight)
                {
                    if (e->x < toolbarWidgetWidth)
                    {
                        enabledStatusBar = !enabledStatusBar;
                    }
                    else if (e->x > physicalScreen->w - toolbarWidgetWidth)
                    {
                        toggleVirtualKeyboard();
                    }
                    else
                    {
                        pushKeyPress(32); // SPACE
                    }
                }

				break;
			}

			case SDL_MOUSEMOTION:
			{
				break;
			}

			// Key pressed
			case SDL_KEYDOWN:
                // printf("SDL-KEY: %d (mod: %d)\n", (int) event.key.keysym.sym, (int) event.key.keysym.mod);

				switch (event.key.keysym.sym) {

					case SDLK_F9:	// F9: Invoke SAM
						//SAM(TheC64);
                        if ((event.key.keysym.mod & KMOD_LSHIFT) != 0)
                        {
						    emulateJoystick = (emulateJoystick + 1) % 3;
                        }
                        else
                        {
					        osd->show(!osd->isShown());
                            invalidateBackground = true;
                        }
						break;

					case SDLK_F10:	// F10: QUIT
						quit_requested = true;
						break;

					case SDLK_F11:	// F11: NMI (Restore)
						TheC64->NMI();
						break;

					case SDLK_F12:	// F12: Reset
						TheC64->Reset();
						break;

					case SDLK_NUMLOCK:
						num_locked = true;
						break;

					case SDLK_KP_PLUS:	// '+' on keypad: Increase SkipFrames
						ThePrefs.SkipFrames++;
						break;

					case SDLK_KP_MINUS:	// '-' on keypad: Decrease SkipFrames
						if (ThePrefs.SkipFrames > 1)
							ThePrefs.SkipFrames--;
						break;

					case SDLK_KP_MULTIPLY:	// '*' on keypad: Toggle speed limiter
						ThePrefs.LimitSpeed = !ThePrefs.LimitSpeed;
						break;

					default:
						if (!osd->isShown()) {

                            printKeyInfo("KEYDOWN", &event.key);
                            pushKey((int) (event.key.keysym.sym));
                            //printf("KEYDOWN: %d %d\n", (int) (event.key.keysym.sym), (int) (event.key.keysym.mod));
							//translate_key(event.key.keysym.sym, false, key_matrix, rev_matrix, joystick);
						}
						break;
				}
				break;

			// Key released
			case SDL_KEYUP:
				if (event.key.keysym.sym == SDLK_NUMLOCK)
				{
					num_locked = false;
				}
                #ifdef WEBOS
                    else if (enableVirtualKeyboard && event.key.keysym.sym == PDLK_GESTURE_DISMISS_KEYBOARD)
                    {
                        toggleVirtualKeyboard();
                    }
                #endif
				else
				{
					if (!osd->isShown()) {

                        printKeyInfo("KEYUP", &event.key);
                        pushKey(- ((int) event.key.keysym.sym));

                        // translate_key(event.key.keysym.sym, true, key_matrix, rev_matrix, joystick);
                    }
                    else if (event.key.keysym.sym == SDLK_ESCAPE)
                    {
                        osd->show(false);
                        invalidateBackground = true;
                    }
				}
				break;

			// Quit Frodo
			case SDL_QUIT:
				quit_requested = true;
				break;
		}
	}

    if (!osdAction)
    {
        int keyInput = popKey();
        if (0 != keyInput)
        {
            if (keyInput > 0)
            {
                translate_key((SDLKey) keyInput, false, key_matrix, rev_matrix, joystick);
            }
            else
            {
                translate_key((SDLKey) -keyInput, true, key_matrix, rev_matrix, joystick);
            }
        }

	    if (2 == emulateJoystick) {
		    updateMouse(joystick);
		    if (*joystick != 0xff) return;
	    }
    }
    else
    {
        *joystick = 0xff;
    }

	if (quit_requested)
	{
		TheC64->Quit();
	}
}


/*
 *  Check if NumLock is down (for switching the joystick keyboard emulation)
 */

bool C64Display::NumLock(void)
{
	return num_locked;
}


/*
 *  Allocate C64 colors
 */

void C64Display::InitColors(uint8 *colors)
{
	for (int i=0; i<16; i++) {
		palette[i].r = palette_red[i];
		palette[i].g = palette_green[i];
		palette[i].b = palette_blue[i];
	}
	palette[fill_gray].r = palette[fill_gray].g = palette[fill_gray].b = 0xd0;
	palette[shine_gray].r = palette[shine_gray].g = palette[shine_gray].b = 0xf0;
	palette[shadow_gray].r = palette[shadow_gray].g = palette[shadow_gray].b = 0x80;
	palette[red].r = 0xf0;
	palette[red].g = palette[red].b = 0;
	palette[green].g = 0xf0;
	palette[green].r = palette[green].b = 0;
    
	SDL_SetColors(physicalScreen, palette, 0, PALETTE_SIZE);
	if (screen != physicalScreen) {
		SDL_SetColors(screen, palette, 0, PALETTE_SIZE);
	}

	for (int i=0; i<256; i++)
		colors[i] = i & 0x0f;
}


/*
 *  Show a requester (error message)
 */

long int ShowRequester(char *a,char *b,char *)
{
	printf("%s: %s\n", a, b);
	return 1;
}
