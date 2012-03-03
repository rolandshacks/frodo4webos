/*
 *  renderer.h - OpenGL rendering
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */
#if USE_OPENGL
#ifndef _RENDERER_H
#define _RENDERER_H

#include <string>

class Texture;
class Font;

// Class for rendering OpenGL graphics
class Renderer
{
    public:
        typedef enum
        {
            ALIGN_LEFT              = 0x01,
            ALIGN_CENTER            = 0x02,
            ALIGN_RIGHT             = 0x04,
            ALIGN_TOP               = 0x10,
            ALIGN_MIDDLE            = 0x20,
            ALIGN_BOTTOM            = 0x40,
            TEXTMODE_TRANSPARENT    = 0x100,
            TEXTMODE_FILL           = 0x200,
            TEXTMODE_BORDER         = 0x400,
        } PainterFlags;

    private:
        static float quadCoords[8];
        static float quadTexCoords[8];
        static float quadColors[16];

    private:
        bool disposed;
        int width;
        int height;
        Font* currentFont;
            
    private:
        static Renderer* __instance;

    public:
        static Renderer* getInstance();

    public:
        Renderer();
        virtual ~Renderer();

        void init(int width, int height);
        void shutdown();
        void resize(int width, int height);
        void beginDraw();
        void endDraw();

        int getWidth() const;
        int getHeight() const;

    public:
        void setFont(Font* font);
        Font* getFont();
        void drawText(float x, float y, const char* text, int flags=0);

    public:
        static void drawTexture(Texture* texture, float x, float y, float width=-1.0f, float height=-1.0f);
        static void drawTiledTexture(Texture* texture, float x, float y, float width, float height, bool horizontal=true, bool vertical=true);
        static void fillRectangle(float x, float y, float width, float height);
        static void draw(int mode, int count, float* coords, float* colors, float* texCoords);
        static void drawQuad(int mode,
                      float x1, float y1,
                      float x2, float y2,
                      float x3, float y3,
                      float x4, float y4,
                      float u1, float v1,
                      float u2, float v2,
                      float u3, float v3,
                      float u4, float v4);

        void enableClipping(int x, int y, int w, int h);
        void disableClipping();

};

#endif /* _RENDERER_H */
#endif /* USE_OPENGL */
