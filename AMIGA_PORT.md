Wizznic for AmigaOS 3.x
=======================

Wizznic! 1.1 (Jimmy Christensen's block puzzle after Puzznic; upstream
https://github.com/DusteDdk/Wizznic, tag 1.1, commit
37f37cf2df14a7bc82575d2b0a675559c23e9ce9, GPL 3 or later, its media
GPL 3 too) for AmigaOS 3.2 on a 68040 or 68060 with an FPU and an RTG
screen, on OpenGPU's SDL 1.2 (sdl12-compat on SDL 2). The port is on
branch team2/wizznic-amiga.

Status: builds; not yet run in the lab.


What you need
-------------

- AmigaOS 3.2, a 68040/68060 with an FPU (not an LC040 or EC040), an RTG
  screen that SDL 2 can open a 320x240 (or 640x480) 16-bit window on.
- OpenGPU 0.6 or later (OpenUp installs it): opengpu.library and
  LIBS:OpenGPU/SDL2.module (picture, keyboard, mouse, joystick, sound
  through AHI). Nothing else: pictures and sound are read by the program.
- About 30 MB of disk and 16 MB of free memory (the menu music and one
  level's track are kept in memory, about 1.2 and up to 2.8 MB).


How to play
-----------

Double-click Wizznic in its drawer, or from a Shell (from any drawer: it
finds everything through PROGDIR:):

    Wizznic           a 320x240 window
    Wizznic -z 2      a 640x480 window (the game draws 320x240, the 68k
                      doubles it: slower)
    Wizznic -f        full screen (remembered; -w goes back to a window)

Settings (settings.ini), highscores (highscores/) and levels made in the
editor (editorlevels/) are written in the drawer, not in a home drawer.

Keys (as data/menu/helppc.png shows them; the menus say "Press Ctrl"):

    cursor keys     move the cursor, choose in menus
    Ctrl            grab a brick (hold it and push left or right),
                    choose in menus
    Space           reset the bricks (restart the level)
    Esc             pause and the menu, back
    Alt             delete a letter (typing a name); try a level in the
                    editor's list
    X / Z           level editor: put / delete a brick (Space saves)
    1 / 2           next / previous track, with your own music
    mouse           the menus, and the game (the pointer shows when moved)

A joystick works as cursor keys and buttons too.

Bricks of the same kind that touch vanish; clear them all before the time
runs out. Bricks fall, so think about what lands where. "Wizznic! 1"
starts with a tutorial level.


How it is built: the route
--------------------------

Wizznic is an SDL 1.2 program that draws 320x240 in 16-bit colour (it
reads and writes 16-bit pixels itself for its particles and effects), so
the Team's 8-bit acsdl12 shim can't run it. The OpenGPU kit's SDL 1.2
(sdl12-compat on SDL 2, lib/libSDL.a) does, as for OpenLieroX. The kit
has no SDL_image or SDL_mixer 1.2, and its SDL2_mixer can't be linked
beside libSDL.a (its stub calls libSDL2.a, which must not be linked with
it), so the port has small ones of its own (amiga/):

- sdl_image_lite.c, png_rgba.c: IMG_Load on libpng, 32-bit surfaces
  (Wizznic's pictures are all PNG; loadImg() then makes them 16-bit).
- sdl_mixer_lite.c: the SDL_mixer calls sound.c makes, on SDL 1.2's audio
  callback: 16 channels of sound effects with volume and left/right
  panning, mixed into 16-bit stereo at 22050 Hz, and one music track with
  loops, fades, pause, volume and starting at a given second (Wizznic
  keeps its own track positions). WAV only: PCM samples are converted to
  the output rate once, at load; music is PCM or mono IMA ADPCM, decoded a
  block at a time as it plays (a few per cent of a 68040). The files are
  read with stdio: sdl12-compat's SDL_LoadWAV_RW never returned in the
  lab (OpenLieroX port).

The sounds and music are Ogg Vorbis upstream; decoding Vorbis would take
much of a 68040. So the drawer is made with WAVs instead
(amiga/tools/ogg2wav.py, on the host, with GStreamer): effects as 16-bit
PCM, music as IMA ADPCM (4 bits a sample, 11 KB a second), both mono at
22050 Hz. The packs' playlists (info.ini) are changed to the .wav names,
and sound.c takes SOUND_EXT (".wav" here, ".ogg" elsewhere).

Changes to upstream's files:

- src/platform/amiga.h (new), src/defs.h: the Amiga platform: the PC's
  keys and 320x240, no OpenGL, no online stats or DLC (curl), no -record
  (ffmpeg) or -d (picture dumps): the PC code isn't compiled.
- src/userfiles.c: the settings drawer is PROGDIR:; DATADIR is PROGDIR:
  (Makefile). src/settings.c: the user music drawer starts at PROGDIR:
  (libnix's getcwd(NULL, 0) is not to be trusted).
- src/sound.c: SOUND_EXT for every sound and the menu music.
  src/mbrowse.c: your own music is WAV (PCM, or mono IMA ADPCM) here.
- src/main.c: "Loading..." on the screen while the packs, sounds and music
  load; amiga/amiga_screens.c (from the Seven Kingdoms port) closes the
  empty screens SDL2.module 0.6 leaves after a full-screen quit.
- src/swscale.c: -z 2 wrote each pair of pixels as one 32-bit word with
  the colour in its low half, which is the second pixel on a big-endian
  machine (and wrote past the line); it now writes both. The scaled
  surface asked for an alpha mask outside its 16 bits, which SDL 2 refuses.
- amiga/amiga_paths.c (from the Commander Genius and OpenLieroX ports):
  the program is linked with --wrap for fopen, open, stat, opendir,
  mkdir, unlink and the rest, so that upstream's joins
  ("PROGDIR:" "/" "settings.ini", ".../000_wizznic" "/" "./" "/" "x.png")
  become AmigaDOS names.

Big-endian check: the levels, playlists and settings are text. The
highscore files (*.hig) are C structs written as they are in memory: the
Amiga reads its own back, but a .hig from a PC is refused (wrong version
number) and deleted, as upstream does with an old file. Upstream's DLC
bundles (.wiz) are structs too; none come here (no network). The 16-bit
pixel code works on native pixels.


Building
--------

    make -f amiga/Makefile.amigaos -j2

with GFOpenAmigaGCC 16.2.0-open4 (STOVE), the OpenNDK (NDK), the OpenGPU
developer kit (SDLKIT) and the Team's static libpng and zlib (DEPS,
amiga/deps/build-m68k-libs.sh); the defaults are the Team's paths. The
program is build/amigaos/Wizznic; the drawer is dist/Wizznic, with
GlowIcons (amiga/icons, port_icons.py). Making the drawer needs Python 3
and GStreamer's gst-launch-1.0 with the Vorbis plugin on the host.


Data and licences
-----------------

See Licences. All of Wizznic's media are GPL 3 (upstream's
media-licenses.txt). The drawer has data/ and the three packs made by the
game's author and his team (Wizznic! 1, Wizznic! 2, Wizznic Silver).
Packs 200-208 (the NES Puzznic levels and Puzznic stages 4-14, retyped by
fans, no licence of their own) are left out.


Known limits
------------

- Not yet run on an Amiga past the loading screen.
- Your own music (Options) must be WAV, PCM or mono IMA ADPCM; Ogg and
  MP3 are not played.
- No online highscores, DLC or level upload; the menus' "Allow Online"
  question is not asked.
- The window is 320x240 unless -z 2 or -f; from the Workbench the last
  -f/-w choice holds.
