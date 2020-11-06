#include <QtGui/QGuiApplication>
#include <QtGui/QSurfaceFormat>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLFunctions_4_3_Core>
#include <QtGui/QOpenGLFramebufferObject>
#include <QtGui/QOpenGLShaderProgram>
#include <QDebug>
#include <QImage>
#include <QOpenGLBuffer>

int write_tga()
{
int* buffer = new int[ WIDTH * HEIGHT * 3 ];
...
glReadPixels( 0, 0, WIDTH, HEIGHT, GL_BGR, GL_UNSIGNED_BYTE, buffer );

Then you can write the buffer into a .tga file:

FILE   *out = fopen(tga_file, "w");
short  TGAhead[] = {0, 2, 0, 0, 0, 0, WIDTH, HEIGHT, 24};
fwrite(&TGAhead, sizeof(TGAhead), 1, out);
fwrite(buffer, 3 * WIDTH * HEIGHT, 1, out);
fclose(out);
}

int main(int argc, char* argv[])
{
   QGuiApplication a(argc, argv);

   QSurfaceFormat surfaceFormat;
   surfaceFormat.setMajorVersion(4);
   surfaceFormat.setMinorVersion(3);

   QOpenGLContext openGLContext;
   openGLContext.setFormat(surfaceFormat);
   openGLContext.create();
   if(!openGLContext.isValid()) return -1;

   QOffscreenSurface surface;
   surface.setFormat(surfaceFormat);
   surface.create();
   if(!surface.isValid()) return -2;

   openGLContext.makeCurrent(&surface);

   QOpenGLFunctions_4_3_Core f;
   if(!f.initializeOpenGLFunctions()) return -3;

   qDebug() << QString::fromLatin1((const char*)f.glGetString(GL_VERSION));

   QSize vpSize = QSize(100, 200);

   qDebug("Hi");

   QOpenGLFramebufferObjectFormat fboFormat;
   fboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
   QOpenGLFramebufferObject fbo(vpSize, fboFormat);

   fbo.bind();

    // //////////


   static const float vertexPositions[] = {
       -0.8f, -0.8f, 0.0f,
        0.8f, -0.8f, 0.0f,
        0.0f,  0.8f, 0.0f
   };

   static const float vertexColors[] = {
       1.0f, 0.0f, 0.0f,
       0.0f, 1.0f, 0.0f,
       0.0f, 0.0f, 1.0f
   };

   QOpenGLBuffer vertexPositionBuffer(QOpenGLBuffer::VertexBuffer);
   vertexPositionBuffer.create();
   vertexPositionBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
   vertexPositionBuffer.bind();
   vertexPositionBuffer.allocate(vertexPositions, 9 * sizeof(float));

   QOpenGLBuffer vertexColorBuffer(QOpenGLBuffer::VertexBuffer);
   vertexColorBuffer.create();
   vertexColorBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
   vertexColorBuffer.bind();
   vertexColorBuffer.allocate(vertexColors, 9 * sizeof(float));

   QOpenGLShaderProgram program;
   program.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                   "#version 330\n"
                                   "in vec3 position;\n"
                                   "in vec3 color;\n"
                                   "out vec3 fragColor;\n"
                                   "void main() {\n"
                                   "    fragColor = color;\n"
                                   "    gl_Position = vec4(position, 1.0);\n"
                                   "}\n"
                                   );
   program.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                   "#version 330\n"
                                   "in vec3 fragColor;\n"
                                   "out vec4 color;\n"
                                   "void main() {\n"
                                   "    color = vec4(fragColor, 1.0);\n"
                                   "}\n"
                                   );
   program.link();
   program.bind();

   vertexPositionBuffer.bind();
   program.enableAttributeArray("position");
   program.setAttributeBuffer("position", GL_FLOAT, 0, 3);

   vertexColorBuffer.bind();
   program.enableAttributeArray("color");
   program.setAttributeBuffer("color", GL_FLOAT, 0, 3);

   f.glClearColor(0.3f, 0.0f, 0.7f, 1.0f);
   f.glClear(GL_COLOR_BUFFER_BIT);

   f.glDrawArrays(GL_TRIANGLES, 0, 3);

   program.disableAttributeArray("position");
   program.disableAttributeArray("color");

   program.release();

   // ///////////////

   fbo.release();

   qDebug("FBO released");

   QImage im = fbo.toImage();

   if (im.save("asd.png")){
       qDebug("Image saved!!");
   }

   return 0;
}
