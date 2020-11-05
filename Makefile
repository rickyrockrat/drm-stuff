all: drm-gbm egltest drm_test

CC?=gcc

drm-gbm: drm-gbm.c
	$(CC) -o $@ $< -ldrm -lgbm -lEGL -lGL -I/usr/include/libdrm
	
egltest: egltest.c
	$(CC) -O3 -Wall -Werror -I. -o $@ $^ -lOpenGL -lEGL

drm_test: drm_test.c
	$(CC) -Wall $< -o $@ $$(pkg-config --cflags --libs libdrm)
