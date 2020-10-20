//*********************************************************
//    Copyright (c) Microsoft. All rights reserved.
//
//    Apache 2.0 License
//
//    You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
//    implied. See the License for the specific language governing
//    permissions and limitations under the License.
//
//*********************************************************
#include "pch.h"
#include <XrSceneLib/PbrModelObject.h>
#include <XrSceneLib/Scene.h>
#include "SimpleRenderer.h"

using namespace DirectX;
using namespace xr::math;
using namespace std::chrono;
using namespace std::chrono_literals;

namespace {
    //
    // This sample displays a simple orbit in front of the user using local reference space
    // and shows how to pause the animation when the session lost focus while it continues rendering.
    // Also demos Gaze-Select interaction, that the user can air tap to move the orbit in front of the user.
    //
    struct OrbitScene : public engine::Scene {
        OrbitScene(engine::Context& context)
            : Scene(context) {
            xr::ActionSet& actionSet = ActionContext().CreateActionSet("orbit_scene_actions", "Orbit Scene Actions");

            m_selectAction = actionSet.CreateAction("select_action", "Select Action", XR_ACTION_TYPE_BOOLEAN_INPUT, {});

            ActionContext().SuggestInteractionProfileBindings("/interaction_profiles/khr/simple_controller",
                                                              {
                                                                  {m_selectAction, "/user/hand/right/input/select"},
                                                                  {m_selectAction, "/user/hand/left/input/select"},
                                                              });

            ActionContext().SuggestInteractionProfileBindings("/interaction_profiles/microsoft/motion_controller",
                                                              {
                                                                  {m_selectAction, "/user/hand/right/input/trigger"},
                                                                  {m_selectAction, "/user/hand/left/input/trigger"},
                                                              });

            if (context.Extensions.SupportsHandInteraction) {
                ActionContext().SuggestInteractionProfileBindings("/interaction_profiles/microsoft/hand_interaction",
                                                                  {
                                                                      {m_selectAction, "/user/hand/right/input/select"},
                                                                      {m_selectAction, "/user/hand/left/input/select"},
                                                                  });
            }

            m_sun = AddObject(engine::CreateSphere(m_context.PbrResources, 0.5f, 20, Pbr::FromSRGB(Colors::OrangeRed)));
            m_sun->SetVisible(false); // invisible until tracking is valid and placement succeeded.

            m_earth = AddObject(engine::CreateSphere(m_context.PbrResources, 0.1f, 20, Pbr::FromSRGB(Colors::SeaGreen)));
            m_earth->SetParent(m_sun);

            XrReferenceSpaceCreateInfo createInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
            createInfo.poseInReferenceSpace = Pose::Identity();
            CHECK_XRCMD(xrCreateReferenceSpace(m_context.Session.Handle, &createInfo, m_viewSpace.Put()));
            InitializeEGL(600, 400);
        }
        void InitializeEGL(int width, int height) {
            D3D11_TEXTURE2D_DESC texDesc = {0};
            texDesc.Width = width;
            texDesc.Height = height;
            texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            texDesc.MipLevels = 1;
            texDesc.ArraySize = 1;
            texDesc.SampleDesc.Count = 1;
            texDesc.SampleDesc.Quality = 0;
            texDesc.Usage = D3D11_USAGE_DEFAULT;
            texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            texDesc.CPUAccessFlags = 0;
            texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

            d3dTex = nullptr;
            HRESULT hr = m_context.Device.get()->CreateTexture2D(&texDesc, nullptr, &d3dTex);
            if FAILED (hr) {
                // error handling code
                throw std::exception("failed to create handler");
            }
            HANDLE sharedHandle = GetHandle(*d3dTex);
            mEglSurface = EGL_NO_SURFACE;

            initialize_gl_contex();
            EGLint pBufferAttributes[] = {
                EGL_WIDTH, width, EGL_HEIGHT, height, EGL_TEXTURE_TARGET, EGL_TEXTURE_2D, EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA, EGL_NONE};

            mEglSurface = eglCreatePbufferFromClientBuffer(
                mEglDisplay, EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE, sharedHandle, mEglConfig, pBufferAttributes);
            if (mEglSurface == EGL_NO_SURFACE) {
                throw std::exception("no EGLsurface");
            }
            const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};

            auto mEglContext = eglCreateContext(mEglDisplay, mEglConfig, EGL_NO_CONTEXT, contextAttributes);
            if (mEglContext == EGL_NO_CONTEXT) {
                throw std::exception("Failed to create EGL context");
            }
            EGLBoolean success = eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext);
            if (success == EGL_FALSE) {
                throw std::exception("Failed to make fullscreen EGLSurface current");
            }
            EGLint panelWidth = 0;
            EGLint panelHeight = 0;
            eglQuerySurface(mEglDisplay, mEglSurface, EGL_WIDTH, &panelWidth);
            eglQuerySurface(mEglDisplay, mEglSurface, EGL_HEIGHT, &panelHeight);
            mCubeRenderer = new SimpleRenderer(false);
            //mCubeRenderer->UpdateWindowSize(panelWidth, panelHeight);
            
        }
        void OnUpdate(const engine::FrameTime& frameTime) override {
            //XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
            //XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
            //getInfo.action = m_selectAction;
            //CHECK_XRCMD(xrGetActionStateBoolean(m_context.Session.Handle, &getInfo, &state));
            //const bool isSelectPressed = state.isActive && state.changedSinceLastSync && state.currentState;
            //const bool firstUpdate = !m_sun->IsVisible();

            //if (firstUpdate || isSelectPressed) {
            //    const XrTime time = state.isActive ? state.lastChangeTime : frameTime.PredictedDisplayTime;
            //    XrSpaceLocation viewInScene = {XR_TYPE_SPACE_LOCATION};
            //    CHECK_XRCMD(xrLocateSpace(m_viewSpace.Get(), m_context.SceneSpace, time, &viewInScene));

            //    if (Pose::IsPoseValid(viewInScene)) {
            //        // Project the forward of the view to the scene's horizontal plane
            //        const XrPosef viewFrontInView = {{0, 0, 0, 1}, {0, 0, -1}};
            //        const XrPosef viewFrontInScene = viewFrontInView * viewInScene.pose;
            //        const XrVector3f viewForwardInScene = viewFrontInScene.position - viewInScene.pose.position;
            //        const XrVector3f viewForwardInGravity = Dot(viewForwardInScene, {0, -1, 0}) * XrVector3f{0, -1, 0};
            //        const XrVector3f userForwardInScene = Normalize(viewForwardInScene - viewForwardInGravity);

            //        // Put the sun 2 meters in front of the user at eye level
            //        const XrVector3f sunInScene = viewInScene.pose.position + 2.f * userForwardInScene;
            //        m_targetPoseInScene = Pose::LookAt(sunInScene, userForwardInScene, {0, 1, 0});

            //        if (firstUpdate) {
            //            m_sun->SetVisible(true);
            //            m_sun->Pose() = m_targetPoseInScene;
            //        }
            //    }
            //}

            //// Slowly ease the sun to the target location
            //m_sun->Pose() = Pose::Slerp(m_sun->Pose(), m_targetPoseInScene, 0.05f);

            //// Animate the earth orbiting the sun, and pause when app lost focus.
            //if (m_context.SessionState == XR_SESSION_STATE_FOCUSED) {
            //    const float angle = frameTime.TotalElapsedSeconds * XM_PI; // half circle a second

            //    XrVector3f earthPosition;
            //    earthPosition.x = 0.6f * sin(angle);
            //    earthPosition.y = 0.0f;
            //    earthPosition.z = 0.6f * cos(angle);
            //    m_earth->Pose().position = earthPosition;
            //}
        }
        void Render(const const engine::FrameTime& frameTime, uint32_t viewIndex) {
            //mCubeRenderer->Draw();

        }
        
    private:
        XrAction m_selectAction{XR_NULL_HANDLE};
        XrPosef m_targetPoseInScene = Pose::Identity();
        std::shared_ptr<engine::PbrModelObject> m_sun;
        std::shared_ptr<engine::PbrModelObject> m_earth;
        xr::SpaceHandle m_viewSpace;

        ID3D11Texture2D* d3dTex;
        EGLConfig mEglConfig;
        EGLDisplay mEglDisplay;
        EGLSurface mEglSurface;
        SimpleRenderer* mCubeRenderer;

        HANDLE GetHandle(ID3D11Texture2D& D3D11Texture2D) {
            IDXGIResource* DXGIResource;

            const HRESULT DXGIResourceResult =
                D3D11Texture2D.QueryInterface(__uuidof(IDXGIResource), reinterpret_cast<void**>(&DXGIResource));

            if (FAILED(DXGIResourceResult)) {
                return 0;
            }

            HANDLE SharedHandle;
            const HRESULT SharedHandleResult = DXGIResource->GetSharedHandle(&SharedHandle);
            return SharedHandle;
            //DXGIResource->Release();

            //if (FAILED(SharedHandleResult)) {
            //    return 0;
            //}

            //return HandleToULong(SharedHandle);
        }
        void initialize_gl_contex() {
            const EGLint configAttributes[] = {
                EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 8, EGL_STENCIL_SIZE, 8, EGL_NONE};

            const EGLint defaultDisplayAttributes[] = {
                // These are the default display attributes, used to request ANGLE's D3D11 renderer.
                // eglInitialize will only succeed with these attributes if the hardware supports D3D11 Feature Level 10_0+.
                EGL_PLATFORM_ANGLE_TYPE_ANGLE,
                EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,

                // EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER is an optimization that can have large performance benefits on mobile
                // devices. Its syntax is subject to change, though. Please update your Visual Studio templates if you experience
                // compilation issues with it.
                // EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER,
                // EGL_TRUE,

                // EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE is an option that enables ANGLE to automatically call
                // the IDXGIDevice3::Trim method on behalf of the application when it gets suspended.
                // Calling IDXGIDevice3::Trim when an application is suspended is a Windows Store application certification requirement.
                EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE,
                EGL_TRUE,
                EGL_NONE,
            };
            const EGLint fl9_3DisplayAttributes[] = {
                // These can be used to request ANGLE's D3D11 renderer, with D3D11 Feature Level 9_3.
                // These attributes are used if the call to eglInitialize fails with the default display attributes.
                EGL_PLATFORM_ANGLE_TYPE_ANGLE,
                EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
                EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE,
                9,
                EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE,
                3,
                // EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER,
                // EGL_TRUE,
                EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE,
                EGL_TRUE,
                EGL_NONE,
            };

            const EGLint warpDisplayAttributes[] = {
                // These attributes can be used to request D3D11 WARP.
                // They are used if eglInitialize fails with both the default display attributes and the 9_3 display attributes.
                EGL_PLATFORM_ANGLE_TYPE_ANGLE,
                EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
                // EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,
                // EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE,
                // EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER,
                // EGL_TRUE,
                EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE,
                EGL_TRUE,
                EGL_NONE,
            };
            PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT =
                reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
            if (!eglGetPlatformDisplayEXT) {
                std::exception("Failed to get function eglGetPlatformDisplayEXT");
            }

            // This tries to initialize EGL to D3D11 Feature Level 10_0+. See above comment for details.
            mEglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, defaultDisplayAttributes);
            if (mEglDisplay == EGL_NO_DISPLAY) {
                throw std::exception("Failed to get EGL display");
            }
            if (eglInitialize(mEglDisplay, NULL, NULL) == EGL_FALSE) {
                // This tries to initialize EGL to D3D11 Feature Level 9_3, if 10_0+ is unavailable (e.g. on some mobile devices).
                mEglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, fl9_3DisplayAttributes);
                if (mEglDisplay == EGL_NO_DISPLAY) {
                    throw std::exception("Failed to get EGL display");
                }

                if (eglInitialize(mEglDisplay, NULL, NULL) == EGL_FALSE) {
                    // This initializes EGL to D3D11 Feature Level 11_0 on WARP, if 9_3+ is unavailable on the default GPU.
                    mEglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, warpDisplayAttributes);
                    if (mEglDisplay == EGL_NO_DISPLAY) {
                        throw std::exception("Failed to get EGL display");
                    }

                    if (eglInitialize(mEglDisplay, NULL, NULL) == EGL_FALSE) {
                        // If all of the calls to eglInitialize returned EGL_FALSE then an error has occurred.
                        throw std::exception("Failed to initialize EGL");
                    }
                }
            }
            mEglConfig = NULL;
            EGLint numConfigs = 0;
            if (eglGetConfigs(mEglDisplay, nullptr, 0, &numConfigs) == EGL_FALSE) {
                throw std::exception("Failed to get EGLConfig count");
            }
            if (eglChooseConfig(mEglDisplay, configAttributes, &mEglConfig, 1, &numConfigs) == EGL_FALSE) {
                throw std::exception("Failed to choose first EGLConfig");
            }
        }
    };
} // namespace

std::unique_ptr<engine::Scene> TryCreateOrbitScene(engine::Context& context) {
    return std::make_unique<OrbitScene>(context);
}
