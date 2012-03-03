#ifndef __RESOURCES_H
#define __RESOURCES_H

#define RES_BACKGROUND          "resources/background.png"
#define RES_BUTTON              "resources/button.png"
#define RES_BUTTON_PRESSED      "resources/button_pressed.png"
#define RES_TAP_ZONES           "resources/help.png"
#define RES_FONT                "resources/font.ttf"
#define RES_ICON_DISK_ACTIVITY  "resources/icon_disk_activity.png"
#define RES_ICON_DISK_ERROR     "resources/icon_disk_error.png"
#define RES_ICON_DISK           "resources/icon_disk.png"
#define RES_ICON_FOLDER         "resources/icon_folder.png"
#define RES_ICON_CLOSE          "resources/icon_close.png"
#define RES_STICK               "resources/stick.png"

typedef struct
{
    bool initialized;

    Texture* screenTexture;
    Texture* backgroundTexture;
    Texture* buttonTexture;
    Texture* buttonPressedTexture;
    Texture* tapZonesTexture;
    Texture* stickTexture;

    Texture* iconDiskActivity;
    Texture* iconDiskError;
    Texture* iconDisk;
    Texture* iconFolder;
    Texture* iconClose;

    Font* fontTiny;
    Font* fontSmall;
    Font* fontNormal;
    Font* fontLarge;
} resource_list_t;

#endif