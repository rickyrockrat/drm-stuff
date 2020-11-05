// gcc -o drm-gbm drm-gbm.c -ldrm -lgbm -lEGL -lGL -I/usr/include/libdrm

// general documentation: man drm

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#define EXIT(msg) { fputs (msg, stderr); exit (EXIT_FAILURE); }

static int device;

static struct gbm_device *gbm_device;
static EGLDisplay display;
static EGLContext context;
static struct gbm_surface *gbm_surface;
static EGLSurface egl_surface;
static struct gbm_bo *previous_bo = NULL;
static uint32_t previous_fb;
static uint32_t connector_id;
static drmModeModeInfo mode_info;
static drmModeCrtc *crtc;

void print_modeinfo(drmModeModeInfo  *m)
{
	printf("%s: clock %d hdisplay %d vdisplay %d vrefresh %d\n",
		m->name,m->clock,m->hdisplay, m->vdisplay,m->vrefresh);
	printf("   hsyncstart %d hsyncend %d htotal %d hskew %d\n",
		m->hsync_start, m->hsync_end, m->htotal, m->hskew);
	printf("   vsyncstart %d, vsyncend %d, vtotal %d, vscan %d\n",
		m->vsync_start, m->vsync_end, m->vtotal, m->vscan);
}

/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static drmModeConnector *find_connector (drmModeRes *resources) 
{
	// iterate the connectors
	int i;
	for (i=0; i<resources->count_connectors; i++) {
		drmModeConnector *connector = drmModeGetConnector (device, resources->connectors[i]);
		// pick the first connected connector
		if (connector->connection == DRM_MODE_CONNECTED) {
			return connector;
		}
		drmModeFreeConnector (connector);
	}
	// no connector found
	return NULL;
}

/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static drmModeEncoder *find_encoder (drmModeRes *resources, drmModeConnector *connector) 
{
	if (connector->encoder_id) {
		return drmModeGetEncoder (device, connector->encoder_id);
	}
	// no encoder found
	return NULL;
}


/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static void find_display_configuration () 
{
	int i;
	drmModeRes *resources = drmModeGetResources (device);
	// find a connector
	drmModeConnector *connector = find_connector (resources);
	if (!connector) EXIT ("no connector found\n");
	// save the connector_id
	connector_id = connector->connector_id;
	/* print the modes */
	for (i=0; i< connector->count_modes; ++i)
		print_modeinfo(&connector->modes[i]);
	// save the first mode
	mode_info = connector->modes[0];
	printf ("resolution: %ix%i\n", mode_info.hdisplay, mode_info.vdisplay);
	// find an encoder
	drmModeEncoder *encoder = find_encoder (resources, connector);
	if (!encoder) EXIT ("no encoder found\n");
	// find a CRTC
	if (encoder->crtc_id) {
		crtc = drmModeGetCrtc (device, encoder->crtc_id);
	}
	drmModeFreeEncoder (encoder);
	drmModeFreeConnector (connector);
	drmModeFreeResources (resources);
}



/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static void setup_opengl (int render_only) 
{
	EGLConfig config;
	EGLint num_config;
	gbm_device = gbm_create_device (device);
	display = eglGetDisplay (gbm_device);
	eglInitialize (display, NULL, NULL);
	
	// create an OpenGL context
	eglBindAPI (EGL_OPENGL_API);
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
	
	eglChooseConfig (display, native_attributes, &config, 1, &num_config);
	context = eglCreateContext (display, config, EGL_NO_CONTEXT, NULL);
	
	// create the GBM and EGL surface
	gbm_surface = gbm_surface_create (gbm_device, mode_info.hdisplay, mode_info.vdisplay, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT|GBM_BO_USE_RENDERING);
	egl_surface = eglCreateWindowSurface (display, config, gbm_surface, NULL);
	eglMakeCurrent (display, egl_surface, egl_surface, context);
}


/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static void swap_buffers () {
	eglSwapBuffers (display, egl_surface);
	struct gbm_bo *bo = gbm_surface_lock_front_buffer (gbm_surface);
	uint32_t handle = gbm_bo_get_handle (bo).u32;
	uint32_t pitch = gbm_bo_get_stride (bo);
	uint32_t fb;
	if(drmModeAddFB (device, mode_info.hdisplay, mode_info.vdisplay, 24, 32, pitch, handle, &fb)){
		printf("drmModeAddFB() failed\n");
		return;
	}
	if(drmModeSetCrtc (device, crtc->crtc_id, fb, 0, 0, &connector_id, 1, &mode_info)){
		printf("drmModeSetCrtc() failed in proc %d @ %d\n",__LINE__, getpid());
		return;
	}
	
	if (previous_bo) {
		drmModeRmFB (device, previous_fb);
		gbm_surface_release_buffer (gbm_surface, previous_bo);
	}
	previous_bo = bo;
	previous_fb = fb;
}

/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static void draw (int master, float progress) {
	GLenum err;
	glClearColor (1.0f-progress, progress, 0.0, 1.0);
	if((err=glGetError()) != GL_NO_ERROR)
		printf("glClearColor error %d\n",err);
	glClear (GL_COLOR_BUFFER_BIT);
	if((err=glGetError()) != GL_NO_ERROR)
                printf("glClear error %d\n",err);
	if(master)
		swap_buffers ();
}

/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
static void clean_up () {
	// set the previous crtc
	drmModeSetCrtc (device, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y, &connector_id, 1, &crtc->mode);
	drmModeFreeCrtc (crtc);
	
	if (previous_bo) {
		drmModeRmFB (device, previous_fb);
		gbm_surface_release_buffer (gbm_surface, previous_bo);
	}
	
	eglDestroySurface (display, egl_surface);
	gbm_surface_destroy (gbm_surface);
	eglDestroyContext (display, context);
	eglTerminate (display);
	gbm_device_destroy (gbm_device);
}

/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
int main (int argc, char *argv[]) {
	int master=1;
	if(argc>1)
		master=strtoul(argv[1],NULL,10);
	printf("pid %d, I am %s\n",getpid(),1==master?"master":"Not master");
	device = open ("/dev/dri/card0", O_RDWR|O_CLOEXEC);
	find_display_configuration ();
	setup_opengl (master);
	
	int i;
	for (i = 0; i < 600; i++)
		draw (master, i / 600.0f);
	
	clean_up ();
	close (device);
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
