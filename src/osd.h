/*
 *  osd.h - On Screen Display
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#ifndef _OSD_H
#define _OSD_H

#include <vector>
#include <string>

class OSD
{
    private:
        C64* the_c64;
        C64Display* display;
        bool visible;

        SDL_Rect windowRect;
        SDL_Rect fileListFrame;
        SDL_Rect toolbarRect;

        int buttonWidth;
        int buttonHeight;

        int oscMaxLen;
        int oscFontWidth;
        int oscFontHeight;
        int oscColumnWidth;
        int oscRowHeight;
        int oscTitleHeight;

        std::string currentDirectory;
        std::vector<std::string> fileList;

        typedef enum
        {
            CMD_ENABLE_JOYSTICK1,
            CMD_ENABLE_JOYSTICK2,
            CMD_ENABLE_TRUEDRIVE,
            CMD_RESET,
            CMD_KEY_F1,
            CMD_KEY_F2,
            CMD_KEY_F3,
            CMD_KEY_F4,
            CMD_KEY_F5,
            CMD_KEY_F6,
            CMD_KEY_F7,
            CMD_KEY_F8,
            CMD_SHOW_ABOUT
        } commandid_t;

        typedef struct command_type
        {
            int id;
            std::string text;

            command_type(int id, const std::string& text)
            {
                this->id = id;
                this->text = text;
            }

        } command_t;
        std::vector<command_t> commandList;

    public:
        OSD();
        virtual ~OSD();

    public:
        void create(C64 *the_c64, C64Display* display);
        void show(bool doShow);
        bool isShown();
        void draw(SDL_Surface* surface, Uint32 fg, Uint32 bg, Uint32 border);
        bool onClick(int x, int y);

    private:
        void init();
        void cleanup();
        void layout(int w, int h);
        std::string getCachedDiskFilename(int idx);
        void update();
        void setNewFile(const std::string& filename);
        void onCommand(const command_t& command);

};

#endif /* _OSD_H */
