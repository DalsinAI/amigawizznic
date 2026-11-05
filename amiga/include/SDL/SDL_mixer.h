/* Wizznic for AmigaOS: the SDL_mixer 1.2 calls it makes, on SDL 1.2's own
 * audio (amiga/sdl_mixer_lite.c). Samples are WAV files read and converted
 * once to the output rate; music is a WAV (16-bit PCM or IMA ADPCM)
 * decoded as it plays. The OpenGPU kit's SDL 1.2 (sdl12-compat) has no
 * SDL_mixer 1.2, and its SDL2_mixer cannot be linked beside it.
 * The AmiCradle Team. GPL 3 or later, like Wizznic. */
#ifndef AMIGA_SDL_MIXER_LITE_H
#define AMIGA_SDL_MIXER_LITE_H
#include <SDL.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SDL_MIXER_MAJOR_VERSION 1
#define SDL_MIXER_MINOR_VERSION 2
#define SDL_MIXER_PATCHLEVEL 12
#define MIX_MAX_VOLUME 128
#define MIX_DEFAULT_FORMAT AUDIO_S16SYS
#define MIX_CHANNEL_POST (-2)

typedef struct _Mix_Music Mix_Music;

typedef struct Mix_Chunk {
	int allocated;
	Uint8 *abuf;	/* 16-bit mono at the output rate */
	Uint32 alen;
	Uint8 volume;
} Mix_Chunk;

typedef void (*Mix_EffectFunc_t)(int chan, void *stream, int len, void *udata);
typedef void (*Mix_EffectDone_t)(int chan, void *udata);

int Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize);
void Mix_CloseAudio(void);
int Mix_AllocateChannels(int numchans);

Mix_Chunk *Mix_LoadWAV(const char *file);
void Mix_FreeChunk(Mix_Chunk *chunk);
int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops);
int Mix_Playing(int channel);
int Mix_Volume(int channel, int volume);
int Mix_HaltChannel(int channel);
int Mix_SetPanning(int channel, Uint8 left, Uint8 right);
int Mix_RegisterEffect(int chan, Mix_EffectFunc_t f, Mix_EffectDone_t d, void *arg);

Mix_Music *Mix_LoadMUS(const char *file);
void Mix_FreeMusic(Mix_Music *music);
int Mix_PlayMusic(Mix_Music *music, int loops);
int Mix_FadeInMusic(Mix_Music *music, int loops, int ms);
int Mix_FadeInMusicPos(Mix_Music *music, int loops, int ms, double position);
int Mix_FadeOutMusic(int ms);
int Mix_HaltMusic(void);
void Mix_PauseMusic(void);
void Mix_ResumeMusic(void);
int Mix_PausedMusic(void);
int Mix_PlayingMusic(void);
int Mix_VolumeMusic(int volume);

#define Mix_GetError SDL_GetError
#define Mix_SetError SDL_SetError
#ifdef __cplusplus
}
#endif
#endif
