# Uncomment the XGST lines to use the -V option
CF=common.c cube-shadertoy.c cube-smooth.c cube-tex.c drm-atomic.c drm-common.c drm-legacy.c esTransform.c frame-512x512-NV12.c frame-512x512-RGBA.c perfcntrs.c

#CGST=-I/usr/include/gstreamer-1.0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -DHAVE_GST
#LGST=-L/usr/lib/x86_64-linux-gnu -lgstreamer-1.0 -lgstvideo-1.0 -lgstbase-1.0 -lgstallocators-1.0 -lgstapp-1.0 -lglib-2.0 -lgobject-2.0 -lgmodule-2.0 -lpthread -lrt
CFLAGS+=-I. -I/usr/include/libdrm $$(pkg-config --cflags libdrm) $(CGST)
#GSTO=cube-video.o gst-decoder.o

OBJ=$(patsubst %.c,%.o,$(CF))

kmscube: kmscube.o $(OBJ) $(GSTO)
	gcc -o $@ $^ $(LGST) -ldrm -lgbm -lEGL -lGL $$(pkg-config --libs libdrm) -lm

texturator: texturator.o $(OBJ)
	gcc -o $@ $^ -ldrm -lgbm -lEGL -lGL $$(pkg-config --libs libdrm) -lm

clean:
	-rm *.o kmscube texturator
