/** \file ******************************************************************
\n\b File:        open_egl.c
\n\b Author:      Doug Springer
\n\b Company:     DNK Designs Inc.
\n\b Date:        11/10/2020  5:26 pm
\n\b Description: File from drm-gpu, w/out globals
*/ /************************************************************************
Change Log: \n
*/

#include "open_egl.h"


const char *glErrorString(GLint error)
{
	switch(error){
		case GL_NO_ERROR           : return "Sucess";
		case GL_INVALID_ENUM       : return "Invalid Enum";
		case GL_INVALID_VALUE      : return "Invalid Value";
		case GL_INVALID_OPERATION  : return "Invalid Operation";
		case GL_STACK_OVERFLOW     : return "Stack Underflow";
		case GL_STACK_UNDERFLOW    : return "Stack Overflow";
		case GL_OUT_OF_MEMORY: return "Out of Memory";
		default: return "UNKNOWN";
	}
	return "oops";
}

const char *eglErrorString(EGLint error)
{
	switch (error)
	{
		case EGL_SUCCESS:
			return "Success";
		case EGL_NOT_INITIALIZED:
			return "EGL is not or could not be initialized";
		case EGL_BAD_ACCESS:
			return "EGL cannot access a requested resource";
		case EGL_BAD_ALLOC:
			return "EGL failed to allocate resources for the requested operation";
		case EGL_BAD_ATTRIBUTE:
			return "An unrecognized attribute or attribute value was passed in the attribute list";
		case EGL_BAD_CONTEXT:
			return "An EGLContext argument does not name a valid EGL rendering context";
		case EGL_BAD_CONFIG:
			return "An EGLConfig argument does not name a valid EGL frame buffer configuration";
		case EGL_BAD_CURRENT_SURFACE:
			return "The current surface of the calling thread is a window, pixel buffer or pixmap that is no longer valid";
		case EGL_BAD_DISPLAY:
			return "An EGLDisplay argument does not name a valid EGL display connection";
		case EGL_BAD_SURFACE:
			return "An EGLSurface argument does not name a valid surface configured for GL rendering";
		case EGL_BAD_MATCH:
			return "Arguments are inconsistent";
		case EGL_BAD_PARAMETER:
			return "One or more argument values are invalid";
		case EGL_BAD_NATIVE_PIXMAP:
			return "A NativePixmapType argument does not refer to a valid native pixmap";
		case EGL_BAD_NATIVE_WINDOW:
			return "A NativeWindowType argument does not refer to a valid native window";
		case EGL_CONTEXT_LOST:
			return "The application must destroy all contexts and reinitialise";
	}
	return "UNKNOWN EGL ERROR";
}


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
drmModeConnector *find_connector (int fd, drmModeRes *resources) 
{
	// iterate the connectors
	int i;
	for (i=0; i<resources->count_connectors; i++) {
		drmModeConnector *connector = drmModeGetConnector (fd, resources->connectors[i]);
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
drmModeEncoder *find_encoder (int fd, drmModeRes *resources, drmModeConnector *connector) 
{
	if (connector->encoder_id) {
		return drmModeGetEncoder (fd, connector->encoder_id);
	}
	// no encoder found
	return NULL;
}


/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
struct drm_gpu *find_display_configuration ( char *device ) 
{
	int i;
	drmModeRes *resources=NULL;
	drmModeConnector *connector=NULL;
	drmModeEncoder *encoder=NULL;
	
	struct drm_gpu *g=NULL;
	if(NULL == device)
		return NULL;
	if(0> (i = open (device, O_RDWR|O_CLOEXEC))){
		printf("Unable to open '%s'\n",device);
		return NULL;
	}
		
	if(NULL ==(g=calloc(1,sizeof(struct drm_gpu)))){
		printf("OOM for struct drm_gpu\n");
		close(i);
		return NULL;
	}
	g->device=i;
	if(NULL == (resources = drmModeGetResources (g->device))){
		printf("No resources found\n");
	  goto err;
	}
			
	// find a connector
	if(NULL == (connector = find_connector (g->device, resources))){
		printf("no connector found\n");	
		goto err;
	}
	
	// save the connector_id
	g->connector_id = connector->connector_id;
	/* print the modes */
	/*for (i=0; i< connector->count_modes; ++i) */
	printf("Using this mode\n");
		print_modeinfo(&connector->modes[0]);
	// save the first mode
	g->mode_info = connector->modes[0];
	printf ("resolution: %ix%i\n", g->mode_info.hdisplay, g->mode_info.vdisplay);
	// find an encoder
	if(NULL == (encoder = find_encoder (g->device, resources, connector))){
		printf("no encoder found\n");	
		goto err;
	}
	
	// find a CRTC
	if (encoder->crtc_id) {
		g->crtc = drmModeGetCrtc (g->device, encoder->crtc_id);
	}else{
		printf("No CRTC found\n");
		goto err;
	}
	goto clean;
	
err:
	if(NULL != g){
		if(0< g->device)
			close(g->device);
		free(g);
		g=NULL;
	}
	
clean:
	drmModeFreeEncoder (encoder);
	drmModeFreeConnector (connector);
	drmModeFreeResources (resources);
	return g;
}



/***************************************************************************/


/** .
Attrib example:
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
\n\b Arguments:
\n\b Returns:
****************************************************************************/
int setup_opengl (struct drm_gpu *g, EGLint *native_attr, EGLint *off_att, int render_only ) 
{
	EGLConfig config;
	EGLint num_config;
	uint32_t format=0!=render_only?0:GBM_BO_USE_SCANOUT;
	if(NULL ==g){
		printf("%s: NULL struct \n",__FUNCTION__);
		return -1;
	}
	if(NULL ==(g->gbm_device = gbm_create_device (g->device))){
		printf("gbm_create_device failed\n");
		return -1;
	}
	if(NULL ==(g->display = eglGetDisplay (g->gbm_device))){
		printf("eglGetDisplay Failed\n");
		return -1;
	}
	EGLCHK(__FUNCTION__,eglInitialize (g->display, NULL, NULL));
	
	// create an OpenGL context
	EGLCHK(__FUNCTION__,eglBindAPI (EGL_OPENGL_API));

	
	EGLCHK(__FUNCTION__,eglChooseConfig (g->display, native_attr, &config, 1, &num_config));
	if(NULL ==(g->context = eglCreateContext (g->display, config, EGL_NO_CONTEXT, NULL))){
		printf("eglCreateContext Failed\n");
		return -1;
	}
	
	// create the GBM and EGL surface
	if(NULL ==(g->gbm_surface = gbm_surface_create (g->gbm_device, g->mode_info.hdisplay, 
		g->mode_info.vdisplay, GBM_BO_FORMAT_XRGB8888, format|GBM_BO_USE_RENDERING))) {
			printf("gbm_surface_create failed\n");
			return -1;																																						
	}
	
	if(NULL ==(g->egl_surface = eglCreateWindowSurface (g->display, config, g->gbm_surface, NULL))){
		printf("eglCreateWindowSurface Failed\n");
		return -1;
	}
	EGLCHK(__FUNCTION__,eglMakeCurrent (g->display, g->egl_surface, g->egl_surface, g->context));
	return 0;
}

/***************************************************************************/
/** .
\n\b Arguments:
\n\b Returns:
****************************************************************************/
void swap_buffers (int is_master, struct drm_gpu *g) 
{
	struct gbm_bo *bo;
	uint32_t handle;
	uint32_t pitch;
	uint32_t fb;   
	eglSwapBuffers (g->display, g->egl_surface);
	if(0 == is_master)
		return;
	bo = gbm_surface_lock_front_buffer (g->gbm_surface);
	handle = gbm_bo_get_handle (bo).u32;
	pitch = gbm_bo_get_stride (bo);
	if(is_master){
		if(drmModeAddFB (g->device, g->mode_info.hdisplay, g->mode_info.vdisplay, 24, 32, pitch, handle, &fb)){
			printf("drmModeAddFB() failed\n");
			return;
		}
		if(drmModeSetCrtc (g->device, g->crtc->crtc_id, fb, 0, 0, &g->connector_id, 1, &g->mode_info)){
			printf("drmModeSetCrtc() failed in proc %d @ %d\n",__LINE__, getpid());
			return;
		}	
	}
	
	
	if (g->previous_bo) {
		if(is_master)
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
void clean_up (struct drm_gpu *g) 
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


