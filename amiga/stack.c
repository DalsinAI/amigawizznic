/* Wizznic recurses a little (lists, menus) and keeps 2 KB line buffers;
 * a Shell gives 4 KB. libnix's startup sets this much up instead.
 * The AmiCradle Team. GPL 3 or later, like Wizznic. */
unsigned long __stack = 256 * 1024;
static const char version[] __attribute__((used)) = "$VER: Wizznic 1.1 Amiga (05.11.2026)";
