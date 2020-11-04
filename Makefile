all: drm-gbm egltest

CC?=gcc

drm-gbm: drm-gbm.c
	$(CC) -o $@ $< -ldrm -lgbm -lEGL -lGL -I/usr/include/libdrm
	
egltest: egltest.c
	$(CC) -O3 -Wall -Werror -I. -o $@ $^ -lOpenGL -lEGL


