/*
 *  texture.cpp - OpenGL texture
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#include "sysdeps.h"
#if USE_OPENGL

#include <SDL_image.h>
#include "texture.h"

static void clearGlError()
{
    glGetError();
}

static void checkGlErrorImpl(const char* file, int line)
{
    GLenum errCode = glGetError();
    if (errCode != GL_NO_ERROR)
    {
        printf("%s(%d): opengl error 0x%04x\n", file, line, (int) errCode);
    }
}

#define checkGlError() checkGlErrorImpl(__FILE__, __LINE__)

#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif

#ifndef GL_GENERATE_MIPMAP_HINT
#define GL_GENERATE_MIPMAP_HINT 0x8192
#endif

Texture::Texture()
{
    init();
}

Texture::Texture(const char* name)
{
    init();
    load(name);
}

Texture::Texture(int width, int height, int bitsPerPixel)
{
    init();
    create(width, height, bitsPerPixel);
}

Texture::~Texture()
{
    free();
}

void Texture::init()
{
    valid = false;

    textureId = 0;
    width = height = bitsPerPixel = pitch = size = 0;
    buffer = NULL;
    bufferSize = 0;
    internalFormat = format = 0;

    paletteSize = 0;
    paletteRed = paletteGreen = paletteBlue = NULL;

    filtering = false;
}

bool Texture::load(const char* name)
{
    SDL_Surface* surface = IMG_Load(name);
    if (NULL == surface)
    {
        printf("FAILED TO LOAD BITMAP: %s\n", name);
        return false;
    }

    bool status = create(surface->w,
                         surface->h,
                         surface->format->BitsPerPixel,
                         surface->pixels);

    SDL_FreeSurface(surface);
    surface = NULL;

    return status;
}

bool Texture::create(int width, int height, int bitsPerPixel, const void* pixels, SDL_Color* palette, int paletteSize)
{
    this->width = width;
    this->height = height;
    this->bitsPerPixel = bitsPerPixel;
    this->pitch = (width * bitsPerPixel) / 8;
    this->size = height * pitch;

    internalFormat = 0;
    format = 0;

    bool usePalette = true;

    switch (bitsPerPixel)
    {
        case 32:
            internalFormat = GL_RGBA;
            format = GL_RGBA;
            break;
        case 24:
            internalFormat = GL_RGB;
            format = GL_RGB;
            break;
        case 8:
            if (usePalette)
            {
                #ifndef HAVE_GLES
                    internalFormat = GL_RGB;
                    format = GL_COLOR_INDEX;
                #else
                    internalFormat = GL_PALETTE8_RGB8_OES;
                    format = 0;
                #endif
            }
            else
            {
                internalFormat = 1;
                format = GL_LUMINANCE;
            }
            break;
        default:
            return false;
    }

    textureId = 0;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);

    //glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    //glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);

    if (8 == bitsPerPixel && usePalette)
    {
        this->paletteSize  = paletteSize;
        if (this->paletteSize < 256) this->paletteSize = 256;
        paletteRed   = new float[this->paletteSize];
        paletteGreen = new float[this->paletteSize];
        paletteBlue  = new float[this->paletteSize];

        for (int i=0; i<paletteSize; i++)
        {
            paletteRed[i]   = (float) palette[i].r / 256.0f;
            paletteGreen[i] = (float) palette[i].g / 256.0f;
            paletteBlue[i]  = (float) palette[i].b / 256.0f;
        }

        enablePalette();
    }

    if (NULL != pixels)
    {
        storePixelData(pixels);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    disablePalette();

    return true;

}

void Texture::free()
{
    if (0 != textureId)
    {
        glDeleteTextures(1, &textureId);
        textureId = 0;
    }

    if (NULL != buffer)
    {
        delete [] buffer;
        buffer = NULL;
    }

    freePalette();

    bufferSize = 0;

    valid = false;
}

int Texture::getWidth() const
{
    return width;
}

int Texture::getHeight() const
{
    return height;
}

int Texture::getBitsPerPixel() const
{
    return bitsPerPixel;
}

void Texture::bind()
{
    glBindTexture(GL_TEXTURE_2D, textureId);

    if (filtering)
    {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else
    {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    enablePalette();

}

void Texture::unbind()
{
    glBindTexture(GL_TEXTURE_2D, 0);

    disablePalette();
}

void Texture::setAntialias(bool enabled)
{
    filtering = enabled;
}

bool Texture::isAntialiased() const
{
    return filtering;
}

void Texture::enablePalette()
{
    #ifndef HAVE_GLES
        if (!hasPalette()) return;

        glPixelTransferi(GL_INDEX_SHIFT, 0);
        glPixelTransferi(GL_INDEX_OFFSET, 0);
        glPixelTransferi(GL_MAP_COLOR, GL_TRUE);

        glPixelMapfv(GL_PIXEL_MAP_I_TO_R, paletteSize, paletteRed);
        glPixelMapfv(GL_PIXEL_MAP_I_TO_G, paletteSize, paletteGreen);
        glPixelMapfv(GL_PIXEL_MAP_I_TO_B, paletteSize, paletteBlue);

    #endif
}

void Texture::disablePalette()
{
    #ifndef HAVE_GLES

        if (!hasPalette()) return;
        glPixelTransferi(GL_MAP_COLOR, GL_FALSE);

    #endif
}

void Texture::freePalette()
{
    paletteSize = 0;

    if (NULL != paletteRed)
    {
        delete [] paletteRed;
        paletteRed = NULL;
    }

    if (NULL != paletteGreen)
    {
        delete [] paletteGreen;
        paletteGreen = NULL;
    }

    if (NULL != paletteBlue)
    {
        delete [] paletteBlue;
        paletteBlue = NULL;
    }

}

void Texture::updateData(const void* pixels, int bitsPerPixel, uint32* palette)
{
    clearGlError();

    bool gles = false;

    #ifdef HAVE_GLES
        gles = true;
    #endif

    const void* texture_data = pixels;

    if (bitsPerPixel != this->bitsPerPixel)
    {
        int src_pitch = (width * bitsPerPixel) / 8;

        int texture_pitch = (width * this->bitsPerPixel) / 8;
        int texture_size = height * texture_pitch;

        if (NULL == buffer || bufferSize != texture_size)
        {
            if (NULL != buffer) delete [] buffer;
            buffer = new uint8[texture_size];
            bufferSize = texture_size;
        }

        texture_data = buffer;

        uint8* srcLine  = (uint8*)  pixels;
        uint8* destLine = (uint8*) buffer;

        for (int y=0; y<height; y++)
        {
            uint8*  src  = srcLine;
            uint32* dest = (uint32*) destLine;

            int i;
            SDL_Color* pal;
            uint32 c;

            for (int x=0; x<width/4; x++)
            {
                i = *(src++) & 0x0f;
                *(dest++) = palette[i];
                i = *(src++) & 0x0f;
                *(dest++) = palette[i];
                i = *(src++) & 0x0f;
                *(dest++) = palette[i];
                i = *(src++) & 0x0f;
                *(dest++) = palette[i];
            }

            srcLine  += src_pitch;
            destLine += texture_pitch;
        }
    }

    if (!valid)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
                     width, height, 0, format, GL_UNSIGNED_BYTE, texture_data);
        checkGlError();
        valid = true;
    }
    else
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        width, height, format, GL_UNSIGNED_BYTE, texture_data);
        checkGlError();
    }

}

bool Texture::hasPalette() const
{
    return (paletteSize > 0);
}

void Texture::updateData(const void* pixels)
{
    if (0 == textureId || NULL == pixels) return;

    enablePalette();

    storePixelData(pixels);

    disablePalette();
}

void Texture::storePixelData(const void* pixels)
{
    clearGlError();

    bool gles = false;

    #ifdef HAVE_GLES
        gles = true;
    #endif

    if (!gles || !hasPalette())
    {
        if (!valid)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
            checkGlError();
            valid = true;
        }
        else
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, pixels);
            checkGlError();
        }

    }
    else
    {
    #ifdef HAVE_GLES

        printf("PALETTESIZE: %d\n", paletteSize);

        int colorSize = paletteSize*3; // RGB
        int imageSize = (width * height * bitsPerPixel) / 8;
        int bufferSize = colorSize + imageSize;

        uint8_t* buffer = new uint8_t[bufferSize];

        uint8_t* colors = buffer;
        for (int i=0; i<paletteSize; i++)
        {
            uint8_t r = (uint8_t) (paletteRed[i] * 255.999f);
            uint8_t g = (uint8_t) (paletteGreen[i] * 255.999f);
            uint8_t b = (uint8_t) (paletteBlue[i] * 255.999f);

            *(colors++) = r;
            *(colors++) = g;
            *(colors++) = b;
        }

        memcpy(buffer+colorSize, pixels, imageSize);

        for (int y=0; y<height; y++)
        {
            for (int x=0; x<width; x++)
            {
                *(buffer+colorSize+(y*width)+x) = (y+x)%256;

            }

        }

        if (!valid)
        {
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, bufferSize, buffer);
            checkGlError();
            valid = true;
        }
        else
        {
            glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, 0, bufferSize, buffer);
            checkGlError();
        }

        delete [] buffer;

    #endif

    }
}

#endif /* USE_OPENGL */
