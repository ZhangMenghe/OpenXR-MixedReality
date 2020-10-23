#include "pch.h"

#include "SimpleRenderer.h"
#include <GLPipeline/Mesh.h>
#include <GLPipeline/Primitive.h>
// These are used by the shader compilation methods.
#include <vector>

#define STRING(s) #s

GLuint CompileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);

    const char* sourceArray[1] = {source.c_str()};
    glShaderSource(shader, 1, sourceArray, NULL);
    glCompileShader(shader);

    GLint compileResult;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);

    if (compileResult == 0) {
        GLint infoLogLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);

        std::vector<GLchar> infoLog(infoLogLength);
        glGetShaderInfoLog(shader, (GLsizei)infoLog.size(), NULL, infoLog.data());

        std::string errorMessage = std::string("Shader compilation failed: ");
        errorMessage += std::string(infoLog.begin(), infoLog.end());

        throw std::exception(errorMessage.c_str());
    }

    return shader;
}

GLuint CompileProgram(const std::string& vsSource, const std::string& fsSource) {
    GLuint program = glCreateProgram();

    if (program == 0) {
        throw std::exception("Program creation failed");
    }

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSource);

    if (vs == 0 || fs == 0) {
        glDeleteShader(fs);
        glDeleteShader(vs);
        glDeleteProgram(program);
        return 0;
    }

    glAttachShader(program, vs);
    glDeleteShader(vs);

    glAttachShader(program, fs);
    glDeleteShader(fs);

    glLinkProgram(program);

    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);

    if (linkStatus == 0) {
        GLint infoLogLength;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);

        std::vector<GLchar> infoLog(infoLogLength);
        glGetProgramInfoLog(program, (GLsizei)infoLog.size(), NULL, infoLog.data());

        std::string errorMessage = std::string("Program link failed: ");
        errorMessage += std::string(infoLog.begin(), infoLog.end());

        throw std::exception(errorMessage.c_str());

    }

    return program;
}

SimpleRenderer::SimpleRenderer() {
    // Vertex Shader source
    const std::string vs = STRING(
        precision mediump float;

        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec3 aTexcoord;

        void main() {
            gl_Position = vec4(aPosition.xy, .0, 1.0);
        }        
        );

    // Fragment Shader source
    const std::string fs = STRING(
        precision mediump float;

        out vec4 fragColor;
        void main() { fragColor = vec4(1.0, .0, .0, 1.0); }
        );




            const std::string vso = STRING(
            in vec3 aPosition; 
            void main() {
                gl_Position = vec4(aPosition.xy, .0, 1.0);
            });

        // Fragment Shader source
        const std::string fso = STRING(
            precision mediump float;

            out vec4 fragColor;
            void main() { fragColor = vec4(1.0, .0, .0, 1.0); });

                if (!shader_.AddShader(GL_VERTEX_SHADER, vs) 
                    || !shader_.AddShader(GL_FRAGMENT_SHADER, fs) 
                    || !shader_.CompileAndLink())
            throw std::exception("Failed to create quad shader");


        Mesh::InitQuadWithTex(vao_, quad_vertices_tex, 4, quad_indices, 6);
}

SimpleRenderer::~SimpleRenderer() {

}
void SimpleRenderer::Draw() {
    // On HoloLens, it is important to clear to transparent.
    glClearColor(1.0f, 1.f, 0.f, 1.f);

    // On HoloLens, this will also update the camera buffers (constant and back).
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GLuint sp = shader_.Use();
    //Shader::Uniform(sp, "uScale", r_scale_);
    //Shader::Uniform(sp, "uOffset", r_offset_);
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    shader_.UnUse();
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    glFlush();
}


void SimpleRenderer::UpdateWindowSize(int offsetx, int offsety, GLsizei width, GLsizei height) {
    glViewport(0,0, width, height);

    mWindowWidth = width;
    mWindowHeight = height;
}
