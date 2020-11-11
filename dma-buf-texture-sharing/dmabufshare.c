
#include <assert.h>
#include <time.h>


#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES

#include "open_egl.h"
#include <EGL/eglext.h>
#include "socket.h"

void parse_arguments(int argc, char **argv, int *is_server);
void rotate_data(int data[4]);



void gl_draw_scene(int is_server, GLuint texture)
{
		char *n;
		if(is_server)
			n="server";
		else
			n="client";
    // clear
    GLCHK(n,glClearColor(0.2f, 0.3f, 0.3f, 1.0f));
    GLCHK(n,glClear(GL_COLOR_BUFFER_BIT));

    // draw quad
    // VAO and shader program are already bound from the call to gl_setup_scene
    GLCHK(n,glActiveTexture(GL_TEXTURE0));
    GLCHK(n,glBindTexture(GL_TEXTURE_2D, texture));
    GLCHK(n,glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0));
}

int compile_shader(char *n, GLuint shader)
{
	
	GLint status;
	GLchar *log;
	GLsizei len;
	GLCHK(n,glCompileShader(shader));
	glGetShaderiv(shader,GL_COMPILE_STATUS,&status);
	
	if(GL_TRUE != status){
		
		printf("%s: Unable to compile!\n",n);
		glGetShaderiv(shader,GL_INFO_LOG_LENGTH,&status);
		if(status >0){
			if(NULL ==(log=malloc(status))){
				printf("%s: unable to alloc for log len of %d\n",n,status);
			}else{
				
				glGetShaderInfoLog(shader,status,&len,log);
				printf("%s: shader compile message:\n'%s'\n",n,log);
				free(log);
			}
		}
		return -1;
	}
	return 0;
}

int gl_setup_scene(int is_server)
{
	//"#version 330 core\n"
    // Shader source that draws a textures quad
  const char *vertex_shader_source =   "#version 300 es\nlayout (location = 0) in vec3 aPos;\n"
  "layout (location = 1) in vec2 aTexCoords;\n"
  "out vec2 TexCoords;\n"
  "void main()\n"
  "{\n"
  "   TexCoords = aTexCoords;\n"
  "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
  "}\0";
  
  const char *fragment_shader_source = "#version 300 es\n"
	"precision mediump float;\n"
	"out vec4 FragColor;\n"
  "in vec2 TexCoords;\n"
  "uniform sampler2D Texture1;\n"
  "void main()\n"
  "{\n"
  "   FragColor = texture(Texture1, TexCoords);\n"
  "}\0";
	
	char *n;
	GLint status;
	if(is_server)
		n="gl_SS_server";
	else
		n="gl_SS_Client";

  // vertex shader
  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  GLCHK(n,glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL));
	if(compile_shader(is_server?"gl_SS_Server Vertex":"gl_SS_client Vertex",vertex_shader))
		return -1;
  // fragment shader
  int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  GLCHK(n,glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL));
	if(compile_shader(is_server?"gl_SS_Server fragment":"gl_SS_client fragment",fragment_shader))
		return -1;
	
  // link shaders
  int shader_program = glCreateProgram();
  GLCHK(n,glAttachShader(shader_program, vertex_shader));
  GLCHK(n,glAttachShader(shader_program, fragment_shader));
  GLCHK(n,glLinkProgram(shader_program));
	// make sure it linked.
	glGetProgramiv(shader_program,GL_LINK_STATUS, &status);
	if(GL_TRUE != status){
		printf("%s: Unable to link program!\n",n);
	}
  // delete shaders
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  // quad
  float vertices[] = {
      0.5f, 0.5f, 0.0f, 0.0f, 0.0f,   // top right
      0.5f, -0.5f, 0.0f, 1.0f, 0.0f,  // bottom right
      -0.5f, -0.5f, 0.0f, 1.0f, 1.0f, // bottom left
      -0.5f, 0.5f, 0.0f, 0.0f, 1.0f   // top left
  };
  unsigned int indices[] = {
      0, 1, 3, // first Triangle
      1, 2, 3  // second Triangle
  };

  unsigned int VBO, VAO, EBO;
  GLCHK(n,glGenVertexArrays(1, &VAO));
  GLCHK(n,glGenBuffers(1, &VBO));
  GLCHK(n,glGenBuffers(1, &EBO));
  GLCHK(n,glBindVertexArray(VAO));

  GLCHK(n,glBindBuffer(GL_ARRAY_BUFFER, VBO));
  GLCHK(n,glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW));

  GLCHK(n,glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO));
  GLCHK(n,glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW));

  GLCHK(n,glEnableVertexAttribArray(0));
  GLCHK(n,glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0));
  GLCHK(n,glEnableVertexAttribArray(1));
  GLCHK(n,glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float))));

  GLCHK(n,glBindBuffer(GL_ARRAY_BUFFER, 0));

  GLCHK(n,glBindVertexArray(0));

  // Prebind needed stuff for drawing
  GLCHK(n,glUseProgram(shader_program));
  GLCHK(n,glBindVertexArray(VAO));
}


int main(int argc, char **argv)
{
    // Parse arguments
	int is_server;
	struct drm_gpu *g;
	char *n;
	EGLint attribute_list_config[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_NONE};
	
	
	
	
  parse_arguments(argc, argv, &is_server);
  if(NULL == (g=find_display_configuration("/dev/dri/card0")))
  	return -1;
	
	
	setup_opengl (g, attribute_list_config, NULL, is_server?0:1);  

  // Initialize EGL


  // Setup GL scene
  if(0 != gl_setup_scene(is_server))
  	return -1;

  // Server texture data { red, greee, blue, white }
  int texture_data[4] = {0x000000FF, 0x0000FF00, 0X00FF0000, 0x00FFFFFF};

  // -----------------------------
  // --- Texture sharing start ---
  // -----------------------------

  // Socket paths for sending/receiving file descriptor and image storage data
  const char *SERVER_FILE = "/tmp/test_server";
  const char *CLIENT_FILE = "/tmp/test_client";
  // Custom image storage data description to transfer over socket
  struct texture_storage_metadata_t {
        int fourcc;
        EGLint offset;
        EGLint stride;
  };

    // GL texture that will be shared
  GLuint texture;

    // The next `if` block contains server code in the `true` branch and client code in the `false` branch. The `true` branch is always executed first and the `false` branch after it (in a different process). This is because the server loops at the end of the branch until it can send a message to the client and the client blocks at the start of the branch until it has a message to read. This way the whole `if` block from top to bottom represents the order of events as they happen.
	if (is_server) {
		n="server";
		// GL: Create and populate the texture
		GLCHK(n,glGenTextures(1, &texture));
		GLCHK(n,glBindTexture(GL_TEXTURE_2D, texture));
		GLCHK(n,glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));
		GLCHK(n,glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, texture_data));
		GLCHK(n,glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
		GLCHK(n,glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
		
		// EGL: Create EGL image from the GL texture
		EGLImage image = eglCreateImage(g->display,
		                                g->context,
		                                EGL_GL_TEXTURE_2D,
		                                (EGLClientBuffer)(uint64_t)texture,
		                                NULL);
		assert(image != EGL_NO_IMAGE);
		
		// The next line works around an issue in radeonsi driver (fixed in master at the time of writing). If you are
		// not having problems with texture rendering until the first texture update you can omit this line
		glFlush();
		
		// EGL (extension: EGL_MESA_image_dma_buf_export): Get file descriptor (texture_dmabuf_fd) for the EGL image and get its
		// storage data (texture_storage_metadata - fourcc, stride, offset)
		int texture_dmabuf_fd;
		struct texture_storage_metadata_t texture_storage_metadata;
		
		PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC eglExportDMABUFImageQueryMESA =
		    (PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC)eglGetProcAddress("eglExportDMABUFImageQueryMESA");
		EGLBoolean queried = eglExportDMABUFImageQueryMESA(g->display,
		                                                   image,
		                                                   &texture_storage_metadata.fourcc,
		                                                   NULL,
		                                                   NULL);
		assert(queried);
		PFNEGLEXPORTDMABUFIMAGEMESAPROC eglExportDMABUFImageMESA =
		    (PFNEGLEXPORTDMABUFIMAGEMESAPROC)eglGetProcAddress("eglExportDMABUFImageMESA");
		EGLBoolean exported = eglExportDMABUFImageMESA(g->display,
		                                               image,
		                                               &texture_dmabuf_fd,
		                                               &texture_storage_metadata.stride,
		                                               &texture_storage_metadata.offset);
		assert(exported);
		printf("Waiting on Client to connect\n");
		// Unix Domain Socket: Send file descriptor (texture_dmabuf_fd) and texture storage data (texture_storage_metadata)
		int sock = create_socket(SERVER_FILE);
		while (connect_socket(sock, CLIENT_FILE) != 0)
		    ;
		write_fd(sock, texture_dmabuf_fd, &texture_storage_metadata, sizeof(texture_storage_metadata));
		close(sock);
		close(texture_dmabuf_fd);
		printf("Sent Client data\n");
  }else {
		// Unix Domain Socket: Receive file descriptor (texture_dmabuf_fd) and texture storage data (texture_storage_metadata)
		int texture_dmabuf_fd;
		struct texture_storage_metadata_t texture_storage_metadata;
		
		int sock = create_socket(CLIENT_FILE);
		n="client";
		printf("Waiting on Server\n");
		read_fd(sock, &texture_dmabuf_fd, &texture_storage_metadata, sizeof(texture_storage_metadata));
		close(sock);
		printf("Got Data from server\n");
		
		// EGL (extension: EGL_EXT_image_dma_buf_import): Create EGL image from file descriptor (texture_dmabuf_fd) and storage
		// data (texture_storage_metadata)
		EGLAttrib const attribute_list[] = {
		    EGL_WIDTH, 2,
		    EGL_HEIGHT, 2,
		    EGL_LINUX_DRM_FOURCC_EXT, texture_storage_metadata.fourcc,
		    EGL_DMA_BUF_PLANE0_FD_EXT, texture_dmabuf_fd,
		    EGL_DMA_BUF_PLANE0_OFFSET_EXT, texture_storage_metadata.offset,
		    EGL_DMA_BUF_PLANE0_PITCH_EXT, texture_storage_metadata.stride,
		    EGL_NONE};
		EGLImage image = eglCreateImage(g->display,
		                                NULL,
		                                EGL_LINUX_DMA_BUF_EXT,
		                                (EGLClientBuffer)NULL,
		                                attribute_list);
		assert(image != EGL_NO_IMAGE);
		close(texture_dmabuf_fd);
		
		// GLES (extension: GL_OES_EGL_image_external): Create GL texture from EGL image
		GLCHK(n,glGenTextures(1, &texture));
		GLCHK(n,glBindTexture(GL_TEXTURE_2D, texture));
		GLCHK(n,glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image));
		GLCHK(n,glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
		GLCHK(n,glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	}

    // -----------------------------
    // --- Texture sharing end ---
    // -----------------------------
	printf("%s: Starting main loop\n",n);
  time_t last_time = time(NULL);
  while (1) {
		EGLint erregl;
		GLint errgl;
    time_t cur_time;
		cur_time = time(NULL);
		// Update texture data each second to see that the client didn't just copy the texture and is indeed referencing
		if (last_time < cur_time) {
			
      last_time = cur_time;
			if(is_server)
				swap_buffers(1,g);
			else {
				
				rotate_data(texture_data);
	      GLCHK(n,glBindTexture(GL_TEXTURE_2D, texture));
	      GLCHK(n,glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, texture_data));	
				gl_draw_scene(is_server,texture);
				eglSwapBuffers (g->display, g->egl_surface);
			}
      
			// Check for errors
			if(GL_NO_ERROR != (errgl=glGetError()))
				printf("GL error %d (%s)\n",errgl, glErrorString(errgl));
			
			if(EGL_SUCCESS != (erregl=eglGetError()))
				printf("EGL Error %d (%s)\n",eglErrorString(eglGetError()));
			if(GL_NO_ERROR != errgl || EGL_SUCCESS != erregl)
				break;
    }
    usleep(100);
  }

  return 0;
}

void help()
{
    printf("USAGE:\n"
           "    dmabufshare server\n"
           "    dmabufshare client\n");
}

void parse_arguments(int argc, char **argv, int *is_server)
{
    if (2 == argc)
    {
        if (strcmp(argv[1], "server") == 0)
        {
            *is_server = 1;
        }
        else if (strcmp(argv[1], "client") == 0)
        {
            *is_server = 0;
        }
        else if (strcmp(argv[1], "--help") == 0)
        {
            help();
            exit(0);
        }
        else
        {
            help();
            exit(-1);
        }
    }
    else
    {
        help();
        exit(-1);
    }
}

void rotate_data(int data[4])
{
    int temp = data[0];
    data[0] = data[1];
    data[1] = data[3];
    data[3] = data[2];
    data[2] = temp;
}
