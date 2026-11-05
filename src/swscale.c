/************************************************************************
 * This file is part of Wizznic.                                        *
 * Copyright 2009-2015 Jimmy Christensen <dusted@dusted.dk>             *
 * Wizznic is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * Wizznic is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with Wizznic.  If not, see <http://www.gnu.org/licenses/>.     *
 ************************************************************************/

#include "swscale.h"
#include "settings.h"
#include "defs.h"
#include "pixel.h"

static SDL_Surface* scale;

SDL_Surface* swScaleInit( int sdlVideoModeFlags, int doScale )
{
  scale = SDL_SetVideoMode(SCREENW*doScale,SCREENH*doScale,16, sdlVideoModeFlags);
  SDL_Surface* screen = SDL_CreateRGBSurface(SDL_SWSURFACE, 320,240,16, scale->format->Rmask,scale->format->Gmask,scale->format->Bmask,0);

  //Set scaling
  setting()->scaleFactor= (float)scale->h/240.0;

  return( screen );
}

void swScale( SDL_Surface* screen, int doScale )
{
    if(doScale==2)
    {
      //Two pixels of the 16-bit scaled surface at a time, as one 32-bit
      //word: the same on big- and little-endian machines.
      int x,y;
      for(y=0; y< SCREENH; y++)
      {
        const uint16_t* src = (const uint16_t*)( (const char*)screen->pixels + screen->pitch*y );
        uint32_t* d0 = (uint32_t*)( (char*)scale->pixels + scale->pitch*(y*2) );
        uint32_t* d1 = (uint32_t*)( (char*)scale->pixels + scale->pitch*(y*2+1) );
        for(x=0; x < SCREENW; x++)
        {
          uint32_t c = src[x];
          c |= c << 16;
          d0[x] = c;
          d1[x] = c;
        }
      }
    } else if(doScale>2)
    {
      int x,y;
      SDL_Rect r;
      for(y=0; y< SCREENH; y++)
      {
        for(x=0; x < SCREENW; x++)
        {
          r.x=x*doScale;
          r.y=y*doScale;
          r.w=doScale;
          r.h=doScale;
          SDL_FillRect(scale, &r, freadPixel(screen,x,y));
        }
      }
    }

    if( doScale > -1 )
    {
      SDL_Flip(scale);
    }
}
