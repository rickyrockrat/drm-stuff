
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>


#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "open_egl.h"

#include "socket.h"

void parse_arguments(int argc, char **argv, int *is_server);
void rotate_data(int data[4]);

void gl_draw_scene(GLuint texture)
{
    // clear
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // draw quad
    // VAO and shader program are already bound from the call to gl_setup_scene
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}


void gl_setup_scene()
{
    // Shader source that draws a textures quad
    const char *vertex_shader_source = "#version 330 core\n"
                                       "layout (location = 0) in vec3 aPos;\n"
                                       "layout (location = 1) in vec2 aTexCoords;\n"

                                       "out vec2 TexCoords;\n"

                                       "void main()\n"
                                       "{\n"
                                       "   TexCoords = aTexCoords;\n"
                                       "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
                                       "}\0";
    const char *fragment_shader_source = "#version 330 core\n"
                                         "out vec4 FragColor;\n"

                                         "in vec2 TexCoords;\n"

                                         "uniform sampler2D Texture1;\n"

                                         "void main()\n"
                                         "{\n"
                                         "   FragColor = texture(Texture1, TexCoords);\n"
                                         "}\0";

    // vertex shader
    int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    // fragment shader
    int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    // link shaders
    int shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);
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
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);

    // Prebind needed stuff for drawing
    glUseProgram(shader_program);
    glBindVertexArray(VAO);
}


int main(int argc, char **argv)
{
    // Parse arguments
    int is_server;
		struct drm_gpu *g;
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
    gl_setup_scene();

    // Server texture data { red, greee, blue, white }
    int texture_data[4] = {0x000000FF, 0x0000FF00, 0X00FF0000, 0x00FFFFFF};

    // -----------------------------
    // --- Texture sharing start ---
    // -----------------------------

    // Socket paths for sending/receiving file descriptor and image storage data
    const char *SERVER_FILE = "/tmp/test_server";
    const char *CLIENT_FILE = "/tmp/test_client";
    // Custom image storage data description to transfer over socket
    struct texture_storage_metadata_t
    {
        int fourcc;
        EGLint offset;
        EGLint stride;
    };

    // GL texture that will be shared
    GLuint texture;

    // The next `if` block contains server code in the `true` branch and client code in the `false` branch. The `true` branch is always executed first and the `false` branch after it (in a different process). This is because the server loops at the end of the branch until it can send a message to the client and the client blocks at the start of the branch until it has a message to read. This way the whole `if` block from top to bottom represents the order of events as they happen.
    if (is_server)
    {
        // GL: Create and populate the texture
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

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

        // Unix Domain Socket: Send file descriptor (texture_dmabuf_fd) and texture storage data (texture_storage_metadata)
        int sock = create_socket(SERVER_FILE);
        while (connect_socket(sock, CLIENT_FILE) != 0)
            ;
        write_fd(sock, texture_dmabuf_fd, &texture_storage_metadata, sizeof(texture_storage_metadata));
        close(sock);
        close(texture_dmabuf_fd);
    }
    else
    {
        // Unix Domain Socket: Receive file descriptor (texture_dmabuf_fd) and texture storage data (texture_storage_metadata)
        int texture_dmabuf_fd;
        struct texture_storage_metadata_t texture_storage_metadata;

        int sock = create_socket(CLIENT_FILE);
        read_fd(sock, &texture_dmabuf_fd, &texture_storage_metadata, sizeof(texture_storage_metadata));
        close(sock);

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
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    // -----------------------------
    // --- Texture sharing end ---
    // -----------------------------

    time_t last_time = time(NULL);
    while (1)
    {
        // Draw scene (uses shared texture)
        gl_draw_scene(texture);
        eglSwapBuffers(g->display, g->egl_surface);

        // Update texture data each second to see that the client didn't just copy the texture and is indeed referencing
        // the same texture data.
        if (is_server)
        {
            time_t cur_time = time(NULL);
            if (last_time < cur_time)
            {
                last_time = cur_time;
                rotate_data(texture_data);
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);
            }
        }

        // Check for errors
        assert(glGetError() == GL_NO_ERROR);
        assert(eglGetError() == EGL_SUCCESS);
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
