/*
 *  texture.cpp - OpenGL texture
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */
#if USE_OPENGL
#ifndef __FONT_H
#define __FONT_H

typedef struct
{
    int lineSkip;
    int height;
    int ascent;
    int descent;
    char family[512];
} FontInfo;

class Font
{
    private:
        TTF_Font* obj;
        FontInfo info;
        bool attached;

    public:
        Font();
        Font(const char* name, float logSize);
        Font(const Font& font);
        virtual ~Font();
        Font operator= (const Font& font);

    public:
        bool create(const char* name, float logSize);
        void free();
        void* getInternal() const;
        float getHeight();
        const FontInfo& getInfo();
};

#endif // __FONT_H
#endif // USE_OPENGL