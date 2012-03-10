/*
 *  osd.h - On Screen Display
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#ifndef _OSD_H
#define _OSD_H

#include <vector>
#include <string>

class OSD : public InputHandler
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

        bool justOpened;

        float pos;

        /*
        int oscWidth;
        int oscMaxLen;
        int oscFontWidth;
        int oscFontHeight;
        int oscRowHeight;
        int oscTitleHeight;
        */

        typedef struct
        {
            bool    valid;

            int     width;
            int     height;
            int     titleHeight;

            int     buttonHeight;
            int     itemHeight;

        } layout_t;

        layout_t    layout;

        bool mousePressed;
        bool mouseGesture;
        int mousePressPosX;
        int mousePressPosY;
        int mouseX;
        int mouseY;

        std::string currentDirectory;

        typedef struct
        {
            std::string name;
            bool isDirectory;
        } fileinfo_t;

        std::vector<fileinfo_t> fileList;

        typedef enum
        {
            CMD_ENABLE_JOYSTICK1,
            CMD_ENABLE_JOYSTICK2,
            CMD_ENABLE_TRUEDRIVE,
            CMD_ENABLE_FILTERING,
            CMD_RESET,
            CMD_WARP,
            CMD_KEY_F1,
            CMD_KEY_F2,
            CMD_KEY_F3,
            CMD_KEY_F4,
            CMD_KEY_F5,
            CMD_KEY_F6,
            CMD_KEY_F7,
            CMD_KEY_F8,
            CMD_SAVE_SNAPSHOT,
            CMD_LOAD_SNAPSHOT,
            CMD_SHOW_ABOUT
        } commandid_t;

        typedef enum
        {
            STATE_NORMAL,
            STATE_SET
        } commandstate_t;

        typedef struct command_type
        {
            int id;
            std::string text;
            std::string description;
            int state;
            bool autoClose;

            command_type(int id, const std::string& text, const std::string& description, bool autoClose=true)
            {
                this->id = id;
                this->text = text;
                this->description = description;
                this->state = STATE_NORMAL;
                this->autoClose = autoClose;
            }

        } command_t;
        std::vector<command_t> commandList;

    private:
        Renderer* renderer;

    public:
        OSD();
        virtual ~OSD();

    public:
        void create(Renderer* renderer, C64 *the_c64, C64Display* display);
        void show(bool doShow);
        bool isShown();
        void draw(float elapsedTime, resource_list_t* res);

    public: // implements InputHandler
        void handleMouseEvent(int x, int y, int eventType);
        void handleKeyEvent(int key, int sym, int eventType);

    public:
        int getWidth() const;

    private:
        bool onClick(int x, int y);
        void init();
        void cleanup();
        void updateLayout(int w, int h, float elapsedTime, resource_list_t* res);
        fileinfo_t getCachedFileInfo(int idx);
        void update();
        void setNewFile(const fileinfo_t& fileInfo);
        void onCommand(const command_t& command);
        void updateCommandState(command_t& command);
        void pushKeyPress(int key);
        void drawFiles(resource_list_t* res);
        void drawToolbar(resource_list_t* res);
        bool insideFileList(int x, int y);
        std::string getLimitedPath(const std::string& path);

    private:
        int scrollElementTop;
        int scrollPixelRange;
        float scrollPixelOffset;
        std::string getDateString();
};

#endif /* _OSD_H */
