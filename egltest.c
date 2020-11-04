/*
 * Copyright (C)2015, 2019 D. R. Commander.  All Rights Reserved.
 * Copyright (C)2002 Brian Paul.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <EGL/egl.h>
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/eglext.h>
#include <GL/gl.h>


/* This function is from pbdemo.c by Brian Paul (part of the Mesa demos) */
static void WriteFile(const char *filename, int gWidth, int gHeight)
{
	FILE *f;
	GLubyte *image;
	int i;

	image = malloc(gWidth * gHeight * 3 * sizeof(GLubyte));
	if(!image)
	{
		printf("Error: couldn't allocate image buffer\n");
		return;
	}

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, gWidth, gHeight, GL_RGB, GL_UNSIGNED_BYTE, image);

	f = fopen(filename, "w");
	if(!f)
	{
		printf("Couldn't open image file: %s\n", filename);
		return;
	}
	fprintf(f,"P6\n");
	fprintf(f,"# ppm-file created by %s\n", "trdemo2");
	fprintf(f,"%i %i\n", gWidth, gHeight);
	fprintf(f,"255\n");
	fclose(f);
	f = fopen(filename, "ab");  /* now append binary data */
	if(!f)
	{
		printf("Couldn't append to image file: %s\n", filename);
		return;
	}

	for(i = 0; i < gHeight; i++)
	{
		GLubyte *rowPtr;
		/* Remember, OpenGL images are bottom to top.  Have to reverse. */
		rowPtr = image + (gHeight - 1 - i) * gWidth * 3;
		fwrite(rowPtr, 1, gWidth * 3, f);
	}

	fclose(f);
	free(image);

	printf("Wrote %d by %d image file: %s\n", gWidth, gHeight, filename);
}


static const char *eglErrorString(EGLint error)
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


#define THROW(m) {  \
	ret = -1;  \
	fprintf(stderr, "ERROR in line %d: %s\n", __LINE__, m);  \
	goto bailout;  \
}

#define THROWEGL() THROW(eglErrorString(eglGetError()))


PFNEGLQUERYDEVICESEXTPROC _eglQueryDevicesEXT = NULL;
PFNEGLQUERYDEVICESTRINGEXTPROC _eglQueryDeviceStringEXT = NULL;
PFNEGLGETPLATFORMDISPLAYEXTPROC _eglGetPlatformDisplayEXT = NULL;


int main(void)
{
	int num_devices = 0, i;
	EGLDeviceEXT *devices = NULL;
	EGLDisplay dpy = NULL;
	EGLConfig *configs = NULL;
	int attribs[] = { EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
		EGL_DEPTH_SIZE, 24, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER, EGL_NONE };
	int pbattribs[] = { EGL_WIDTH, 640, EGL_HEIGHT, 480, EGL_NONE };
	int major, minor, ret = 0, nc = 0;
	EGLSurface pb = 0;
	EGLContext ctx = 0;
	unsigned char *buf = NULL;

	if((_eglQueryDevicesEXT =
		(PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT")) == NULL)
		THROW("eglQueryDevicesEXT() could not be loaded");
	if((_eglQueryDeviceStringEXT =
		(PFNEGLQUERYDEVICESTRINGEXTPROC)eglGetProcAddress("eglQueryDeviceStringEXT")) == NULL)
		THROW("eglQueryDeviceStringEXT() could not be loaded");
	if((_eglGetPlatformDisplayEXT =
		(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT")) == NULL)
		THROW("eglGetPlatformDisplayEXT() could not be loaded");

	if(!_eglQueryDevicesEXT(0, NULL, &num_devices) || num_devices<1)
		THROWEGL();
	if((devices =
		(EGLDeviceEXT *)malloc(sizeof(EGLDeviceEXT) * num_devices)) == NULL)
		THROW("Memory allocation failure");
	if(!_eglQueryDevicesEXT(num_devices, devices, &num_devices) || num_devices<1)
		THROWEGL();

	for(i = 0; i < num_devices; i++)
	{
		const char *devstr = _eglQueryDeviceStringEXT(devices[i],
			EGL_DRM_DEVICE_FILE_EXT);
		printf("Device 0x%.8lx: %s\n", (unsigned long)devices[i],
			devstr ? devstr : "NULL");
	}

	if((dpy = _eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT,
		devices[0], NULL)) == NULL)
		THROWEGL();
	if(!eglInitialize(dpy, &major, &minor))
		THROWEGL();
	printf("EGL version %d.%d\n", major, minor);
	if(!eglChooseConfig(dpy, attribs, NULL, 0, &nc) || nc<1)
		THROWEGL();
	if((configs = (EGLConfig *)malloc(sizeof(EGLConfig) * nc)) == NULL)
		THROW("Memory allocation failure");
	if(!eglChooseConfig(dpy, attribs, configs, nc, &nc) || nc<1)
		THROWEGL();

	if((pb = eglCreatePbufferSurface(dpy, configs[0], pbattribs)) == NULL)
		THROWEGL();
	if (!eglBindAPI(EGL_OPENGL_API))
		THROWEGL();
	if((ctx = eglCreateContext(dpy, configs[0], NULL, NULL)) == NULL)
		THROWEGL();
	if(!eglMakeCurrent(dpy, pb, pb, ctx))
		THROWEGL();

	glClear(GL_COLOR_BUFFER_BIT);
	glColor3f(1.0, 1.0, 0.0);
	glRectf(-0.8, -0.8, 0.8, 0.8);

	if((buf = (unsigned char *)malloc(640 * 480 * 3)) == NULL)
		THROW("Memory allocation failure");
	WriteFile("pbuffer.ppm", 640, 480);
	if(glGetError() != GL_NO_ERROR)
		THROW("glReadPixels() failed");
	printf("Wrote image to pbuffer.ppm\n");

	bailout:
	if(buf) free(buf);
	if(ctx && dpy) eglDestroyContext(dpy, ctx);
	if(pb && dpy) eglDestroySurface(dpy, pb);
	if(dpy) eglTerminate(dpy);
	if(configs) free(configs);
	if(devices) free(devices);

	return ret;
}
