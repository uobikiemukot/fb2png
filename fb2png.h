/* See LICENSE for licence details. */
#define _XOPEN_SOURCE 600
//#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/fb.h>
#include <linux/vt.h>
#include <linux/kd.h>
//#include <limits.h>
//#include <png.h>
//#include <sys/stat.h>
//#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

const char *fb_path = "/dev/fb0";

enum {
	DEBUG         = false,
	COLORS        = 256,
	BITS_PER_BYTE = 8,
};

const uint32_t bit_mask[] = {
	0x00,
	0x01,       0x03,       0x07,       0x0F,
	0x1F,       0x3F,       0x7F,       0xFF,
	0x1FF,      0x3FF,      0x7FF,      0xFFF,
	0x1FFF,     0x3FFF,     0x7FFF,     0xFFFF,
	0x1FFFF,    0x3FFFF,    0x7FFFF,    0xFFFFF,
	0x1FFFFF,   0x3FFFFF,   0x7FFFFF,   0xFFFFFF,
	0x1FFFFFF,  0x3FFFFFF,  0x7FFFFFF,  0xFFFFFFF,
	0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF,
};

struct framebuffer {
	unsigned char *fp;               /* pointer of framebuffer (read only) */
	unsigned char *buf;              /* copy of framebuffer */
	unsigned char *wall;             /* buffer for wallpaper */
	int fd;                          /* file descriptor of framebuffer */
	int width, height;               /* display resolution */
	long screen_size;                /* screen data size (byte) */
	int line_length;                 /* line length (byte) */
	int bpp;                         /* BYTES per pixel */
	struct fb_cmap *cmap, *cmap_org; /* cmap for legacy framebuffer (8bpp pseudocolor) */
	struct fb_var_screeninfo vinfo;
};
