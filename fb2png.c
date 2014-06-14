/* See LICENSE for licence details. */
#include "fb2png.h"

/* error functions */
void error(char *str)
{
	perror(str);
	exit(EXIT_FAILURE);
}

void fatal(char *str)
{
	fprintf(stderr, "%s\n", str);
	exit(EXIT_FAILURE);
}

/* wrapper of C functions */
int eopen(const char *path, int flag)
{
	int fd;
	errno = 0;

	if ((fd = open(path, flag)) < 0) {
		fprintf(stderr, "cannot open \"%s\"\n", path);
		error("open");
	}

	return fd;
}

void eclose(int fd)
{
	errno = 0;

	if (close(fd) < 0)
		error("close");
}

void *emmap(void *addr, size_t len, int prot, int flag, int fd, off_t offset)
{
	uint32_t *fp;
	errno = 0;

	if ((fp = (uint32_t *) mmap(addr, len, prot, flag, fd, offset)) == MAP_FAILED)
		error("mmap");

	return fp;
}

void emunmap(void *ptr, size_t len)
{
	errno = 0;

	if (munmap(ptr, len) < 0)
		error("munmap");
}

void *ecalloc(size_t nmemb, size_t size)
{
	void *ptr;
	errno = 0;

	if ((ptr = calloc(nmemb, size)) == NULL)
		error("calloc");

	return ptr;
}

/* some functions for Linux framebuffer */
inline int my_ceil(int val, int div)
{
	return (val + div - 1) / div;
}

void cmap_create(struct fb_cmap **cmap)
{
	*cmap           = (struct fb_cmap *) ecalloc(1, sizeof(struct fb_cmap));
	(*cmap)->start  = 0;
	(*cmap)->len    = COLORS;
	(*cmap)->red    = (uint16_t *) ecalloc(COLORS, sizeof(uint16_t));
	(*cmap)->green  = (uint16_t *) ecalloc(COLORS, sizeof(uint16_t));
	(*cmap)->blue   = (uint16_t *) ecalloc(COLORS, sizeof(uint16_t));
	(*cmap)->transp = NULL;
}

void cmap_die(struct fb_cmap *cmap)
{
	if (cmap) {
		free(cmap->red);
		free(cmap->green);
		free(cmap->blue);
		free(cmap->transp);
		free(cmap);
	}
}

void fb_init(struct framebuffer *fb)
{
	char *path;
	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;

	if ((path = getenv("FRAMEBUFFER")) != NULL)
		fb->fd = eopen(path, O_RDWR);
	else
		fb->fd = eopen(fb_path, O_RDWR);

	if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &finfo) < 0)
		fatal("ioctl: FBIOGET_FSCREENINFO failed");

	if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &vinfo) < 0)
		fatal("ioctl: FBIOGET_VSCREENINFO failed");

	/* check screen offset and initialize because linux console change this */
	/*
	if (vinfo.xoffset != 0 || vinfo.yoffset != 0) {
		vinfo.xoffset = vinfo.yoffset = 0;
		ioctl(fb->fd, FBIOPUT_VSCREENINFO, &vinfo);
	}
	*/

	fb->width       = vinfo.xres;
	fb->height      = vinfo.yres;
	fb->screen_size = finfo.smem_len;
	fb->line_length = finfo.line_length;

	if ((finfo.visual == FB_VISUAL_TRUECOLOR || finfo.visual == FB_VISUAL_DIRECTCOLOR)
		&& (vinfo.bits_per_pixel == 15 || vinfo.bits_per_pixel == 16
		|| vinfo.bits_per_pixel == 24 || vinfo.bits_per_pixel == 32)) {
		fb->cmap = fb->cmap_org = NULL;
		fb->bpp = my_ceil(vinfo.bits_per_pixel, BITS_PER_BYTE);
	}
	else if (finfo.visual == FB_VISUAL_PSEUDOCOLOR && vinfo.bits_per_pixel == 8) {
		cmap_create(&fb->cmap_org);
		if (ioctl(fb->fd, FBIOGETCMAP, fb->cmap_org) < 0)
			fatal("ioctl: FBIOGETCMAP failed");
		fb->cmap = NULL;
		fb->bpp  = 1;
	}
	else /* non packed pixel, mono color, grayscale: not implimented */
		fatal("unsupported framebuffer type");

	fb->fp    = (unsigned char *) emmap(0, fb->screen_size, PROT_WRITE | PROT_READ, MAP_SHARED, fb->fd, 0);
	fb->buf   = (unsigned char *) ecalloc(1, fb->screen_size);
	fb->vinfo = vinfo;
}

void fb_die(struct framebuffer *fb)
{
	cmap_die(fb->cmap);
	cmap_die(fb->cmap_org);
	free(fb->buf);
	emunmap(fb->fp, fb->screen_size);
	eclose(fb->fd);
}

/* fb2png function */
void usage()
{
	printf("usage: fb2png FILE\n");
}

inline void color2rgb(struct framebuffer *fb,
	uint32_t color, uint8_t *r, uint8_t *g, uint8_t *b)
{
	if (fb->bpp == 1 && fb->cmap_org != NULL) {
		*r  = *(fb->cmap_org->red   + fb->cmap_org->start + color) >> 8;
		*g  = *(fb->cmap_org->green + fb->cmap_org->start + color) >> 8;
		*b  = *(fb->cmap_org->blue  + fb->cmap_org->start + color) >> 8;
		//printf("color:%d r:%d g:%d b:%d\n", color, *r, *g, *b);
		return;
	}

	*r = bit_mask[fb->vinfo.red.length]   & (color >> fb->vinfo.red.offset);
	*g = bit_mask[fb->vinfo.green.length] & (color >> fb->vinfo.green.offset);
	*b = bit_mask[fb->vinfo.blue.length]  & (color >> fb->vinfo.blue.offset);

	*r = bit_mask[8] * *r / bit_mask[fb->vinfo.red.length];
	*g = bit_mask[8] * *g / bit_mask[fb->vinfo.green.length];
	*b = bit_mask[8] * *b / bit_mask[fb->vinfo.blue.length];
}

int main(int argc, char *argv[])
{
	int w, h;
	long offset;
	uint32_t color = 0;
	uint8_t r, g, b;
	unsigned char *buf, *ptr;
	struct framebuffer fb;

	if (argc < 2) {
		usage();
		return EXIT_FAILURE;
	}

	fb_init(&fb);

	//memcpy(fb.buf, fb.fp, fb.screen_size);
	buf = ecalloc(3, fb.width * fb.height);

	for (h = 0; h < fb.height; h++) {
		for (w = 0; w < fb.width; w++) {
			ptr = fb.fp + h * fb.line_length + w * fb.bpp;
			memcpy(&color, ptr, fb.bpp);
			color2rgb(&fb, color, &r, &g, &b);

			offset = 3 * (h * fb.width + w);
			*(buf + offset + 0) = r;
			*(buf + offset + 1) = g;
			*(buf + offset + 2) = b;
		}
	}
	stbi_write_png(argv[1], fb.width, fb.height, 3, buf, 3 * fb.width);

	free(buf);
	fb_die(&fb);

	return EXIT_SUCCESS;
}
