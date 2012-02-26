/*
 *  main.cpp - Main program
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */
#include "sysdeps.h"
#include "Version.h"

#include "main.h"
#include "C64.h"
#include "Display.h"
#include "Prefs.h"
#include "SAM.h"
#include "Input.h"
#include "virtual_joystick.h"

bool run_async_emulation = true;

// Global variables
char AppDirPath[1024];	// Path of application directory
Frodo *TheApp = NULL;   // The application.

Frodo::Frodo()
{
    running = false;
	TheC64 = NULL;
}

Frodo::~Frodo()
{
    shutdown();
}

/*
 *  Initialize
 */

bool Frodo::initialize(int argc, char **argv)
{
    running = false;

	if (argc == 2)
    {
		strncpy(prefs_path, argv[1], 255);
    }

	getcwd(AppDirPath, 256);

	// Load preferences
	if (!prefs_path[0]) {
		char *home = getenv("HOME");
		if (home != NULL && strlen(home) < 240) {
			strncpy(prefs_path, home, 200);
			strcat(prefs_path, "/");
		}
		strcat(prefs_path, ".frodorc");
	}

	ThePrefs.Load(prefs_path);

	// Create and start C64
	TheC64 = new C64;
    if (false == TheC64->init())
    {
        return false;
    }

    return true;
}

static int threadEntry(void* context)
{
    ((Frodo*) context)->emulationLoop();
    return 0;
}

void Frodo::run()
{
    running = true;

    SDL_Thread* emulation_thread = NULL;
    
    if (run_async_emulation)
    {
        emulation_thread = SDL_CreateThread(threadEntry, this);
    }

    Uint32 start = 0;
    Uint32 elapsed = 0;
    Uint32 maxFramerate = 50 / ThePrefs.SkipFrames;
    Uint32 cycleTime = 1000 / maxFramerate;

    SDL_Event event;

    while (running)
    {
        while (running)
        {
            while (SDL_PollEvent(&event)) 
            {
                handleEvent(&event);
                if (false == running )
                {
                    break;
                }
            }

            if (TheC64->isCancelled())
            {
                running = false;
                break;
            }

            elapsed = SDL_GetTicks() - start;

            if (start == 0 || elapsed >= cycleTime)
            {
                break;
            }

            uint32 waitTime = cycleTime - elapsed;
            SDL_Delay(waitTime < 4 ? waitTime : 4);
        }

        start = SDL_GetTicks();

        if (NULL != emulation_thread)
        {
            TheC64->TheDisplay->redraw();
        }
        else
        {
            doStep();
        }
    }

    if (NULL != emulation_thread)
    {
        SDL_WaitThread(emulation_thread, NULL);
    }

    running= false;
}

void Frodo::emulationLoop()
{
    while (running && !TheC64->isCancelled())
    {
        doStep();
    }
}

void Frodo::doStep()
{
    TheC64->doStep();
}

void Frodo::shutdown()
{
    running = false;

    if (NULL != TheC64)
    {
	    delete TheC64;
        TheC64 = NULL;
    }
}

void Frodo::handleEvent(SDL_Event* event)
{
    InputHandler* inputHandler = (InputHandler*) TheC64->TheInput;

    if (TheC64->TheDisplay->isOsdActive())
    {
        inputHandler = (InputHandler*) TheC64->TheDisplay->getOsd();
    }

    switch (event->type)
    {
        case SDL_MOUSEBUTTONDOWN:
        {
            SDL_MouseButtonEvent* e = &event->button;
            inputHandler->handleMouseEvent(e->x, e->y, true);
            break;
        }
        case SDL_MOUSEBUTTONUP:
        {
            SDL_MouseButtonEvent* e = &event->button;
            inputHandler->handleMouseEvent(e->x, e->y, false);
            break;
        }
        case SDL_KEYDOWN:
        {
            SDLKey key = event->key.keysym.sym;
            SDLMod mod = event->key.keysym.mod;

            if (TheC64->TheDisplay->isOsdActive())
            {
                inputHandler->handleKeyEvent(key, mod, true);
            }
            else
            {
                if (TheC64->TheJoystick->getMode() == VirtualJoystick::MODE_KEYBOARD &&
                   (key == SDLK_LEFT ||
                    key == SDLK_RIGHT ||
                    key == SDLK_UP ||
                    key == SDLK_DOWN ||
                    key == SDLK_LCTRL))
                {
                    TheC64->TheJoystick->keyInput(key, true);
                }
                else
                {
                    switch (key)
                    {
                        case SDLK_F9:
                        {
                            if ((mod & KMOD_LSHIFT) != 0)
                            {
					            TheC64->TheJoystick->setMode((TheC64->TheJoystick->getMode() + 1) % 3);
                            }
                            else
                            {
                                if (TheC64->TheDisplay->isOsdActive())
                                {
                                    TheC64->TheDisplay->closeOsd();
                                }
                                else
                                {
                                    TheC64->TheDisplay->openOsd();
                                }
                            }

                            break;
                        }
			            case SDLK_F10:	// F10: QUIT
                        {
                            TheC64->Quit();
				            break;
                        }
			            case SDLK_F11:	// F11: NMI (Restore)
                        {
				            TheC64->NMI();
				            break;
                        }
			            case SDLK_F12:	// F12: Reset
                        {
				            TheC64->Reset();
				            break;
                        }
                        default:
                        {
                            inputHandler->handleKeyEvent(key, mod, true);
                            break;
                        }
                    }
                }
            }

            break;
        }
        case SDL_KEYUP:
        {
            SDLKey key = event->key.keysym.sym;
            SDLMod mod = event->key.keysym.mod;

            if (TheC64->TheJoystick->getMode() == VirtualJoystick::MODE_KEYBOARD &&
                (key == SDLK_LEFT ||
                key == SDLK_RIGHT ||
                key == SDLK_UP ||
                key == SDLK_DOWN ||
                key == SDLK_LCTRL))
            {
                TheC64->TheJoystick->keyInput(key, false);
            }
            else
            {
                inputHandler->handleKeyEvent(key, mod, false);
            }

            break;
        }
        case SDL_VIDEORESIZE:
        {
			SDL_ResizeEvent* r = &event->resize;
            TheC64->TheDisplay->resize(r->w, r->h);
            break;
        }
        case SDL_QUIT:
        {
            running = false;
            break;
        }
        default:
        {
            break;
        }
    }
}


/*
 *  Create application object and start it
 */

int main(int argc, char **argv)
{
	// Init SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK ) < 0) {
		fprintf(stderr, "Couldn't initialize SDL (%s)\n", SDL_GetError());
		return 0;
	}

    #ifdef WEBOS
        PDL_Init(0);
        atexit(PDL_Quit);
    #endif

    srand( (unsigned)time( NULL ) );

	printf("%s by Christian Bauer\n", VERSION_STRING);

    #ifdef FRODO_SC
        printf("Mode: single-cycle emulation\n");
    #else
        printf("Mode: line-based emulation\n");
    #endif

	fflush(stdout);

	TheApp = new Frodo();
	if (TheApp->initialize(argc, argv))
    {
    	TheApp->run();
    }

    TheApp->shutdown();
	delete TheApp;
    TheApp = NULL;

	SDL_Quit();

	return 0;
}
