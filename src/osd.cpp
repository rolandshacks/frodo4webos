/*
 *  osd.cpp - On Screen Display
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#include "sysdeps.h"

#include "C64.h"
#include "Display.h"
#include "main.h"
#include "Prefs.h"
#include "ndir.h"
#include "osd.h"

#include <algorithm>

#ifdef WIN32
#define NATIVE_SLASH '\\'
#else
#define NATIVE_SLASH '/'
#endif

using namespace std;

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

    windowRect.x = windowRect.y = windowRect.w = windowRect.h = 0;
    fileListFrame.x = fileListFrame.y = fileListFrame.w = fileListFrame.h = 0;
    toolbarRect.x = toolbarRect.y = toolbarRect.w = toolbarRect.h = 0;

    oscFontWidth = 8;
    oscFontHeight = 8;
    #ifdef WEBOS
    oscMaxLen = 24;
    oscRowHeight = 40;
    #else
    oscMaxLen = 16;
    oscRowHeight = 40;
    #endif
    oscColumnWidth = oscMaxLen*oscFontWidth + 12;

    oscTitleHeight = 12;

    commandList.push_back( command_t ( CMD_ENABLE_JOYSTICK1, "JOY1" ) );
    commandList.push_back( command_t ( CMD_ENABLE_JOYSTICK2, "JOY2" ) );
    commandList.push_back( command_t ( CMD_ENABLE_TRUEDRIVE, "DRV" ) );
    commandList.push_back( command_t ( CMD_RESET, "RESET" ) );

    commandList.push_back( command_t ( CMD_KEY_F1, "F1" ) );
    commandList.push_back( command_t ( CMD_KEY_F2, "F2" ) );
    commandList.push_back( command_t ( CMD_KEY_F3, "F3" ) );
    commandList.push_back( command_t ( CMD_KEY_F4, "F4" ) );
    commandList.push_back( command_t ( CMD_KEY_F5, "F5" ) );
    commandList.push_back( command_t ( CMD_KEY_F6, "F6" ) );
    commandList.push_back( command_t ( CMD_KEY_F7, "F7" ) );
    commandList.push_back( command_t ( CMD_KEY_F8, "F8" ) );

    commandList.push_back( command_t ( CMD_SHOW_ABOUT, "ABOUT" ) );
}

void OSD::create(C64* the_c64, C64Display* display)
{
    this->the_c64 = the_c64;
    this->display = display;

    buttonWidth = 50;
    buttonHeight = 50;

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

void OSD::show(bool doShow)
{
    if (doShow != visible)
    {
        visible = doShow;

        if (visible)
        {
            update();
        }
    }
}

bool OSD::isShown()
{
    return visible;
}

void OSD::layout(int w, int h)
{
    int borderX=5;
    int borderY=5;

    windowRect.x = borderX;
    windowRect.y = borderY;
    windowRect.w = w-borderX*2;
    windowRect.h = h-borderY*2;

    toolbarRect.x = windowRect.x+windowRect.w-buttonWidth;
    toolbarRect.y = windowRect.y+oscTitleHeight+1;
    toolbarRect.w = buttonWidth;
    toolbarRect.h = windowRect.h-oscTitleHeight-1;

    fileListFrame.x = windowRect.x;
    fileListFrame.y = windowRect.y+oscTitleHeight;
    fileListFrame.w = toolbarRect.x - windowRect.x;
    fileListFrame.h = windowRect.h-oscTitleHeight;
}

void OSD::draw(SDL_Surface* surface, Uint32 fg, Uint32 bg, Uint32 border)
{
    if (!visible) return;

    layout(surface->w, surface->h);

    int fontOffsetY = (oscRowHeight-oscFontHeight) / 2;

	SDL_FillRect(surface, NULL, bg);

    SDL_Rect titleRect = { windowRect.x,
                           windowRect.y,
                           windowRect.w,
                           oscTitleHeight };

    SDL_FillRect(surface, &titleRect, border);
	display->draw_string(surface, windowRect.x + 2, windowRect.y + (oscTitleHeight-oscFontHeight)/2, currentDirectory.c_str(), fg, border);

    // printf("DIR:%s\n", currentDirectory.c_str());

    int ystart = 0;
	int y = ystart;
    int x = 0;

    char buf[256];

    SDL_Rect entryRect;

    for (vector<string>::const_iterator it = fileList.begin();
         it != fileList.end();
         ++it)
    {
        const string& fileName = *it;

        strncpy(buf, fileName.c_str(), oscMaxLen);
        *(buf+oscMaxLen) = '\0';

		//printf("D64-File: %s\n", filename);

        entryRect.x = x+fileListFrame.x+1;
        entryRect.y = y+fileListFrame.y+1;
        entryRect.w = oscColumnWidth-2;
        entryRect.h = oscRowHeight-2;

        SDL_FillRect(surface, &entryRect, border);
		display->draw_string(surface, x+fileListFrame.x+2, y+fontOffsetY+fileListFrame.y, buf, fg, border);
		y += oscRowHeight;

        if (y >= fileListFrame.h)
        {
            y = ystart;
            x += oscColumnWidth;
            if (x >= fileListFrame.w)
            {
                break;
            }
        }

	}

	SDL_FillRect(surface, &toolbarRect, bg);
    SDL_Rect buttonRect = toolbarRect;

    buttonRect.h = buttonHeight;

    for (vector<command_t>::const_iterator it = commandList.begin();
         it != commandList.end();
         ++it)
    {
        const command_t& cmd = *it;

        if (buttonRect.y + buttonRect.h >= toolbarRect.y + toolbarRect.h)
            break;

        SDL_FillRect(surface, &buttonRect, border);
        display->draw_string(surface,
                             buttonRect.x + (buttonRect.w - cmd.text.size()*oscFontWidth)/2 ,
                             buttonRect.y + (buttonRect.h-oscFontHeight)/2,
                             cmd.text.c_str(), fg, border);

        buttonRect.y += buttonRect.h + 1;
    }
}

void OSD::update()
{

    fileList.clear();

    fileList.push_back("(DIR) Parent");

	DIR* dir = opendir (currentDirectory.c_str());
	if (NULL != dir) {

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
                        fileList.push_back(filename);
                    }
			    }
                else if (dir_entry->d_type == DT_DIR)
                {
                    string dirname = "(DIR) " + filename;
                    fileList.push_back(dirname);
                }
            }
		}

		closedir(dir);
		dir = NULL;
	}
}

string OSD::getCachedDiskFilename(int idx)
{
    if (idx < 0 || idx >= fileList.size()) return "";

    vector<string>::const_iterator it = fileList.begin() + idx;
    if (it == fileList.end()) return "";

    return *it;
}

bool OSD::onClick(int x, int y)
{
    if (!visible) return false;

    if (x >= fileListFrame.x && x < fileListFrame.x+fileListFrame.w &&
        y >= fileListFrame.y && y < fileListFrame.y+fileListFrame.h) 
    {

        int oscX = x-fileListFrame.x;
        int oscY = y-fileListFrame.y;

        int filesPerColumn = (fileListFrame.h+oscRowHeight-1) / oscRowHeight;
        int row = oscY / oscRowHeight;
        int col = oscX / oscColumnWidth;
        int idx = col*filesPerColumn + row;

        //printf("OSC %d / col=%d / row=%d / idx=%d\n", filesPerColumn, col, row, idx);

        string selectedFilename = getCachedDiskFilename(idx);
        if (!selectedFilename.empty())
        {
            printf("SELECTION: %s\n", selectedFilename.c_str());

            if (idx > 0 && selectedFilename.find("(DIR)") == 0)
            {
                if (currentDirectory.at(currentDirectory.size()-1) != NATIVE_SLASH)
                {
                    currentDirectory.append(1, NATIVE_SLASH);
                }
                currentDirectory.append(selectedFilename.substr(6));
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

                string diskPath = currentDirectory;
                if (diskPath.at(diskPath.size()-1) != NATIVE_SLASH)
                {
                    diskPath.append(1, NATIVE_SLASH);
                }

                diskPath.append(selectedFilename);

                setNewFile(diskPath);
            }
        }

        visible = false;
    }
    else if (x >= toolbarRect.x && x < toolbarRect.x+toolbarRect.w &&
             y >= toolbarRect.y && y < toolbarRect.y+toolbarRect.h) 
    {
        int ofsY = y - toolbarRect.y;
        int idx = ofsY / (buttonHeight+1);
        if (idx >= 0 && idx < commandList.size()) {

            vector<command_t>::const_iterator it = commandList.begin() + idx;
            if (it != commandList.end()) {
                onCommand(*it);
                visible = false;
            }
        }
    }

    return true;
}

void pushKeyPress(int key);

void OSD::onCommand(const OSD::command_t& command)
{
    // printf("COMMAND: %s\n", command.text.c_str());

    Prefs *prefs = new Prefs(ThePrefs);

    bool prefsChanged = false;

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
            display->showAbout(true);
            break;
        default:
            break;
    }

    if (prefsChanged)
    {
        the_c64->NewPrefs(prefs);
	    ThePrefs = *prefs;
    }

    delete prefs;
}

void OSD::setNewFile(const string& filename)
{
	Prefs *prefs = new Prefs(ThePrefs);

    strcpy(prefs->DrivePath[0], filename.c_str());

    int extPos = filename.rfind('.');
    if (extPos > 0)
    {
        string extension = filename.substr(extPos+1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        int driveType = DRVTYPE_DIR;
        if (extension == "prg")
        {
            int slashPos = filename.rfind(NATIVE_SLASH);
            if (slashPos > 0)
            {
                string parentDir = filename.substr(0, slashPos);
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

    //the_c64->Reset();
}
