#ifndef SIMPLE_RENDERER_H
#define SIMPLE_RENDERER_H

#include "pch.h"
#include <GLPipeline/Shader.h>



    class SimpleRenderer {
    public:
        SimpleRenderer();
        ~SimpleRenderer();
        void Draw();
        void UpdateWindowSize(int offx, int offy, GLsizei width, GLsizei height);

    protected:
        GLuint vao_;
        Shader shader_;
        glm::vec2 r_scale_, r_offset_;
        const static int MAX_INSTANCES = 5;

    private:
        GLuint mProgram;
        GLsizei mWindowWidth;
        GLsizei mWindowHeight;

        GLint mPositionAttribLocation;
        GLint mColorAttribLocation;

        GLint mModelUniformLocation;
        GLint mViewUniformLocation;
        GLint mProjUniformLocation;
        GLint mRtvIndexAttribLocation;

        GLuint mVertexPositionBuffer;
        GLuint mVertexColorBuffer;
        GLuint mIndexBuffer;
        GLuint mRenderTargetArrayIndices;

        int mDrawCount;
        bool mIsHolographic;
    };
#endif