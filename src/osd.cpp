/*
 *  osd.cpp - On Screen Display
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#include "sysdeps.h"

#include "main.h"
#include "C64.h"
#include "Display.h"
#include "main.h"
#include "Prefs.h"
#include "ndir.h"
#include "osd.h"
#include "Input.h"
#include "renderer.h"
#include "texture.h"
#include "font.h"

#include <algorithm>

#ifdef WIN32
#define NATIVE_SLASH '\\'
#else
#define NATIVE_SLASH '/'
#endif

using namespace std;

static float movement = 0.0f;
static int controlDir = 0;
static const int maxFilenameLen = 36;

OSD::OSD()
{
    init();
}

OSD::~OSD()
{
    cleanup();
}

void OSD::init()
{
    visible = false;

    pos = 0.0f;

    windowRect.x = windowRect.y = windowRect.w = windowRect.h = 0;
    fileListFrame.x = fileListFrame.y = fileListFrame.w = fileListFrame.h = 0;
    toolbarRect.x = toolbarRect.y = toolbarRect.w = toolbarRect.h = 0;

    commandList.push_back( command_t ( CMD_ENABLE_JOYSTICK1, "JOY1", "Enable Joystick 1" ) );
    commandList.push_back( command_t ( CMD_ENABLE_JOYSTICK2, "JOY2", "Enable Joystick 2" ) );
    commandList.push_back( command_t ( CMD_ENABLE_TRUEDRIVE, "DRV", "Toggle 1541 Drive Emulation" ) );
    commandList.push_back( command_t ( CMD_WARP, "WARP", "Toggle Warp Mode" ) );
    commandList.push_back( command_t ( CMD_RESET, "RESET", "Reset" ) );

    commandList.push_back( command_t ( CMD_KEY_F1, "F1", "F1 Key" ) );
    commandList.push_back( command_t ( CMD_KEY_F2, "F2", "F2 Key" ) );
    commandList.push_back( command_t ( CMD_KEY_F3, "F3", "F3 Key" ) );
    commandList.push_back( command_t ( CMD_KEY_F4, "F4", "F4 Key" ) );
    commandList.push_back( command_t ( CMD_KEY_F5, "F5", "F5 Key" ) );
    commandList.push_back( command_t ( CMD_KEY_F6, "F6", "F6 Key" ) );
    commandList.push_back( command_t ( CMD_KEY_F7, "F7", "F7 Key" ) );
    commandList.push_back( command_t ( CMD_KEY_F8, "F8", "F8 Key" ) );

    commandList.push_back( command_t ( CMD_SHOW_ABOUT, "HELP", "Show Help" ) );

    scrollElementTop = scrollPixelRange = 0;
    scrollPixelOffset = 0.0f;

    mousePressed = mouseGesture = false;
    mouseX = mousePressPosX = 0;
    mouseY = mousePressPosY = 0;

    memset(&layout, 0, sizeof(layout));
    layout.valid = false;
}

void OSD::create(Renderer* renderer, C64* the_c64, C64Display* display)
{
    this->renderer = renderer;
    this->the_c64 = the_c64;
    this->display = display;

    buttonWidth = 60;

    char nameBuffer[512];

    #if   defined( WEBOS )
        strcpy(nameBuffer, "/media/internal/c64");
    #elif defined( WIN32 )
        ::GetCurrentDirectory(sizeof(nameBuffer), nameBuffer);
    #else
        getcwd(nameBuffer, sizeof(nameBuffer));
    #endif

    currentDirectory = nameBuffer;
}

void OSD::cleanup()
{
    fileList.clear();
    currentDirectory.clear();
}

int OSD::getWidth() const
{
    int w = renderer->getWidth() - (int) pos;
    if (w < 0) w = 0;

    return w;
}

void OSD::show(bool doShow)
{
    if (doShow != visible)
    {
        visible = doShow;

        if (visible)
        {
            movement = 0.0f;
            controlDir = 0;

            pos = renderer->getWidth();
            update();
        }
    }
}

bool OSD::isShown()
{
    return visible;
}

void OSD::updateLayout(int w, int h, float elapsedTime, resource_list_t* res)
{
    int borderX=5;
    int borderY=5;

    layout.rowHeight    = res->buttonTexture->getHeight();
    layout.width        = renderer->getWidth()-(DISPLAY_X-44)*2; //350;
    layout.height       = renderer->getHeight();
    layout.titleHeight  = 30;

    if (pos > renderer->getWidth()-layout.width)
    {
        pos -= 1000.0f * elapsedTime;
        if (pos < renderer->getWidth()-layout.width)
        {
            pos = renderer->getWidth() - layout.width;
        }
    }

    windowRect.x = (int) pos;
    windowRect.y = borderY;
    windowRect.w = layout.width;
    windowRect.h = h-borderY*2;

    fileListFrame.x = windowRect.x + 16;
    fileListFrame.y = windowRect.y + layout.titleHeight;
    fileListFrame.w = windowRect.w - buttonWidth - 21;
    fileListFrame.h = windowRect.h - layout.titleHeight - 30;

    toolbarRect.x = fileListFrame.x + fileListFrame.w + 5;
    toolbarRect.y = windowRect.y + layout.titleHeight + 1;
    toolbarRect.w = buttonWidth;
    toolbarRect.h = windowRect.h - layout.titleHeight - 1;

    layout.valid = true;
}

void OSD::draw(float elapsedTime, resource_list_t* res)
{
    if (!visible) return;

    updateLayout(renderer->getWidth(), renderer->getHeight(), elapsedTime, res);

    SDL_Rect titleRect = { windowRect.x+10,
                           windowRect.y+4,
                           windowRect.w,
                           layout.titleHeight };


    glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

    renderer->fillRectangle(windowRect.x, windowRect.y, windowRect.w, windowRect.h);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    renderer->drawTiledTexture(res->backgroundTexture,
                               windowRect.x, windowRect.y,
                               windowRect.w+res->backgroundTexture->getWidth()/2.0f, windowRect.h);

    renderer->setFont(res->fontSmall);
    renderer->drawText(titleRect.x + 10, titleRect.y + titleRect.h/2,
                       currentDirectory.c_str(),
                       Renderer::ALIGN_MIDDLE);

    renderer->enableClipping(fileListFrame.x, fileListFrame.y,
                             fileListFrame.w, fileListFrame.h);

    drawFiles(res);

    renderer->disableClipping();

    renderer->drawTexture(res->iconClose,
                          windowRect.x + windowRect.w-res->iconClose->getWidth()+2,
                          windowRect.y - res->iconClose->getHeight()/2 + 11);

    drawToolbar(res);
}

void OSD::drawFiles(resource_list_t* res)
{
    if (controlDir != 0)
    {
        movement += controlDir;
        if (movement > 25.0f) movement = 25.0f;
        else if (movement < -25.0f) movement = -25.0f;
    }
    else 
    {
        movement *= 0.9f;
    }

    // --------------------------------------------------------------

    renderer->setFont(res->fontSmall);

    // printf("DIR:%s\n", currentDirectory.c_str());

    char buf[256];

    SDL_Rect entryRect;

    int itemHeight = layout.rowHeight;

    scrollPixelRange = fileList.size() * layout.rowHeight - fileListFrame.h;

    int rowFontOffsetY = (itemHeight - renderer->getFont()->getHeight()) / 2;

    scrollPixelOffset += movement;

    if (scrollPixelOffset >= scrollPixelRange)
    {
        scrollPixelOffset = (float) (scrollPixelRange - 1);
        movement = 0.0f;
    }
    else if (scrollPixelOffset < 0.0f)
    {
        scrollPixelOffset = 0.0f;
        movement = 0.0f;
    }

    int y = (int) -scrollPixelOffset;

    for (vector<fileinfo_t>::const_iterator it = fileList.begin();
         it != fileList.end();
         ++it)
    {
        if (y >= -itemHeight && y < fileListFrame.h + itemHeight)
        {
            const fileinfo_t& fileInfo = *it;
            const string& fileName = fileInfo.name;

            strncpy(buf, fileName.c_str(), maxFilenameLen);
            *(buf + maxFilenameLen) = '\0';

		    //printf("D64-File: %s\n", filename);

            entryRect.x = fileListFrame.x+1;
            entryRect.y = y+fileListFrame.y+1;
            entryRect.w = fileListFrame.w-2;
            entryRect.h = itemHeight-2;

            glColor4f(1.0f, 1.0f, 1.0f, 0.1f);

            renderer->drawTiledTexture(res->buttonTexture,
                                       fileListFrame.x,
                                       fileListFrame.y+y,
                                       fileListFrame.w,
                                       itemHeight);

            glColor4f(1.0f, 1.0f, 1.0f, 0.8f);

            if (!fileInfo.isDirectory)
            {
                renderer->drawTexture(res->iconDisk,
                                      fileListFrame.x+4,
                                      fileListFrame.y+y+(itemHeight-res->iconDisk->getHeight())/2);
            }
            else
            {
                renderer->drawTexture(res->iconFolder,
                                      fileListFrame.x+4,
                                      fileListFrame.y+y+(itemHeight-res->iconFolder->getHeight())/2);
            }

            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

            renderer->drawText(fileListFrame.x+28,
                               fileListFrame.y+y+itemHeight/2,
                               buf, Renderer::ALIGN_MIDDLE);
        }

		y += itemHeight;

        if (y >= fileListFrame.h)
        {
            break;
        }
	}
}

void OSD::drawToolbar(resource_list_t* res)
{
    renderer->setFont(res->fontTiny);

    SDL_Rect buttonRect = toolbarRect;

    buttonHeight = res->buttonTexture->getHeight();
    buttonRect.h = buttonHeight;

    int buttonFontHeight = renderer->getFont()->getHeight();

    for (vector<command_t>::iterator it = commandList.begin();
         it != commandList.end();
         ++it)
    {
        command_t& cmd = *it;

        updateCommandState(cmd);

        if (buttonRect.y + buttonRect.h >= toolbarRect.y + toolbarRect.h)
        {
            break;
        }

        glColor4f(1.0f, 1.0f, 1.0f, 0.5f);

        renderer->drawTiledTexture((cmd.state == STATE_NORMAL) ? res->buttonTexture : res->buttonPressedTexture,
                                   buttonRect.x, buttonRect.y,
                                   buttonRect.w, buttonRect.h);

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        renderer->drawText(buttonRect.x + buttonRect.w/2 ,
                           buttonRect.y + buttonRect.h/2,
                           cmd.text.c_str(), Renderer::ALIGN_CENTER|Renderer::ALIGN_MIDDLE);

        buttonRect.y += buttonRect.h + 1;
    }
}

void OSD::update()
{

    fileList.clear();

    fileinfo_t parentInfo;
    parentInfo.name = "Parent";
    parentInfo.isDirectory = true;
    fileList.push_back(parentInfo);

	DIR* dir = opendir (currentDirectory.c_str());
	if (NULL != dir) 
    {

		for (;;)
		{
			struct dirent* dir_entry = readdir(dir);
			if (NULL == dir_entry) break;

			string filename = dir_entry->d_name;

            const char* filenameStr = filename.c_str();

            if (*filenameStr != '.') // filter everything that starts with '.'
            {
                int extPos = filename.rfind('.');
                if (extPos > 0)
                {
                    string extension = filename.substr(extPos+1);
                    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                    if (extension == "d64" || extension == "t64" /* || extension == "prg" */)
                    {
                        fileinfo_t fileInfo;
                        fileInfo.name = filename;
                        fileInfo.isDirectory = false;
                        fileList.push_back(fileInfo);
                    }
			    }
                else if (dir_entry->d_type == DT_DIR)
                {
                    fileinfo_t dirInfo;
                    dirInfo.name = filename;
                    dirInfo.isDirectory = true;
                    fileList.push_back(dirInfo);
                }
            }
		}

		closedir(dir);
		dir = NULL;
	}

    /*
    for (int i=0; i<100; i++)
    {
        char buf[256];
        sprintf(buf, "TestFile-%d.d64", i);

        fileinfo_t fileInfo;
        fileInfo.name = buf;
        fileInfo.isDirectory = false;
        fileList.push_back(fileInfo);
    }
    */

}

OSD::fileinfo_t OSD::getCachedFileInfo(int idx)
{
    if (idx >= 0 || idx < fileList.size())
    {
        vector<fileinfo_t>::const_iterator it = fileList.begin() + idx;
        if (it != fileList.end()) return *it;
    }

    fileinfo_t noInfo;
    noInfo.name = "";
    noInfo.isDirectory = false;
    return noInfo;
}

void OSD::handleMouseEvent(int x, int y, int eventType)
{
    int mouseDeltaX = x - mouseX;
    int mouseDeltaY = y - mouseY;

    mouseX = x;
    mouseY = y;

    if (InputHandler::EVENT_Down == eventType) // mouse down
    {
        mousePressed = true;
        mouseGesture = false;
        mousePressPosX = mouseX;
        mousePressPosY = mouseY;
        movement = 0.0f;
    }
    else if (InputHandler::EVENT_Up == eventType)
    {
        if (!mouseGesture)
        {
            onClick(x, y);
        }

        mousePressed = false;
        mouseGesture = false;
        //movement = 0.0f;
    }
    else if (InputHandler::EVENT_Move == eventType)
    {
        if (mousePressed)
        {
            movement = - (float) mouseDeltaY * 4.0f;
            if (!mouseGesture && (abs(mousePressPosX - mouseX) > 5 || abs(mousePressPosY - mouseY) > 5))
            {
                mouseGesture = true;
            }

        }
    }
}

void OSD::handleKeyEvent(int key, int sym, int eventType)
{
    controlDir = 0;

    bool press = (InputHandler::EVENT_Down == eventType);

    if (key == SDLK_ESCAPE)
    {
        visible = false;
    }
    else if (key == SDLK_UP)
    {
        if (press) controlDir = -1;
    }
    else if (key == SDLK_DOWN)
    {
        if (press) controlDir = 1;
    }
}

bool OSD::onClick(int x, int y)
{
    if (!visible || !layout.valid) return false;

    if (x >= fileListFrame.x && x < fileListFrame.x+fileListFrame.w &&
        y >= fileListFrame.y && y < fileListFrame.y+fileListFrame.h) 
    {

        int oscY = y - fileListFrame.y;
        int idx = (oscY + (int) scrollPixelOffset) / layout.rowHeight;

        fileinfo_t fileInfo = getCachedFileInfo(idx);
        if (!fileInfo.name.empty())
        {
            if (idx > 0 && fileInfo.isDirectory)
            {
                if (currentDirectory.at(currentDirectory.size()-1) != NATIVE_SLASH)
                {
                    currentDirectory.append(1, NATIVE_SLASH);
                }
                currentDirectory.append(fileInfo.name);
                update();

                return true;
            }
            else if (idx == 0)
            {
                int pos = currentDirectory.rfind(NATIVE_SLASH);
                if (pos > 1)
                {
                    currentDirectory.erase(pos);

                    #ifdef WIN32
                    if (currentDirectory.size() == 2)
                    {
                        currentDirectory.append(1, NATIVE_SLASH);
                    }
                    #endif
                    update();
                    return true;
                }
            }
            else
            {
                setNewFile(fileInfo);
            }
        }

        visible = false;
    }
    else if (x >= toolbarRect.x && x < toolbarRect.x+toolbarRect.w &&
             y >= toolbarRect.y && y < toolbarRect.y+toolbarRect.h) 
    {
        int ofsY = y - toolbarRect.y;
        int idx = ofsY / (buttonHeight+1);
        if (idx >= 0 && idx < commandList.size()) 
        {
            vector<command_t>::const_iterator it = commandList.begin() + idx;
            if (it != commandList.end())
            {
                onCommand(*it);
                //visible = false;
            }
        }
    }
    else
    {
        visible = false;
    }

    return true;
}

void OSD::onCommand(const OSD::command_t& command)
{
    // printf("COMMAND: %s\n", command.text.c_str());

    Prefs *prefs = new Prefs(ThePrefs);

    bool prefsChanged = false;
    bool commandDispatched = true;

    int id = command.id;
    switch (id)
    {
        case CMD_ENABLE_JOYSTICK1:
            prefs->JoystickSwap = true;
            prefsChanged = true;
            break;
        case CMD_ENABLE_JOYSTICK2:
            prefs->JoystickSwap = false;
            prefsChanged = true;
            break;
        case CMD_ENABLE_TRUEDRIVE:
            prefs->Emul1541Proc = !prefs->Emul1541Proc;
            prefsChanged = true;
            break;
        case CMD_WARP:
            prefs->LimitSpeed = !prefs->LimitSpeed;
            prefsChanged = true;
            break;
        case CMD_RESET:
            the_c64->Reset();
            break;
        case CMD_KEY_F1:
            pushKeyPress(282);
            break;
        case CMD_KEY_F2:
            pushKeyPress(283);
            break;
        case CMD_KEY_F3:
            pushKeyPress(284);
            break;
        case CMD_KEY_F4:
            pushKeyPress(285);
            break;
        case CMD_KEY_F5:
            pushKeyPress(286);
            break;
        case CMD_KEY_F6:
            pushKeyPress(287);
            break;
        case CMD_KEY_F7:
            pushKeyPress(288);
            break;
        case CMD_KEY_F8:
            pushKeyPress(289);
            break;
        case CMD_SHOW_ABOUT:
            display->showAbout(!display->isAboutActive());
            break;
        default:
            commandDispatched = false;
            break;
    }

    if (commandDispatched)
    {
        the_c64->TheDisplay->setStatusMessage(command.description);
    }

    if (prefsChanged)
    {
        the_c64->NewPrefs(prefs);
	    ThePrefs = *prefs;
    }

    delete prefs;
}

void OSD::setNewFile(const fileinfo_t& fileInfo)
{
    string diskPath = currentDirectory;
    if (diskPath.at(diskPath.size()-1) != NATIVE_SLASH)
    {
        diskPath.append(1, NATIVE_SLASH);
    }

    diskPath.append(fileInfo.name);

	Prefs *prefs = new Prefs(ThePrefs);

    strcpy(prefs->DrivePath[0], diskPath.c_str());

    int extPos = diskPath.rfind('.');
    if (extPos > 0)
    {
        string extension = diskPath.substr(extPos+1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        int driveType = DRVTYPE_DIR;
        if (extension == "prg")
        {
            int slashPos = diskPath.rfind(NATIVE_SLASH);
            if (slashPos > 0)
            {
                string parentDir = diskPath.substr(0, slashPos);
                strcpy(prefs->DrivePath[0], parentDir.c_str());
            }

            driveType = DRVTYPE_DIR;
        }
        else if (extension == "t64")
        {
            driveType = DRVTYPE_T64;
        }
        else // (extension == "d64")
        {
            driveType = DRVTYPE_D64;
        }

        prefs->DriveType[0] = driveType;
	}

	the_c64->NewPrefs(prefs);
	ThePrefs = *prefs;
	delete prefs;

    the_c64->TheDisplay->setStatusMessage("Inserted disk: " + fileInfo.name);

    //the_c64->Reset();
}

void OSD::updateCommandState(command_t& command)
{
    int id = command.id;
    switch (id)
    {
        case CMD_ENABLE_JOYSTICK1:
            command.state = ThePrefs.JoystickSwap ? STATE_SET : STATE_NORMAL;
            break;
        case CMD_ENABLE_JOYSTICK2:
            command.state = ThePrefs.JoystickSwap ? STATE_NORMAL : STATE_SET;
            break;
        case CMD_ENABLE_TRUEDRIVE:
            command.state = ThePrefs.Emul1541Proc ? STATE_SET : STATE_NORMAL;
            break;
        case CMD_WARP:
            command.state = ThePrefs.LimitSpeed ? STATE_NORMAL : STATE_SET;
            break;
        case CMD_SHOW_ABOUT:
            command.state = the_c64->TheDisplay->isAboutActive() ? STATE_SET : STATE_NORMAL;
            break;
        default:
            break;
    }
}

void OSD::pushKeyPress(int key)
{
    the_c64->TheInput->pushKeyPress(key);
}
