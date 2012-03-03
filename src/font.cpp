/*
 *  texture.cpp - OpenGL texture
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#include "sysdeps.h"
#if USE_OPENGL

#include "renderer.h"
#include "font.h"

Font::Font()
{
    obj = NULL;
    attached = false;

    info.ascent = info.descent = info.height = info.lineSkip = 0;
    strcpy(info.family, "");
}

Font::Font(const char* name, float logSize)
{
    obj = NULL;
    attached = false;

    create(name, logSize);
}

Font::Font(const Font& font)
{
    obj = font.obj;
    info = font.info;
    attached = true;
}

Font::~Font()
{
    free();
}

Font Font::operator= (const Font& font)
{
    free();

    obj = font.obj;
    info = font.info;
    attached = true;

    return *this;
}

bool Font::create(const char* name, float logSize)
{
    free();

    int pixelSize = (int) logSize;

    obj = TTF_OpenFont(name, pixelSize);
    if (NULL == obj) return false;

    info.lineSkip = TTF_FontLineSkip(obj);
    info.height = TTF_FontHeight(obj);
    info.ascent = TTF_FontAscent(obj);
    info.descent = TTF_FontDescent(obj);
    strcpy(info.family, TTF_FontFaceFamilyName(obj));

    return (NULL != obj);
}

float Font::getHeight()
{
    if (NULL == obj) return 0.0f;

    return info.height;
}

void Font::free()
{
    if (!attached && NULL != obj)
    {
        TTF_CloseFont(obj);
    }

    attached = false;
    obj = NULL;
}

void* Font::getInternal() const
{
    return (void*) obj;
}

const FontInfo& Font::getInfo()
{
    return info;
}

#endif
