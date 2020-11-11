// gcc -o drm-gbm drm-gbm.c -ldrm -lgbm -lEGL -lGL -I/usr/include/libdrm
#include "open_egl.h"

// general documentation: man drm




/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static void swap_buffers (struct drm_gpu *g) {
	eglSwapBuffers (g->display, g->egl_surface);
	struct gbm_bo *bo = gbm_surface_lock_front_buffer (g->gbm_surface);
	uint32_t handle = gbm_bo_get_handle (bo).u32;
	uint32_t pitch = gbm_bo_get_stride (bo);
	uint32_t fb;
	if(drmModeAddFB (g->device, g->mode_info.hdisplay, g->mode_info.vdisplay, 24, 32, pitch, handle, &fb)){
		printf("drmModeAddFB() failed\n");
		return;
	}
	if(drmModeSetCrtc (g->device, g->crtc->crtc_id, fb, 0, 0, &g->connector_id, 1, &g->mode_info)){
		printf("drmModeSetCrtc() failed in proc %d @ %d\n",__LINE__, getpid());
		return;
	}
	
	if (g->previous_bo) {
		drmModeRmFB (g->device, g->previous_fb);
		gbm_surface_release_buffer (g->gbm_surface, g->previous_bo);
	}
	g->previous_bo = bo;
	g->previous_fb = fb;
}

/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static void draw (struct drm_gpu *g,int master, float progress) {
	GLenum err;
	glClearColor (1.0f-progress, progress, 0.0, 1.0);
	if((err=glGetError()) != GL_NO_ERROR)
		printf("glClearColor error %d\n",err);
	glClear (GL_COLOR_BUFFER_BIT);
	if((err=glGetError()) != GL_NO_ERROR)
                printf("glClear error %d\n",err);
	if(master)
		swap_buffers (g);
}

/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static void clean_up (struct drm_gpu *g) 
{
	// set the previous crtc
	drmModeSetCrtc (g->device, g->crtc->crtc_id, g->crtc->buffer_id, g->crtc->x, 
		g->crtc->y, &g->connector_id, 1, &g->crtc->mode);	
	drmModeFreeCrtc (g->crtc);
	
	if (g->previous_bo) {
		drmModeRmFB (g->device, g->previous_fb);
		gbm_surface_release_buffer (g->gbm_surface, g->previous_bo);
	}
	
	eglDestroySurface (g->display, g->egl_surface);
	gbm_surface_destroy (g->gbm_surface);
	eglDestroyContext (g->display, g->context);
	eglTerminate (g->display);
	gbm_device_destroy (g->gbm_device);
}

/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
int main (int argc, char *argv[]) 
{
	struct drm_gpu *g;
	int master=1;
	EGLint native_attributes[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
	EGL_NONE};

	EGLint offscreen_attributes[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
	  EGL_TEXTURE_TARGET,EGL_TEXTURE_2D,
	EGL_NONE};
	if(argc>1)
		master=strtoul(argv[1],NULL,10);
	printf("pid %d, I am %s\n",getpid(),1==master?"master":"Not master");
	
	if(NULL == (g=find_display_configuration ("/dev/dri/card0")))
		return -1;
	setup_opengl (g,native_attributes, offscreen_attributes, master);
	
	int i;
	for (i = 0; i < 600; i++)
		draw (g, master, i / 600.0f);
	
	clean_up (g);
	close (g->device);
	return 0;
}

#if pixmap_surface
void
die(const char * errstr, ...) {
    va_list ap;
    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(1);
}

int main() {
    Display * display = XOpenDisplay(NULL);
    if (!display) die("Can't create xlib display.\n");
    int screen = XDefaultScreen(display);
    GC gc = XDefaultGC(display, screen);
    Window root_window = XRootWindow(display, screen);
    unsigned long black_pixel = XBlackPixel(display, screen);
    unsigned long white_pixel = XWhitePixel(display, screen);
    Window window = XCreateSimpleWindow(display, root_window, 0, 0, 640, 480,
        0, black_pixel, white_pixel);
    if (!window) die("Can't create window.\n");
    int res = XSelectInput(display, window, ExposureMask);
    if (!res) die("XSelectInput failed.\n");
    Pixmap pixmap = XCreatePixmap(display, window, 400, 400, 24);
    if (!pixmap) die("Can't create pixmap.\n");
    EGLDisplay egldisplay = eglGetDisplay(display);
    if (EGL_NO_DISPLAY == egldisplay) die("Can't cate egl display.\n");
    res = eglInitialize(egldisplay, NULL, NULL);
    if (!res) die("eglInitialize failed.\n");
    EGLConfig config;
    int num_configs;
    static int attrib_list[] = {
        EGL_RED_SIZE,           8,
        EGL_GREEN_SIZE,         8,
        EGL_BLUE_SIZE,          8,
        EGL_ALPHA_SIZE,         0,
        EGL_RENDERABLE_TYPE,    EGL_OPENGL_BIT,
        EGL_SURFACE_TYPE,       EGL_PIXMAP_BIT,
        EGL_NONE
    };
    res = eglChooseConfig(egldisplay, attrib_list, &config, 1, &num_configs);
    if (!res) die("eglChooseConfig failed.\n");
    if (0 == num_configs) die("No appropriate egl config found.\n");
    EGLSurface surface =
        eglCreatePixmapSurface(egldisplay, config, pixmap, NULL);
    if (EGL_NO_SURFACE == surface) die("Can't create egl pixmap surface.\n");
    res = eglBindAPI(EGL_OPENGL_API);
    if (!res) die("eglBindApi failed.\n");
    EGLContext context =
        eglCreateContext(egldisplay, config, EGL_NO_CONTEXT, NULL);
    if (EGL_NO_CONTEXT == config) die("Can't create egl context.\n");
    res = eglMakeCurrent(egldisplay, surface, surface, context);
    if (!res) die("eglMakeCurrent failed.\n");
    res = XMapWindow(display, window);
    if (!res) die("XMapWindow failed.\n");
    while (1) {
        XEvent event;
        res = XNextEvent(display, &event);
        if (Expose != event.type) continue;
        glClearColor(0, 0, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glFinish();
        res = eglWaitGL();
        if (!res) die("eglWaitGL failed.\n");
        res = XCopyArea(display, pixmap, window, gc, 0, 0, 400, 400, 0, 0);
        if (!res) die("XCopyArea failed.\n");
    }
}

c
opengl
xlib
egl
#endif
