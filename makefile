CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -pedantic \
-march=native -Os -pipe -s
#-march=native -Ofast -flto -pipe -s
#-Og -pg -g -rdynamic
LDFLAGS = #-lpng

HDR = fb2png.h stb_image_write.h
DST = fb2png

all:  $(DST)

fb2png: fb2png.c $(HDR)
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

clean:
	rm -f $(DST) *.o
