/*

AUTHOR : unixman, unixman at archlinux dot info
LICENSE: public domain
BUILD  : gcc -I/usr/include/libdrm file.c -lgbm -lEGL -lGLESv2
RUN    : this program can run as ordinary user. I tested it on arch linux. works in the console session
         don't work in weston session. I put error log to end of this file.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <drm.h>
#include <gbm.h>
#include <EGL/egl.h>
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/eglext.h>
#include <GLES3/gl32.h>

/*
 Shaders code. Dump. Contains almost nothing.
*/
static const char* fragment = "\
#version 320 es                              \n\
precision mediump float;                \n\
out vec4 col;		  		         \n\
void main(){                                    \n\
	col = vec4(0, 0, 0, 1);		 \n\
}\n";

static const char* vertex ="\
#version 320 es                                       \n\
layout(location = 0) in vec4 coord;         \n\
void main(){                                            \n\
	gl_Position = coord;       		         \n\
}\n";

static const EGLint cfg[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_BUFFER_SIZE, 32,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
	EGL_NONE
};

static const EGLint ctx[] = {
	EGL_CONTEXT_MAJOR_VERSION, 3,
	EGL_CONTEXT_MINOR_VERSION, 2,
	EGL_NONE
};

struct drm{
	int fd;
	unsigned int conn_id;
	unsigned int crtc_id;
	unsigned int plane_id;
	unsigned int enc_id;
	struct drm_mode_modeinfo mode;
	struct gbm_device* gbm;
};

/*
 This is just a helper func to get some graphics infos/resources from underlaying stack which is needed. eg crtc_id, screen resolution.
 I use gbm interface because of simplify things, shorten LOC, minimize dependency.
*/
extern void getres(void*);

/*
 A global buffer variable that used to hold the data of the GL_ARRAY_BUFFER.
 Its size is hardcoded as 8192. No strong reason that why it is hardcoded and why 8192.
 So the text file which will rendering to screen cannot contains more than a few hundred ascii characters.
 Because one character needs minumum 4*sizeof(float) or 16, maximum 36*sizeof(float) or 144 byte memory, assumed sizeof(float)=4.
 Also because current approach load the whole file content to buffer single turn and draw it to screen in single OpenGL drawing command.
 'malloc/realloc' may works properly here instead of it, but needs figure out how is it, may be move global variable as local variable,
 set buffer size dynamically according to size of text/file which want to rendering to screen and tweaks functions which use it.
 then free buffer when no more needed. I dont think cons/pros of it versus each other. So lets left it as is as long as this is just a toy program.
*/
static float buff[8192] = {0};

/*
 A global variable using for store current position in buffer when transferring data to buffer.
*/
static int p = 0;

/*
 A global variable store ordered start indices of the corresponding ASCII characters matches the 'coords' array below.
 Two coordinates in the 'coords' count as one element as GL_POINTS consist of two coordinates (x, y).
 This is just an implementation detail I choice.
 This is used as lookup table and numeric ascii value of character minus 33 used as input variable.(eg 32 for A)
 Because first 32 non printable chars skipped for convionency.
*/
static int indices[] = { 0, 4, 8, 16, 28, 46, 54, 56, 62, 68, 74, 78, 80, 82, 90, 92, 102, 108, 118, 126, 132, 142, 150, 156, 166, 174, 190,
                         200, 204, 208, 212, 230, 244, 250/*A*/, 262, 268, 280, 288, 294, 304, 310, 316, 322, 328, 332,340, 346, 354, 362, 372,
                         382, 392, 396, 402, 406, 414, 418, 422, 430/*Z*/, 436, 438, 444, 448, 450, 452, 462, 470, 476, 484, 492, 498, 508, 514,
                         518, 524, 530, 536, 546, 552, 560, 568, 576, 580, 590, 594, 600,604, 612, 616, 620, 626, 638, 640, 652, 658 };

/*
 Nontransformed and unscaled GL coordinates of GL_LINES as (x1, y1, x2, y2) that defines the shape of the corresponding printable ASCII character.
 It is ordered according to universal ASCII table. The 'baseline' is assumed y=0 here. For meaning of 'baseline' see 'freetype' docs.
 The most right point of the character assumed x=0 here too. All that is just an implementation detail as I can try to simplyfied things.
 Also shaping of the characters simplyfied as it is consist of only straight lines, there is no curve. But i think it is still readable/figurable, isn't it:)
*/
static float coords[] = {
		0.0, 2.0, 0.0, 0.5, 0.0, 0.2, 0.0, 0.0,                                                        /*  ASCII char !  */
		0.0, 2.0, 0.0, 1.6, 0.4, 2.0, 0.4, 1.6,                                                        /*  ASCII char "  (double quote)  */
		0.3, 2.0, 0.3, 0.0, 0.8, 2.0, 0.8, 0.0, 0.0, 1.2, 1.0, 1.2, 0.0, 0.8, 1.0, 0.8,
		1.0, 1.8, 0.0, 1.8, 0.0, 1.8, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.2, 1.0, 0.2, 0.0, 0.2, 0.5, 2.0, 0.5, 0.0,
		0.0, 0.0, 1.0, 2.0, 0.0, 2.0, 0.2, 2.0, 0.2, 2.0, 0.2, 1.8, 0.2, 1.8, 0.0, 1.8, 0.0, 1.8, 0.0, 2.0, 1.0, 0.0, 1.0, 0.2, 1.0, 0.2, 0.8, 0.2, 0.8, 0.2, 0.8, 0.0, 0.8, 0.0, 1.0, 0.0,
		0.0, 0.0, 1.0, 2.0, 0.0, 2.0, 1.15, -0.3, 0.0, 2.0, 1.0, 2.0, 0.0, 0.0, 1.0, 0.0,
		0.0, 2.0, 0.0, 1.5,
		0.5, 2.0, 0.0, 1.5, 0.0, 1.5, 0.0, 0.5, 0.0, 0.5, 0.5, 0.0,
		0.0, 2.0, 0.5, 1.5, 0.5, 1.5, 0.5, 0.5, 0.5, 0.5, 0.0, 0.0,
		0.0, 0.4, 1.0, 1.6, 0.0, 1.6, 1.0, 0.4, 0.0, 1.0, 1.0, 1.0,
		0.0, 1.0, 1.0, 1.0, 0.5, 1.5, 0.5, 0.5,
		0.5, 0.0, 0.0, -0.5,
		0.0, 1.0, 1.0, 1.0,
		0.0, 0.0, 0.0, 0.1, 0.0, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.0, 0.1, 0.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 2.0,
		0.0, 0.0, 0.0, 2.0, 0.0, 2.0, 1.0, 2.0, 1.0, 2.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 2.0, 1.0, 0.0,
		0.0, 0.0, 1.0, 0.0, 0.5, 2.0, 0.5, 0.0, 0.5, 2.0, 0.0, 1.5,
		0.0, 1.7, 0.3, 2.0, 0.3, 2.0, 0.6, 2.0, 0.6, 2.0, 1.0, 1.7, 1.0, 1.7, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0,
		0.0, 2.0, 1.0, 2.0, 1.0, 2.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0,
		0.0, 0.6, 1.0, 0.6, 0.0, 0.6, 0.7, 2.0, 0.7, 2.0, 0.7, 0.0,
		1.0, 2.0, 0.0, 2.0, 0.0, 2.0, 0.0, 1.3, 0.0, 1.3, 1.0, 1.3, 1.0, 1.3, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0,
		1.0, 2.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.5, 1.0,
		0.0, 2.0, 1.0, 2.0, 1.0, 2.0, 0.0, 0.0, 0.2, 1.0, 0.8, 1.0,
		0.0, 2.0, 1.0, 2.0, 1.0, 2.0, 1.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0,
		0.0, 2.0, 1.0, 2.0, 1.0, 2.0, 1.0, 0.0, 0.0, 2.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0,
		0.0, 1.3, 0.0, 1.5, 0.0, 1.5, 0.2, 1.5, 0.2, 1.5, 0.2, 1.3, 0.2, 1.3, 0.0, 1.3, 0.0, 0.7, 0.0, 0.5, 0.0, 0.5, 0.2, 0.5, 0.2, 0.5, 0.2, 0.7, 0.2, 0.7, 0.0, 0.7,
		0.0, 1.3, 0.0, 1.5, 0.0, 1.5, 0.2, 1.5, 0.2, 1.5, 0.2, 1.3, 0.2, 1.3, 0.0, 1.3, 0.0, 0.7, 0.0, 0.2,
		0.0, 1.0, 1.0, 2.0, 0.0, 1.0, 1.0, 0.0,
		0.0, 1.2, 1.0, 1.2, 0.0, 0.9, 1.0, 0.9,
		0.0, 2.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0,
		0.0, 1.6, 0.0, 2.0, 0.0, 2.0, 1.0, 2.0, 1.0, 2.0, 1.0, 1.1, 1.0, 1.1, 0.5, 1.1, 0.5, 1.1, 0.5, 0.6, 0.4, 0.2, 0.6, 0.2, 0.6, 0.2, 0.6, 0.0, 0.6, 0.0, 0.4, 0.0, 0.4, 0.0, 0.4, 0.2,
		1.0, 2.0, 0.0, 2.0, 0.0, 2.0, 0.0, 0.2, 0.0, 0.2, 1.0, 0.2, 1.0, 0.2, 1.0, 1.4, 1.0, 1.4, 0.5, 1.4, 0.5, 1.4, 0.5, 0.9, 0.5, 0.9, 1.0, 0.9,
		0.0, 0.0, 0.5, 2.0, 0.5, 2.0, 1.0, 0.0, 0.25, 1.0, 0.75, 1.0, //letter A
		0.0, 0.0, 0.0, 2.0, 0.0, 2.0, 0.7, 2.0, 0.7, 2.0, 0.7, 1.3, 0.7, 1.3, 0.0, 1.3, 0.7, 1.3, 0.7, 0.0, 0.7, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 2.0, 0.0, 2.0, 1.0, 2.0, 0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 2.0, 0.0, 2.0, 0.5, 2.0, 0.5, 2.0, 1.0, 1.33, 1.0, 1.33, 1.0, 0.66, 1.0, 0.66, 0.5, 0.0, 0.5, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 2.0, 0.0, 2.0, 1.0, 2.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 2.0, 0.0, 2.0, 1.0, 2.0, 0.0, 1.0, 1.0, 1.0,
		0.0, 0.0, 0.0, 2.0, 0.0, 2.0, 1.0, 2.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.5, 1.0,
		0.0, 0.0, 0.0, 2.0, 1.0, 0.0, 1.0, 2.0, 0.0, 1.0, 1.0, 1.0,
		0.5, 0.0, 0.5, 2.0, 0.0, 2.0, 1.0, 2.0, 0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.8, 0.0, 0.8, 0.0, 0.8, 2.0, 0.6, 2.0, 1.0, 2.0,
		0.0, 0.0, 0.0, 2.0, 0.0, 1.0, 1.0, 2.0, 0.0, 1.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 2.0, 0.0, 2.0, 0.5, 1.0, 0.5, 1.0, 1.0, 2.0, 1.0, 2.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 2.0, 0.0, 2.0, 1.0, 2.0, 1.0, 2.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 2.0, 0.0, 2.0, 1.0, 2.0, 1.0, 2.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 2.0, 0.0, 2.0, 1.0, 2.0, 1.0, 2.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0,
		0.0, 0.0, 0.0, 2.0, 0.0, 2.0, 1.0, 2.0, 1.0, 2.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.5, 0.5, 1.5, -0.5,
		0.0, 0.0, 0.0, 2.0, 0.0, 2.0, 1.0, 2.0, 1.0, 2.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0,
		0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 2.0, 0.0, 2.0, 1.0, 2.0,
		0.0, 2.0, 1.0, 2.0, 0.5, 2.0, 0.5, 0.0,
		0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 2.0,
		0.0, 2.0, 0.5, 0.0, 0.5, 0.0, 1.0, 2.0,
		0.0, 2.0, 0.33, 0.0, 0.33, 0.0, 0.5, 1.0, 0.5, 1.0, 0.66, 0.0, 0.66, 0.0, 1.0, 2.0,
		0.0, 0.0, 1.0, 2.0, 0.0 ,2.0, 1.0, 0.0,                                                                                            //letter X
		0.0, 0.0, 1.0, 2.0, 0.0, 2.0, 0.5, 1.0,
		0.0, 2.0, 1.0, 2.0, 1.0, 2.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.20, 1.0, 0.8, 1.0,
		0.6, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 2.0, 0.0, 2.0, 0.6, 2.0,
		0.0, 2.0, 1.0, 0.0,
		0.0, 2.0,	0.6, 2.0, 0.6, 2.0, 0.6, 0.0, 0.6, 0.0, 0.0, 0.0,
		0.0, 1.4, 0.5, 2.0, 0.5, 2.0, 1.0, 1.4,
		0.0, 0.0, 1.0, 0.0,
		0.0, 2.0, 0.6, 1.4,
		0.25, 1.75, 1.0, 1.75, 1.0, 1.75, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0,       //letter a
		0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0,                                           //letter b
		1.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0,
		1.0, 2.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0,
		0.0, 0.7, 1.4, 0.7, 1.4, 0.7, 0.7, 1.4, 0.7, 1.4, 0.0, 0.7, 0.0, 0.7, 1.3, 0.0,
		1.0, 2.0, 0.5, 2.0, 0.5, 2.0, 0.5, -1.0, 0.0, 1.0, 1.0, 1.0,
		0.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0,
		0.0, 2.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0,
		0.0, 1.5, 0.0, 1.3, 0.0, 1.0, 0.0, 0.0,
		0.6, 1.5, 0.6, 1.3, 0.6, 1.0, 0.6, -0.8, 0.6, -0.8, 0.0, -0.8,
		0.0, 2.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.5, 0.0, 0.5, 1.0, 0.0,
		0.3, 2.0, 0.3, 0.0, 0.0, 2.0, 0.3, 2.0, 0.0, 0.0, 0.6, 0.0,
		0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 2.0, 1.0, 2.0, 1.0, 2.0, 0.0,
		0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0,
		0.0, -1.0, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0,
		1.0, -1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.6, 1.0,
		1.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.5, 0.0, 0.5, 1.0, 0.5, 1.0, 0.5, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0,
		0.5, 1.5, 0.5, 0.0, 0.0, 1.0, 1.0, 1.0,
		0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 1.0,
		0.0, 1.0, 0.5, 0.0, 0.5, 0.0, 1.0, 1.0,
		0.0, 1.0, 0.5, 0.0, 0.5, 0.0, 1.0, 1.0, 1.0, 1.0, 1.5, 0.0, 1.5, 0.0, 2.0, 1.0,
		0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0,
		1.0, 1.0, 0.0, -1.0, 0.0, 1.0, 0.5, 0.0,
		0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0,
		1.1, 0.0, 0.5, 0.0, 0.5, 0.0, 0.5, 0.5, 0.5, 0.5, 0.0, 1.0, 0.0, 1.0, 0.5, 1.5, 0.5, 1.5, 0.5, 2.0, 0.5, 2.0, 1.1, 2.0,
		0.0, 0.0, 0.0, 2.0,
		0.0, 2.0, 0.6, 2.0, 0.6, 2.0, 0.6, 1.5, 0.6, 1.5, 1.1, 1.0, 1.1, 1.0, 0.6, 0.5, 0.6, 0.5, 0.6, 0.0, 0.6, 0.0, 0.0, 0.0,
		0.0, 0.6, 0.33, 1.4, 0.33, 1.4, 0.66, 0.6, 0.66, 0.6, 1.0, 1.4
};

/*
Just a boilerplate shader builder helper.
*/
unsigned int prog(const char* vertex, const char* fragment)
{
	GLuint vert, frag;
	static GLuint program;
	char* buffer = 0;
	int n;

	vert = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vert, 1, &vertex, 0);
	glCompileShader(vert);
	glGetShaderiv(vert, GL_COMPILE_STATUS, &n);

	if(!n){
		glGetShaderiv(vert, GL_INFO_LOG_LENGTH, &n);
		buffer = realloc(buffer, n);
		glGetShaderInfoLog(vert, n, 0, buffer);
		printf("\n### vert ###\n%s\n", buffer);
	}

	glReleaseShaderCompiler();

	frag = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(frag, 1, &fragment, 0);
	glCompileShader(frag);
	glGetShaderiv(frag, GL_COMPILE_STATUS, &n);

	if(!n){
		glGetShaderiv(frag, GL_INFO_LOG_LENGTH, &n);
		buffer = realloc(buffer, n);
		glGetShaderInfoLog(frag, n, 0, buffer);
		printf("\n### frag ###\n%s\n", buffer);
	}

	glReleaseShaderCompiler();

	program = glCreateProgram();
	glAttachShader(program, vert);
	glAttachShader(program, frag);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &n);

	if(!n){
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &n);
		buffer = realloc(buffer, n);
		glGetProgramInfoLog(program, n, 0, buffer);
		printf("\n### prog ###\n%s\n", buffer);
	}

	if(buffer) free(buffer);

	glDetachShader(program, vert);
	glDetachShader(program, frag);
	glDeleteShader(vert);
	glDeleteShader(frag);

	return program;
}

/*
 A helper that load data(coordinates) to buffer. It knows some control characters too.
 It uses some global variables defined above.
*/
void setbuff(const char* u, float sx, float sy, float b)
{
	extern int indices[];
	extern float coords[];
	extern float buff[];
	extern int p;

  /*
   x coordinate where drawing start.Something like which column in the row.Reset to sx - 1.0 first and after every newline char or end of row event.
   Advance a fixed value after every char of which occupy place. I meant space and TAB included.
  */
	float s = sx - 1.0;

  /*
   Construct a loop that will visit every char in the string from start to end as order.
  */
	for(; *u; u++)
	{

    /*
     Handle some control chars and row width(end of row event).
    */
		if(*u == 10 || s > (1.0 - 3.0 * sx))
		{
			s = sx - 1.0;
			b -= 3.0 * sy;
			continue;
		}

		if(*u == 32 || *u == 9)
		{
   		s += 3.0 * sx;
			continue;
    }

    /*
     Lookup into 'indices' for start index.
     Calculate len as len = (index of char which next to currert char) minus (index of current).
   */
		int j = indices[*u-33];
		int len = indices[*u-32]-j;

    /*
     Then transfer coordinates of current char to buffer.
    */
		for(int i=0; i<len; i++)
		{
			buff[2*i+p]   = coords[2*(i+j)]*sx + s;
			buff[2*i+1+p] = coords[2*(i+j)+1]*sy + b;
		}

    /*
    Transferring to buffer finished so far. So advance the buffer position.
    Why multiply by 2? See comment section for 'indices' above.
   */
		p += 2 * len;

    /*
    Some wide or narrow shaped chars needs special handling. eg w,m,i
    Just an implementation detail.no backdoors here, trust me:)
    */
		if(*u == 109 || *u == 119) s += 2.5 * sx;
		else if(*u == 105 || *u == 108) s += 1.3 * sx;
		else s += 1.7 * sx;
	}
}

void getres(void* data)
{
	struct drm_mode_card_res        res  = {0};
	struct drm_mode_get_connector   conn;
	struct drm_mode_get_encoder     enc;
	int fd;

	struct drm* drm = (struct drm*) data;

	fd = open("/dev/dri/card0", O_RDWR | O_NONBLOCK | O_CLOEXEC);
	drm->gbm = gbm_create_device(fd);

	ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res);

	res.fb_id_ptr        = (unsigned long long) malloc(res.count_fbs * sizeof(int));
	res.crtc_id_ptr      = (unsigned long long) malloc(res.count_crtcs * sizeof(int));
	res.connector_id_ptr = (unsigned long long) malloc(res.count_connectors * sizeof(int));
	res.encoder_id_ptr   = (unsigned long long) malloc(res.count_encoders * sizeof(int));

	ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res);

	for(int i=0; i< res.count_connectors; i++)
	{
		memset(&conn, 0, sizeof(struct drm_mode_get_connector));
		conn.connector_id = ((unsigned int*)res.connector_id_ptr)[i];
		ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn);

		if(conn.connection)
		{
			conn.modes_ptr       = (unsigned long long) malloc(conn.count_modes * sizeof(struct drm_mode_modeinfo));
			conn.props_ptr       = (unsigned long long) malloc(conn.count_props * sizeof(int));
			conn.encoders_ptr    = (unsigned long long) malloc(conn.count_encoders * sizeof(struct drm_mode_get_encoder));
			conn.prop_values_ptr = (unsigned long long) malloc(conn.count_props * sizeof(struct drm_mode_get_property));

			ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn);

			drm->mode    = ((struct drm_mode_modeinfo*)conn.modes_ptr)[0];
			drm->conn_id = conn.connector_id;
			drm->enc_id  = enc.encoder_id = conn.encoder_id;
			ioctl(fd, DRM_IOCTL_MODE_GETENCODER, &enc);
			drm->crtc_id = enc.crtc_id;
			break;
		}
	}

	drm->fd = fd;
	free((void*) res.fb_id_ptr);
	free((void*) res.crtc_id_ptr);
	free((void*) res.connector_id_ptr);
	free((void*) res.encoder_id_ptr);
}

//################ MAIN ##############################

int main(int argc, char **argv) {
	struct drm_mode_crtc_page_flip flip = {0};
	struct drm_mode_fb_cmd fb_cmd;
	struct drm* drm;
	struct gbm_surface *gbm_surf;
	struct gbm_bo *bo;
       uint32_t handle, stride, width, height;

	EGLDisplay egl_disp;
	EGLSurface egl_surf;
	EGLContext egl_context;
        EGLConfig egl_conf;
	GLuint vbo, program;
        EGLint tmp;

	drm = malloc(sizeof(struct drm));
	getres(drm);
	width  = drm->mode.hdisplay;
	height = drm->mode.vdisplay;

  /*
   sx -> scale factor for x coordinates = 10.0/(vertical resulotion). 10.0 is choosen arbitrary but after drawing it seems to correspont to 10px. I am not an expert.
   sy -> same like sx but for y coordinates.
    b -> 'baseline' see freetype docs.
  */
	float sx = 10.0/width, sy = 10.0/height, b  = 1.0-3*sy;

	egl_disp = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, drm->gbm, NULL);
        eglInitialize(egl_disp, 0, 0);
        eglBindAPI(EGL_OPENGL_ES_API);
        eglChooseConfig(egl_disp, cfg, &egl_conf, 1, &tmp);

	gbm_surf = gbm_surface_create(drm->gbm, width, height, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT);
        egl_surf = eglCreatePlatformWindowSurface(egl_disp, egl_conf, gbm_surf, NULL);
	egl_context = eglCreateContext(egl_disp, egl_conf, EGL_NO_CONTEXT, ctx);
	eglMakeCurrent(egl_disp, egl_surf, egl_surf, egl_context);

	program = prog(vertex, fragment);
	glUseProgram(program);

	glEnableVertexAttribArray(0);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	glViewport(0, 0, width, height);
        glClearColor(1.0, 1.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
	glLineWidth(1.0);

  /*
   I hardcoded the string argument here.But main program can be easily extended to handle file name as argument
   since 'setbuff' already handle some control chars(eg. newline) others can be add trivially.
   Adding colored text and scaling support seems trivial too.
  */
	setbuff("The Quick Brown Fox Jumps Over The Lazy Dog. \n	The Newlined Fox Jumps Over The Lazy Dog.", sx, sy, b);
	glBufferData(GL_ARRAY_BUFFER, 2*p*sizeof(float), buff, GL_STATIC_DRAW);
	glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, sizeof(float)*2, 0);
	glDrawArrays(GL_LINES, 0, p);
	glFinish();
	eglSwapBuffers(egl_disp, egl_surf);

        bo = gbm_surface_lock_front_buffer(gbm_surf);
        handle = gbm_bo_get_handle(bo).u32;
        stride = gbm_bo_get_stride(bo);
	gbm_surface_release_buffer(gbm_surf, bo);

	fb_cmd.width = width;
	fb_cmd.height = height;
	fb_cmd.pitch = stride;
	fb_cmd.bpp = 32;
	fb_cmd.depth = 24;
	fb_cmd.handle = handle;
	ioctl(drm->fd, DRM_IOCTL_MODE_ADDFB, &fb_cmd);

	flip.crtc_id = drm->crtc_id;
	flip.fb_id = fb_cmd.fb_id;
	flip.flags = DRM_MODE_PAGE_FLIP_EVENT;
        ioctl(drm->fd, DRM_IOCTL_MODE_PAGE_FLIP, &flip);
	getchar();

	ioctl(drm->fd, DRM_IOCTL_MODE_RMFB, &fb_cmd.fb_id);

	glUseProgram(0);
	glDeleteProgram(program);
	glDeleteBuffers(1, &vbo);

	eglMakeCurrent(egl_disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(egl_disp, egl_context);
        eglDestroySurface(egl_disp, egl_surf);
        eglTerminate(egl_disp);

	gbm_surface_destroy(gbm_surf);
	gbm_device_destroy(drm->gbm);
	close(drm->fd);

	return 0;
}

/*
 I run this program in weston-terminal i get this errors. Put here just for info. Seems like weston bug?

 $ ./a.out
 i965_dri.so does not support the 0xffffffff PCI ID.
 Segmentation fault (core dumped)

 $ INTEL_DEVID_OVERRIDE=0x5916 ./a.out
 [intel_init_bufmgr: 1317] Kernel 3.6 required.
 Segmentation fault (core dumped)

*/
