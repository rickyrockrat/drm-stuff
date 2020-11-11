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
	for (i=0; i< connector->count_modes; ++i)
		print_modeinfo(&connector->modes[i]);
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
	uint32_t format=render_only?0:GBM_BO_USE_SCANOUT;
	g->gbm_device = gbm_create_device (g->device);
	g->display = eglGetDisplay (g->gbm_device);
	eglInitialize (g->display, NULL, NULL);
	
	// create an OpenGL context
	eglBindAPI (EGL_OPENGL_API);

	
	eglChooseConfig (g->display, native_attr, &config, 1, &num_config);
	g->context = eglCreateContext (g->display, config, EGL_NO_CONTEXT, NULL);
	
	// create the GBM and EGL surface
	g->gbm_surface = gbm_surface_create (g->gbm_device, g->mode_info.hdisplay, 
		g->mode_info.vdisplay, GBM_BO_FORMAT_XRGB8888, format|GBM_BO_USE_RENDERING);
	g->egl_surface = eglCreateWindowSurface (g->display, config, g->gbm_surface, NULL);
	eglMakeCurrent (g->display, g->egl_surface, g->egl_surface, g->context);
	return 0;
}


