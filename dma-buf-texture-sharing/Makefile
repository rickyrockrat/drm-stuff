all: dmabufshare


../open_egl.o: ../open_egl.c
	$(MAKE) -C ../

dmabufshare:../open_egl.o dmabufshare.c socket.h 
	$(CC) -o $@ dmabufshare.c $< -g -lEGL -lGL -lgbm -I../ -o dmabufshare $$(pkg-config --libs --cflags libdrm)

clean:
	rm -f dmabufshare
