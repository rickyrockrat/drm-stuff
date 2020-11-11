/** \file ******************************************************************
\n\b File:        open_egl.h
\n\b Author:      Doug Springer
\n\b Company:     DNK Designs Inc.
\n\b Date:        11/10/2020  5:19 pm
\n\b Description: Common routines for opening a EGL context w/out X
*/ /************************************************************************
Change Log: \n
*/
#ifndef OPEN_EGL_H
#define OPEN_EGL_H 1


#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>



struct drm_gpu {
	int device;
	
	struct gbm_device *gbm_device;
	
	struct gbm_surface *gbm_surface;
	EGLDisplay display;
	EGLContext context;
	EGLSurface egl_surface;
	struct gbm_bo *previous_bo;
	uint32_t previous_fb;
	uint32_t connector_id;
	drmModeModeInfo mode_info;
	drmModeCrtc *crtc;	
	
};

const char *glErrorString(GLint error);
const char *eglErrorString(EGLint error);
void print_modeinfo(drmModeModeInfo  *m);
drmModeConnector *find_connector (int fd, drmModeRes *resources);
drmModeEncoder *find_encoder (int fd, drmModeRes *resources, drmModeConnector *connector);
struct drm_gpu *find_display_configuration ( char *device );
int setup_opengl (struct drm_gpu *g, EGLint *native_attr, EGLint *off_att, int render_only );
void swap_buffers (int is_master, struct drm_gpu *g);
void clean_up (struct drm_gpu *g) ;

#define STRINGIFY(x) (#x)

#define EGLCHK(n,x) do { \
	EGLint err; \
	x; \
	if(EGL_SUCCESS != (err=eglGetError())) \
				printf("%s: EGL Error %s: %d (%s)\n",n,STRINGIFY(x), err, eglErrorString(err)); \
}while(0)

#define GLCHK(n,x) do { \
	GLint err; \
	x; \
	if(GL_NO_ERROR != (err=glGetError())) \
		printf("%s: GL error %s: %d (%s)\n",n,STRINGIFY(x),err, glErrorString(err)); \
}while(0)


				

#endif
