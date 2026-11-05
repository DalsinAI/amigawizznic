/*
 * Wizznic joins its paths the Unix way: a base plus "/" plus a name
 * ("PROGDIR:" "/" "settings.ini", "PROGDIR:packs/000_wizznic" "/" "./"
 * "/" "finished.png"). On AmigaDOS "PROGDIR:/x" is the drawer above
 * PROGDIR:, "a//b" is a's parent, and "." is no name at all.
 *
 * The Makefile links the program with --wrap for each call below, so that
 * every path upstream's code hands to libnix first goes through
 * amiga_path(), which makes an AmigaDOS name of it:
 *   - a volume or assign ("PROGDIR:", "Work:") keeps its colon, and no "/"
 *     follows it; anything written before the volume is dropped;
 *   - "." and empty parts go, "x/.." goes;
 *   - a ".." that climbs above the start (no volume) becomes AmigaDOS's
 *     leading "/" (parent); above a volume it is dropped;
 *   - a "/" at the very start (no volume) stays the parent, so that a
 *     name made here comes out the same when it is put through again
 *     (libnix's fopen() calls open(), both wrapped).
 * "" is the current drawer, as AmigaDOS has it.
 *
 * (From the Commander Genius and OpenLieroX ports.)
 * The AmiCradle Team. GPL 3 or later, like Wizznic.
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <SDL.h>
#include <exec/semaphores.h>
#include <proto/exec.h>

/* libnix's stdio and file-descriptor tables have no lock of their own;
 * Wizznic opens files on its main task only, but the lock costs nothing
 * and keeps SDL's own tasks safe. Opening and closing files and drawers
 * goes through one Exec semaphore (a task may take it again). */
static struct SignalSemaphore io_sema;

__attribute__((constructor)) static void io_sema_init(void)
{
	InitSemaphore(&io_sema);
}

#define IO_LOCK() ObtainSemaphore(&io_sema)
#define IO_UNLOCK() ReleaseSemaphore(&io_sema)

#define AP_MAX 1024
#define AP_PARTS 64

const char *amiga_path(const char *in, char *out, size_t size)
{
	const char *colon, *p, *start[AP_PARTS];
	size_t len[AP_PARTS], n = 0, o = 0, vol = 0, i;
	int parents = 0;

	if (!in || size == 0)
		return in;

	colon = strchr(in, ':');
	if (colon) {
		/* "./PROGDIR:x" or "/PROGDIR:x": the volume starts the name */
		const char *b = colon;
		while (b > in && b[-1] != '/' && b[-1] != '\\')
			b--;
		vol = (size_t)(colon - b) + 1;
		if (vol >= size)
			return in;
		memcpy(out, b, vol);
		o = vol;
		p = colon + 1;
	} else {
		p = in;
		/* leading slashes: AmigaDOS's parent drawer */
		while (*p == '/') {
			parents++;
			p++;
		}
	}

	while (*p) {
		const char *s;
		size_t l;
		while (*p == '/' || *p == '\\')
			p++;
		if (!*p)
			break;
		s = p;
		while (*p && *p != '/' && *p != '\\')
			p++;
		l = (size_t)(p - s);
		if (l == 1 && s[0] == '.')
			continue;
		if (l == 2 && s[0] == '.' && s[1] == '.') {
			if (n > 0)
				n--;
			else if (!vol)
				parents++;
			continue;
		}
		if (n == AP_PARTS)
			return in;
		start[n] = s;
		len[n] = l;
		n++;
	}

	for (; parents > 0; parents--) {
		if (o + 1 >= size)
			return in;
		out[o++] = '/';
	}
	for (i = 0; i < n; i++) {
		if (o + len[i] + 2 >= size)
			return in;
		memcpy(out + o, start[i], len[i]);
		o += len[i];
		if (i + 1 < n)
			out[o++] = '/';
	}
	out[o] = '\0';
	return out;
}

#define AP(name) char name##_buf[AP_MAX]; const char *name##_a = amiga_path(name, name##_buf, sizeof(name##_buf))

FILE *__real_fopen(const char *, const char *);
FILE *__wrap_fopen(const char *name, const char *mode)
{
	FILE *f;
	AP(name);
	IO_LOCK();
	f = __real_fopen(name_a, mode);
	IO_UNLOCK();
	return f;
}

int __real_fclose(FILE *);
int __wrap_fclose(FILE *f)
{
	int r;
	IO_LOCK();
	r = __real_fclose(f);
	IO_UNLOCK();
	return r;
}

FILE *__real_fdopen(int, const char *);
FILE *__wrap_fdopen(int fd, const char *mode)
{
	FILE *f;
	IO_LOCK();
	f = __real_fdopen(fd, mode);
	IO_UNLOCK();
	return f;
}

int __real_close(int);
int __wrap_close(int fd)
{
	int r;
	IO_LOCK();
	r = __real_close(fd);
	IO_UNLOCK();
	return r;
}

int __real_closedir(DIR *);
int __wrap_closedir(DIR *d)
{
	int r;
	IO_LOCK();
	r = __real_closedir(d);
	IO_UNLOCK();
	return r;
}

FILE *__real_freopen(const char *, const char *, FILE *);
FILE *__wrap_freopen(const char *name, const char *mode, FILE *f)
{
	FILE *r;
	if (!name) {
		IO_LOCK();
		r = __real_freopen(name, mode, f);
		IO_UNLOCK();
		return r;
	}
	{
		AP(name);
		IO_LOCK();
		r = __real_freopen(name_a, mode, f);
		IO_UNLOCK();
		return r;
	}
}

int __real_open(const char *, int, ...);
int __wrap_open(const char *name, int flags, ...)
{
	va_list ap;
	int mode;
	AP(name);
	va_start(ap, flags);
	mode = va_arg(ap, int);
	va_end(ap);
	IO_LOCK();
	mode = __real_open(name_a, flags, mode);
	IO_UNLOCK();
	return mode;
}

int __real_stat(const char *, struct stat *);
int __wrap_stat(const char *name, struct stat *st)
{
	AP(name);
	return __real_stat(name_a, st);
}

int __real_lstat(const char *, struct stat *);
int __wrap_lstat(const char *name, struct stat *st)
{
	AP(name);
	return __real_lstat(name_a, st);
}

DIR *__real_opendir(const char *);
DIR *__wrap_opendir(const char *name)
{
	DIR *d;
	AP(name);
	IO_LOCK();
	d = __real_opendir(name_a);
	IO_UNLOCK();
	return d;
}

int __real_mkdir(const char *, mode_t);
int __wrap_mkdir(const char *name, mode_t mode)
{
	AP(name);
	/* "" (the current drawer) or a bare volume is there already */
	if (!name_a[0] || name_a[strlen(name_a) - 1] == ':')
		return -1;
	return __real_mkdir(name_a, mode);
}

int __real_rmdir(const char *);
int __wrap_rmdir(const char *name)
{
	AP(name);
	return __real_rmdir(name_a);
}

int __real_remove(const char *);
int __wrap_remove(const char *name)
{
	AP(name);
	return __real_remove(name_a);
}

int __real_unlink(const char *);
int __wrap_unlink(const char *name)
{
	AP(name);
	return __real_unlink(name_a);
}

int __real_rename(const char *, const char *);
int __wrap_rename(const char *from, const char *to)
{
	AP(from);
	AP(to);
	return __real_rename(from_a, to_a);
}

int __real_chdir(const char *);
int __wrap_chdir(const char *name)
{
	AP(name);
	return __real_chdir(name_a);
}

int __real_access(const char *, int);
int __wrap_access(const char *name, int mode)
{
	AP(name);
	return __real_access(name_a, mode);
}

SDL_RWops *__real_SDL_RWFromFile(const char *, const char *);
SDL_RWops *__wrap_SDL_RWFromFile(const char *name, const char *mode)
{
	AP(name);
	return __real_SDL_RWFromFile(name_a, mode);
}
