/*
 *  SID_SDL.h - 6581 emulation, SDL specific stuff
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 *  WIN32 code by J. Richard Sladkey <jrs@world.std.com>
 */

#include "VIC.h"
#include "main.h"
#include "C64.h"

DigitalRenderer::~DigitalRenderer()
{
    if (ready) {
        ready = false;
        SDL_CloseAudio();
    }
}

void mixAudio(void* userdata, uint8* stream, int len)
{
    ((DigitalRenderer *) userdata)->mixAudio(stream, len);
}

void DigitalRenderer::mixAudio(uint8* stream, int len)
{
    calc_buffer((int16 *) stream, len);
}

void DigitalRenderer::init_sound()
{
    ready = false;

    format.freq = SAMPLE_FREQ;
    format.format = AUDIO_S16;
    format.channels = 2;

    int numSamples = SAMPLE_FREQ / CALC_FREQ;

    format.samples = numSamples;
    format.callback = ::mixAudio;
    format.userdata = this;

    if ( SDL_OpenAudio(&format, NULL) < 0 ) {
        fprintf(stderr, "Unable to open sound device: %s\n", SDL_GetError());
    }

    SDL_PauseAudio(0);

    ready = true;
}

void DigitalRenderer::EmulateLine()
{
	if (!ready)
		return;

	sample_buf[sample_in_ptr] = volume;
	sample_in_ptr = (sample_in_ptr + 1) % SAMPLE_BUF_SIZE;

}

void DigitalRenderer::VBlank()
{
	if (!ready)
		return;
}

void DigitalRenderer::Pause()
{
	if (!ready)
		return;
}

void DigitalRenderer::Resume()
{
	if (!ready)
		return;
}
