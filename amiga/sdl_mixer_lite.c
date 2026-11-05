/* The SDL_mixer 1.2 calls Wizznic makes, on SDL 1.2's audio
 * (see include/SDL/SDL_mixer.h).
 *
 * Output: 16-bit signed stereo, native order, 22050 Hz, whatever the game
 * asks (it asks 44100); SDL_OpenAudio is given no "obtained" spec, so SDL
 * converts if the device wants something else.
 *
 * Samples: WAV, 8/16-bit PCM, mono or stereo, any rate, converted once at
 * load to 16-bit mono at the output rate, so the 68k only adds them up,
 * each with its channel's volume and left/right panning.
 *
 * Music: one track at a time, WAV, either 16-bit PCM (converted at load,
 * like a sample) or mono IMA ADPCM (format 0x11), kept as it is in memory
 * and decoded a block at a time as it plays (the drawer's music is made so
 * by amiga/tools/ogg2wav.py: about 11 KB a second). Loops, fade in and out,
 * pause, volume and starting at a position in seconds, as sound.c uses them.
 *
 * The files are read with stdio, not SDL_RWops: sdl12-compat's
 * SDL_LoadWAV_RW never returned in the lab (OpenLieroX port).
 *
 * The AmiCradle Team. GPL 3 or later, like Wizznic. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL_mixer.h"

#define OUT_RATE 22050
#define MAX_CHANS 16
#define SEG 512

struct channel {
	Mix_Chunk *chunk;
	Uint32 pos;	/* in samples */
	int volume;	/* 0..128 */
	int left, right;	/* 0..255 */
};

enum { MUS_PCM, MUS_ADPCM };

struct _Mix_Music {
	int type;
	Uint32 frames;	/* samples in the track */
	Uint32 step;	/* source samples per output sample, 16.16 */
	Sint16 *pcm;	/* MUS_PCM: mono, at the output rate */
	Uint8 *data;	/* MUS_ADPCM: the blocks */
	Uint32 nblocks, blockalign, spb;
	Sint16 *cbuf;	/* the decoded block */
	Uint32 cstart;	/* its first sample, or ~0 */
};

static struct channel chans[MAX_CHANS];
static int nchans = 8;
static int opened;
static int outchans = 2;

/* music */
static Mix_Music *mus;
static int mus_playing, mus_paused, mus_loops;
static Uint32 mus_pos, mus_frac;
static int mus_volume = MIX_MAX_VOLUME;
static int fade_mode;	/* 0 none, 1 in, 2 out */
static Uint32 fade_total, fade_left;

static Sint32 accl[SEG], accr[SEG];

static const int step_table[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
	50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
	253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
	1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
	3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
	11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
	32767
};
static const int index_table[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };

/* one mono IMA ADPCM block: a 4-byte header (first sample, step index),
 * then two samples a byte, low nibble first */
static void adpcm_block(Mix_Music *m, Uint32 b)
{
	const Uint8 *p = m->data + b * m->blockalign;
	Sint16 *out = m->cbuf;
	int pred = (Sint16)(p[0] | (p[1] << 8));
	int index = p[2];
	Uint32 i, n = m->spb;

	if (index > 88)
		index = 88;
	*out++ = (Sint16)pred;
	p += 4;
	for (i = 1; i < n; i++) {
		int code = (i & 1) ? (*p & 15) : (*p++ >> 4);
		int step = step_table[index];
		int vp = step >> 3;
		if (code & 4) vp += step;
		if (code & 2) vp += step >> 1;
		if (code & 1) vp += step >> 2;
		pred += (code & 8) ? -vp : vp;
		if (pred > 32767) pred = 32767;
		else if (pred < -32768) pred = -32768;
		index += index_table[code];
		if (index < 0) index = 0;
		else if (index > 88) index = 88;
		*out++ = (Sint16)pred;
	}
	m->cstart = b * m->spb;
}

static void music_mix(int n)
{
	Mix_Music *m = mus;
	int i, gain, fade = 256;

	if (!m || !mus_playing || mus_paused)
		return;
	if (fade_mode && fade_total) {
		Uint32 done = fade_total - fade_left;
		fade = (int)(((fade_mode == 1 ? done : fade_left) * 256u) / fade_total);
		fade_left = fade_left > (Uint32)n ? fade_left - (Uint32)n : 0;
		if (fade_left == 0) {
			if (fade_mode == 2) {
				mus_playing = 0;
				fade_mode = 0;
				return;
			}
			fade_mode = 0;
		}
	}
	gain = (mus_volume * fade) >> 1;	/* 0..16384 */
	for (i = 0; i < n; i++) {
		Sint32 s;
		if (mus_pos >= m->frames) {
			if (mus_loops == 0) {
				mus_playing = 0;
				return;
			}
			if (mus_loops > 0)
				mus_loops--;
			mus_pos = 0;
			mus_frac = 0;
		}
		if (m->type == MUS_PCM)
			s = m->pcm[mus_pos];
		else {
			if (mus_pos < m->cstart || mus_pos - m->cstart >= m->spb)
				adpcm_block(m, mus_pos / m->spb);
			s = m->cbuf[mus_pos - m->cstart];
		}
		s = (s * gain) >> 14;
		accl[i] += s;
		accr[i] += s;
		mus_frac += m->step;
		mus_pos += mus_frac >> 16;
		mus_frac &= 0xFFFF;
	}
}

static void mix_callback(void *user, Uint8 *stream, int len)
{
	Sint16 *out = (Sint16 *)stream;
	int frames = len / (2 * outchans), i, c;
	(void)user;

	while (frames > 0) {
		int n = frames > SEG ? SEG : frames;
		memset(accl, 0, sizeof(Sint32) * n);
		memset(accr, 0, sizeof(Sint32) * n);
		music_mix(n);
		for (c = 0; c < nchans; c++) {
			struct channel *ch = &chans[c];
			const Sint16 *src;
			int left, todo, base, gl, gr;
			if (!ch->chunk)
				continue;
			left = (int)((ch->chunk->alen - ch->pos) / 2);
			todo = left < n ? left : n;
			src = (const Sint16 *)(ch->chunk->abuf + ch->pos);
			base = ch->volume * ch->chunk->volume;	/* 0..16384 */
			gl = (base * ch->left) >> 8;
			gr = (base * ch->right) >> 8;
			for (i = 0; i < todo; i++) {
				Sint32 s = src[i];
				accl[i] += (s * gl) >> 14;
				accr[i] += (s * gr) >> 14;
			}
			ch->pos += (Uint32)todo * 2;
			if (ch->pos >= ch->chunk->alen)
				ch->chunk = NULL;
		}
		if (outchans == 2) {
			for (i = 0; i < n; i++) {
				Sint32 l = accl[i], r = accr[i];
				*out++ = (Sint16)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
				*out++ = (Sint16)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
			}
		} else {
			for (i = 0; i < n; i++) {
				Sint32 v = (accl[i] + accr[i]) >> 1;
				*out++ = (Sint16)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
			}
		}
		frames -= n;
	}
}

int Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize)
{
	SDL_AudioSpec want;
	int c;
	(void)frequency;
	(void)format;

	if (opened)
		return 0;
	memset(&want, 0, sizeof(want));
	want.freq = OUT_RATE;
	want.format = AUDIO_S16SYS;
	want.channels = (Uint8)(channels == 1 ? 1 : 2);
	want.samples = (Uint16)(chunksize >= 1024 ? 1024 : 512);
	want.callback = mix_callback;
	/* no "obtained": SDL converts to this format if the device differs */
	if (SDL_OpenAudio(&want, NULL) < 0)
		return -1;
	outchans = want.channels;
	for (c = 0; c < MAX_CHANS; c++) {
		chans[c].chunk = NULL;
		chans[c].volume = MIX_MAX_VOLUME;
		chans[c].left = chans[c].right = 255;
	}
	opened = 1;
	SDL_PauseAudio(0);
	return 0;
}

void Mix_CloseAudio(void)
{
	if (!opened)
		return;
	SDL_CloseAudio();
	opened = 0;
	mus = NULL;
	mus_playing = 0;
}

int Mix_AllocateChannels(int numchans)
{
	if (numchans < 0)
		return nchans;
	SDL_LockAudio();
	nchans = numchans > MAX_CHANS ? MAX_CHANS : numchans;
	SDL_UnlockAudio();
	return nchans;
}

/* ---- WAV files ---- */

static Uint32 le32(const Uint8 *p) { return p[0] | (p[1] << 8) | ((Uint32)p[2] << 16) | ((Uint32)p[3] << 24); }
static Uint16 le16(const Uint8 *p) { return (Uint16)(p[0] | (p[1] << 8)); }

struct wav {
	int format, channels, rate, bits, blockalign, spb;
	const Uint8 *data;
	Uint32 datalen;
};

static int parse_wav(const Uint8 *d, Uint32 size, struct wav *w)
{
	const Uint8 *fmt = NULL;
	Uint32 fmtlen = 0, pos = 12;

	memset(w, 0, sizeof(*w));
	if (size < 12 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) {
		SDL_SetError("not a WAV file");
		return 0;
	}
	while (pos + 8 <= size) {
		Uint32 len = le32(d + pos + 4);
		if (len > size - pos - 8)
			len = size - pos - 8;	/* some files claim more than they have */
		if (!memcmp(d + pos, "fmt ", 4) && len >= 16) {
			fmt = d + pos + 8;
			fmtlen = len;
		} else if (!memcmp(d + pos, "data", 4)) {
			w->data = d + pos + 8;
			w->datalen = len;
		}
		pos += 8 + len + (len & 1);
	}
	if (!fmt || !w->data) {
		SDL_SetError("WAV without fmt or data");
		return 0;
	}
	w->format = le16(fmt);
	w->channels = le16(fmt + 2);
	w->rate = (int)le32(fmt + 4);
	w->blockalign = le16(fmt + 12);
	w->bits = le16(fmt + 14);
	if (w->format == 0x11 && fmtlen >= 20)
		w->spb = le16(fmt + 18);
	if (w->channels < 1 || w->channels > 2 || w->rate <= 0) {
		SDL_SetError("WAV: only mono or stereo");
		return 0;
	}
	return 1;
}

/* PCM (8-bit unsigned or 16-bit signed little-endian, mono or stereo, any
 * rate) to 16-bit native mono at the output rate (linear interpolation) */
static Sint16 *pcm_convert(const struct wav *w, Uint32 *outframes)
{
	int step, channels = w->channels, bits = w->bits;
	Uint32 frames, n, i;
	const Uint8 *data = w->data;
	Sint16 *out;

	if (w->format != 1 || (bits != 8 && bits != 16)) {
		SDL_SetError("WAV: only 8/16-bit PCM or IMA ADPCM");
		return NULL;
	}
	step = channels * (bits / 8);
	frames = w->datalen / (Uint32)step;
	if (frames == 0) {
		SDL_SetError("WAV: empty");
		return NULL;
	}
	n = (Uint32)(((unsigned long long)frames * OUT_RATE) / (Uint32)w->rate);
	if (n == 0)
		n = 1;
	out = (Sint16 *)malloc((size_t)n * 2);
	if (!out) {
		SDL_SetError("WAV: out of memory");
		return NULL;
	}
	for (i = 0; i < n; i++) {
		Uint32 s0, s1;
		int f, c;
		Sint32 a = 0, b = 0;
		if (w->rate == OUT_RATE) {
			s0 = s1 = i;
			f = 0;
		} else {
			unsigned long long sp = ((unsigned long long)i * (Uint32)w->rate << 16) / OUT_RATE;
			s0 = (Uint32)(sp >> 16);
			s1 = s0 + 1 < frames ? s0 + 1 : s0;
			f = (int)(sp & 0xFFFF);
		}
		for (c = 0; c < channels; c++) {
			if (bits == 8) {
				a += ((int)data[s0 * step + c] - 128) << 8;
				b += ((int)data[s1 * step + c] - 128) << 8;
			} else {
				a += (Sint16)le16(data + s0 * step + c * 2);
				b += (Sint16)le16(data + s1 * step + c * 2);
			}
		}
		a /= channels;
		b /= channels;
		out[i] = (Sint16)(a + (((b - a) * f) >> 16));
	}
	*outframes = n;
	return out;
}

static Uint8 *read_file(const char *file, Uint32 *size)
{
	FILE *f = fopen(file, "rb");
	long len;
	Uint8 *d = NULL;

	if (!f) {
		SDL_SetError("cannot open %s", file);
		return NULL;
	}
	if (fseek(f, 0, SEEK_END) == 0 && (len = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0) {
		d = (Uint8 *)malloc((size_t)len);
		if (d && fread(d, 1, (size_t)len, f) != (size_t)len) {
			free(d);
			d = NULL;
		}
		*size = (Uint32)len;
	}
	fclose(f);
	if (!d)
		SDL_SetError("cannot read %s", file);
	return d;
}

Mix_Chunk *Mix_LoadWAV(const char *file)
{
	Uint32 size = 0, n = 0;
	Uint8 *d;
	struct wav w;
	Sint16 *pcm = NULL;
	Mix_Chunk *chunk;

	if (!opened || !file) {
		SDL_SetError("Mix_LoadWAV: audio not open");
		return NULL;
	}
	d = read_file(file, &size);
	if (!d)
		return NULL;
	if (parse_wav(d, size, &w))
		pcm = pcm_convert(&w, &n);
	free(d);
	if (!pcm)
		return NULL;
	chunk = (Mix_Chunk *)malloc(sizeof(*chunk));
	if (!chunk) {
		free(pcm);
		return NULL;
	}
	chunk->allocated = 1;
	chunk->abuf = (Uint8 *)pcm;
	chunk->alen = n * 2;
	chunk->volume = MIX_MAX_VOLUME;
	return chunk;
}

void Mix_FreeChunk(Mix_Chunk *chunk)
{
	int c;
	if (!chunk)
		return;
	if (opened) {
		SDL_LockAudio();
		for (c = 0; c < MAX_CHANS; c++)
			if (chans[c].chunk == chunk)
				chans[c].chunk = NULL;
		SDL_UnlockAudio();
	}
	free(chunk->abuf);
	free(chunk);
}

int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops)
{
	int c, found = -1;
	(void)loops;	/* Wizznic plays every sample once */

	if (!opened || !chunk || chunk->alen == 0)
		return -1;
	SDL_LockAudio();
	if (channel >= 0 && channel < nchans)
		found = channel;
	else
		for (c = 0; c < nchans; c++)
			if (!chans[c].chunk) {
				found = c;
				break;
			}
	if (found >= 0) {
		chans[found].chunk = chunk;
		chans[found].pos = 0;
	}
	SDL_UnlockAudio();
	if (found < 0)
		SDL_SetError("Mix_PlayChannel: no free channel");
	return found;
}

int Mix_Playing(int channel)
{
	int c, n = 0;
	if (channel >= 0)
		return channel < MAX_CHANS && chans[channel].chunk != NULL;
	for (c = 0; c < MAX_CHANS; c++)
		n += chans[c].chunk != NULL;
	return n;
}

int Mix_Volume(int channel, int volume)
{
	int c, old;
	if (channel >= MAX_CHANS)
		return 0;
	old = chans[channel < 0 ? 0 : channel].volume;
	if (volume < 0)
		return old;
	if (volume > MIX_MAX_VOLUME)
		volume = MIX_MAX_VOLUME;
	SDL_LockAudio();
	if (channel < 0)
		for (c = 0; c < MAX_CHANS; c++)
			chans[c].volume = volume;
	else
		chans[channel].volume = volume;
	SDL_UnlockAudio();
	return old;
}

int Mix_HaltChannel(int channel)
{
	int c;
	if (!opened)
		return 0;
	SDL_LockAudio();
	for (c = 0; c < MAX_CHANS; c++)
		if (channel < 0 || c == channel)
			chans[c].chunk = NULL;
	SDL_UnlockAudio();
	return 0;
}

int Mix_SetPanning(int channel, Uint8 left, Uint8 right)
{
	if (channel < 0 || channel >= MAX_CHANS)
		return 0;
	SDL_LockAudio();
	chans[channel].left = left;
	chans[channel].right = right;
	SDL_UnlockAudio();
	return 1;
}

int Mix_RegisterEffect(int chan, Mix_EffectFunc_t f, Mix_EffectDone_t d, void *arg)
{
	(void)chan;
	(void)f;
	(void)d;
	(void)arg;
	SDL_SetError("Mix_RegisterEffect: not in this mixer");
	return 0;
}

/* ---- music ---- */

Mix_Music *Mix_LoadMUS(const char *file)
{
	Uint32 size = 0;
	Uint8 *d;
	struct wav w;
	Mix_Music *m;

	if (!opened || !file) {
		SDL_SetError("Mix_LoadMUS: audio not open");
		return NULL;
	}
	d = read_file(file, &size);
	if (!d)
		return NULL;
	if (!parse_wav(d, size, &w)) {
		free(d);
		return NULL;
	}
	m = (Mix_Music *)calloc(1, sizeof(*m));
	if (!m) {
		free(d);
		return NULL;
	}
	if (w.format == 0x11) {
		if (w.channels != 1 || w.bits != 4 || w.blockalign < 5 ||
		    w.spb != (w.blockalign - 4) * 2 + 1) {
			SDL_SetError("Mix_LoadMUS: only mono IMA ADPCM");
			free(m);
			free(d);
			return NULL;
		}
		m->type = MUS_ADPCM;
		m->blockalign = (Uint32)w.blockalign;
		m->spb = (Uint32)w.spb;
		m->nblocks = w.datalen / m->blockalign;
		m->frames = m->nblocks * m->spb;
		m->step = (Uint32)(((unsigned long long)w.rate << 16) / OUT_RATE);
		m->cbuf = (Sint16 *)malloc(sizeof(Sint16) * m->spb);
		/* keep only the blocks */
		m->data = (Uint8 *)malloc((size_t)m->nblocks * m->blockalign + 1);
		if (!m->cbuf || !m->data || m->nblocks == 0) {
			free(m->cbuf);
			free(m->data);
			free(m);
			free(d);
			SDL_SetError("Mix_LoadMUS: out of memory or empty");
			return NULL;
		}
		memcpy(m->data, w.data, (size_t)m->nblocks * m->blockalign);
		m->cstart = 0xFFFFFFFFu;
	} else {
		m->type = MUS_PCM;
		m->pcm = pcm_convert(&w, &m->frames);
		m->step = 0x10000;
		if (!m->pcm) {
			free(m);
			free(d);
			return NULL;
		}
	}
	free(d);
	return m;
}

void Mix_FreeMusic(Mix_Music *m)
{
	if (!m)
		return;
	if (opened) {
		SDL_LockAudio();
		if (mus == m) {
			mus = NULL;
			mus_playing = 0;
		}
		SDL_UnlockAudio();
	}
	free(m->pcm);
	free(m->data);
	free(m->cbuf);
	free(m);
}

int Mix_FadeInMusicPos(Mix_Music *m, int loops, int ms, double position)
{
	Uint32 start = 0;

	if (!opened || !m) {
		SDL_SetError("Mix_FadeInMusicPos: no music");
		return -1;
	}
	if (position > 0) {
		/* sound.c counts the seconds a track has played, past its end
		 * when it loops */
		double f = position * (double)OUT_RATE * ((double)m->step / 65536.0);
		if (f < 4294967040.0)
			start = (Uint32)f;
		if (start >= m->frames)
			start = loops != 0 ? start % m->frames : m->frames;
	}
	SDL_LockAudio();
	mus = m;
	mus_pos = start;
	mus_frac = 0;
	mus_loops = loops;
	mus_playing = 1;
	mus_paused = 0;
	if (ms > 0) {
		fade_mode = 1;
		fade_total = fade_left = (Uint32)ms * OUT_RATE / 1000u;
	} else
		fade_mode = 0;
	SDL_UnlockAudio();
	return 0;
}

int Mix_FadeInMusic(Mix_Music *m, int loops, int ms)
{
	return Mix_FadeInMusicPos(m, loops, ms, 0.0);
}

int Mix_PlayMusic(Mix_Music *m, int loops)
{
	return Mix_FadeInMusicPos(m, loops, 0, 0.0);
}

int Mix_FadeOutMusic(int ms)
{
	int r = 0;
	if (!opened)
		return 0;
	SDL_LockAudio();
	if (mus_playing) {
		if (ms <= 0)
			mus_playing = 0;
		else {
			fade_mode = 2;
			fade_total = fade_left = (Uint32)ms * OUT_RATE / 1000u;
		}
		r = 1;
	}
	SDL_UnlockAudio();
	return r;
}

int Mix_HaltMusic(void)
{
	if (!opened)
		return 0;
	SDL_LockAudio();
	mus_playing = 0;
	fade_mode = 0;
	SDL_UnlockAudio();
	return 0;
}

void Mix_PauseMusic(void)
{
	mus_paused = 1;
}

void Mix_ResumeMusic(void)
{
	mus_paused = 0;
}

int Mix_PausedMusic(void)
{
	return mus_paused;
}

int Mix_PlayingMusic(void)
{
	return mus_playing;
}

int Mix_VolumeMusic(int volume)
{
	int old = mus_volume;
	if (volume < 0)
		return old;
	if (volume > MIX_MAX_VOLUME)
		volume = MIX_MAX_VOLUME;
	mus_volume = volume;
	return old;
}
