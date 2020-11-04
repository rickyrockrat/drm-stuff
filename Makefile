all: drm-gbm 

drm-gbm: drm-gbm.c
	gcc -o $@ $< -ldrm -lgbm -lEGL -lGL -I/usr/include/libdrm

te

