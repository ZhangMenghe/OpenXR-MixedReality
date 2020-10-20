#pragma once

#include "pch.h"

    class SimpleRenderer {
    public:
        SimpleRenderer(bool holographic);
        ~SimpleRenderer();
        void Draw(MathHelper::Matrix4 proj_mat);
        void UpdateWindowSize(int offx, int offy, GLsizei width, GLsizei height);

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
