#ifndef AMIGA_H_INCLUDED
#define AMIGA_H_INCLUDED

/* AmigaOS 3.x (68040/68060 + FPU, RTG) on OpenGPU's SDL 1.2 (sdl12-compat).
 * As the PC build, without OpenGL, the online stats, the DLC download,
 * -record and -d: no network and no shell tools here. Data, settings and
 * highscores live in the program's drawer (DATADIR is PROGDIR:). */

#define SCREENW 320
#define SCREENH 240

#define STR_PLATFORM "AmigaOS"

/* the mixer (amiga/sdl_mixer_lite.c) plays at 22050 Hz whatever is asked */
#define SOUND_RATE  22050
#define SOUND_FORMAT  AUDIO_S16SYS
#define SOUND_BUFFERS 1024
#define SOUND_MIX_CHANNELS 16

/* The drawer's sounds and music are WAV files made from upstream's Ogg
 * Vorbis ones (amiga/tools/ogg2wav.py): the 68k does not decode Vorbis. */
#define SOUND_EXT ".wav"

/* The PC keys, as data/menu/helppc.png shows them */
#define PLATFORM_HELP_FILE        "data/menu/helppc.png"
#define PLATFORM_BUTTON_UP        SDLK_UP
#define PLATFORM_BUTTON_DOWN      SDLK_DOWN
#define PLATFORM_BUTTON_LEFT      SDLK_LEFT
#define PLATFORM_BUTTON_RIGHT     SDLK_RIGHT
#define PLATFORM_BUTTON_X         SDLK_x
#define PLATFORM_BUTTON_Y         SDLK_z
#define PLATFORM_BUTTON_A         SDLK_LALT
#define PLATFORM_BUTTON_B         SDLK_LCTRL
#define PLATFORM_SHOULDER_LEFT    SDLK_1
#define PLATFORM_SHOULLER_RIGHT   SDLK_2
#define PLATFORM_BUTTON_MENU      SDLK_ESCAPE
#define PLATFORM_BUTTON_SELECT    SDLK_SPACE
#define PLATFORM_BUTTON_VOLUP     0
#define PLATFORM_BUTTON_VOLDOWN   0
/* digital Amiga joysticks give full deflection */
#define PLATFORM_JOY_DEADZONE     8000

/* -z 2 and up: upstream's software scaler */
#define WANT_SWSCALE

#endif
