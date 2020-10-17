#ifndef SIMPLE_OBJ_RENDERER_H
#define SIMPLE_OBJ_RENDERER_H
#include <pch.h>
#include <XrSceneLib/Scene.h>
#include <XrSceneLib/Context.h>
class SimpleObjRenderer : public engine::Object {
public:
    SimpleObjRenderer(bool holographic);
    ~SimpleObjRenderer();
    void Render(engine::Context& context) const override;
    //void UpdateWindowSize(GLsizei width, GLsizei height);

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
#endif // !SIMPLE_OBJ_RENDERER_H
