TARGS=drm-gbm egltest drm_test drm-prime-dumb-kms modeset-double-buffered
all: 	$(TARGS)


CC?=gcc

%.o:%.c
	$(CC) -Wall -c -o $@ $< $$(pkg-config --cflags libdrm)

drm-gbm: drm-gbm.o open_egl.o
	$(CC) -o $@ $^ -ldrm -lgbm -lEGL -lGL -I/usr/include/libdrm
	
egltest: egltest.c
	$(CC) -O3 -Wall -Werror -I. -o $@ $^ -lOpenGL -lEGL

drm_test: drm_test.c
	$(CC) -Wall $< -o $@ $$(pkg-config --cflags --libs libdrm)
	
drm-prime-dumb-kms: drm-prime-dumb-kms.c
	$(CC) -O3 -Wall -Werror -I. -o $@ $^ $$(pkg-config --cflags --libs libdrm)
	
modeset-double-buffered: modeset-double-buffered.c
	$(CC) -O3 -Wall -Werror -I. -o $@ $^ $$(pkg-config --cflags --libs libdrm)
	

clean:
	-rm *.o $(TARGS)

