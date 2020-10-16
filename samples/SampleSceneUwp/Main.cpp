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

#include <XrSceneLib/XrApp.h>
std::unique_ptr<engine::Scene> TryCreateTitleScene(engine::Context& context);
std::unique_ptr<engine::Scene> TryCreateOrbitScene(engine::Context& context);
std::unique_ptr<engine::Scene> TryCreateHandTrackingScene(engine::Context& context);

#include <Unknwn.h> // Required to interop with IUnknown. Must be included before C++/WinRT headers.
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.ApplicationModel.Preview.Holographic.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Text.Core.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.Graphics.Holographic.h>

namespace windows {
    using namespace winrt::Windows::ApplicationModel::Activation;
    using namespace winrt::Windows::ApplicationModel::Core;
    using namespace winrt::Windows::UI::Core;
    using namespace winrt::Windows::UI::Text::Core;
    using namespace winrt::Windows::UI::ViewManagement;
    using namespace winrt::Windows::Graphics::Holographic;
    using namespace winrt::Windows::ApplicationModel::Preview::Holographic;
} // namespace windows

namespace {
    std::unique_ptr<engine::XrApp> CreateUwpXrApp(XrHolographicWindowAttachmentMSFT&& holographicWindowAttachment) {
        engine::XrAppConfiguration appConfig({"SampleSceneUwp", 2});
        appConfig.HolographicWindowAttachment = std::move(holographicWindowAttachment);

        appConfig.RequestedExtensions.push_back(XR_EXT_WIN32_APPCONTAINER_COMPATIBLE_EXTENSION_NAME);
        appConfig.RequestedExtensions.push_back(XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME);
        appConfig.RequestedExtensions.push_back(XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME);
        appConfig.RequestedExtensions.push_back(XR_MSFT_HAND_INTERACTION_EXTENSION_NAME);
        appConfig.RequestedExtensions.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
        appConfig.RequestedExtensions.push_back(XR_MSFT_HAND_TRACKING_MESH_EXTENSION_NAME);

        auto app = CreateXrApp(appConfig);
        app->AddScene(TryCreateTitleScene(app->Context()));
        app->AddScene(TryCreateOrbitScene(app->Context()));
        app->AddScene(TryCreateHandTrackingScene(app->Context()));
        return app;
    }

    struct AppView : winrt::implements<AppView, windows::IFrameworkView> {
        void Initialize(windows::CoreApplicationView const& applicationView) {
            sample::Trace("IFrameworkView::Initialize");
            applicationView.Activated({this, &AppView::OnActivated});
        }

        void Load(winrt::hstring const& entryPoint) {
            sample::Trace("IFrameworkView::Load entryPoint : {}", winrt::to_string(entryPoint).c_str());
        }

        void Uninitialize() {
            sample::Trace("IFrameworkView::Uninitialize");
        }

        void OnActivated(windows::CoreApplicationView const&, windows::IActivatedEventArgs const& args) {
            if (args.Kind() == windows::ActivationKind::Protocol) {
                windows::ProtocolActivatedEventArgs eventArgs{args.as<windows::ProtocolActivatedEventArgs>()};
                sample::Trace("Protocol uri : {}", winrt::to_string(eventArgs.Uri().RawUri()).c_str());
            }

            // Inspecting whether the application is launched from within holographic shell or from desktop.
            if (windows::HolographicApplicationPreview::IsHolographicActivation(args)) {
                sample::Trace("App activation is targeted at the holographic shell.");
            } else {
                sample::Trace("App activation is targeted at the desktop.");
            }

            // NOTE: CoreWindow will be activated later after the HolographicSpace has been created.
        }

        void InitializeGL(windows::HolographicSpace holographicSpace) {
            const EGLint configAttributes[] = {
                EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 8, EGL_STENCIL_SIZE, 8, EGL_NONE};

            const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

            const EGLint surfaceAttributes[] = {
                // EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER is part of the same optimization as EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER
                // (see above). If you have compilation issues with it then please update your Visual Studio templates.
                // EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER,
                EGL_TRUE,
                EGL_NONE};

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
                EGL_TRUE,
                EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE,
                EGL_TRUE,
                EGL_NONE,
            };

            const EGLint warpDisplayAttributes[] = {
                // These attributes can be used to request D3D11 WARP.
                // They are used if eglInitialize fails with both the default display attributes and the 9_3 display attributes.
                EGL_PLATFORM_ANGLE_TYPE_ANGLE,
                EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
                EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,
                // EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE,
                // EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER,
                EGL_TRUE,
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
            auto mEglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, defaultDisplayAttributes);
            if (mEglDisplay == EGL_NO_DISPLAY) {
                std::cout << "please" << std::endl;
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
            EGLConfig config = NULL;
            EGLint numConfigs = 0;
            if (eglGetConfigs(mEglDisplay, nullptr, 0, &numConfigs) == EGL_FALSE) {
                throw std::exception("Failed to get EGLConfig count");
            }
            if (eglChooseConfig(mEglDisplay, configAttributes, &config, 1, &numConfigs) == EGL_FALSE) {
                throw std::exception("Failed to choose first EGLConfig");
            }

            winrt::Windows::Foundation::Collections::PropertySet surfaceProperties;
            surfaceProperties.Insert(EGLNativeWindowTypeProperty, holographicSpace);
            EGLNativeWindowType win = reinterpret_cast<EGLNativeWindowType>(&surfaceProperties);
            auto mEglSurface = eglCreateWindowSurface(mEglDisplay, config, win, surfaceAttributes);
            if (mEglSurface == EGL_NO_SURFACE) {
                throw std::exception("Failed to create EGL fullscreen surface");
            }
        }
        
        void Run() {
            sample::Trace("IFrameworkView::Run");

            // Creating a HolographicSpace before activating the CoreWindow to make it a holographic window
            windows::CoreWindow window = windows::CoreWindow::GetForCurrentThread();
            windows::HolographicSpace holographicSpace = windows::HolographicSpace::CreateForCoreWindow(window);

             InitializeGL(holographicSpace);
            
            window.Activate();

            XrHolographicWindowAttachmentMSFT holographicWindowAttachment{XR_TYPE_HOLOGRAPHIC_WINDOW_ATTACHMENT_MSFT};
            holographicWindowAttachment.coreWindow = window.as<::IUnknown>().get();
            holographicWindowAttachment.holographicSpace = holographicSpace.as<::IUnknown>().get();

            std::unique_ptr<engine::XrApp> app = CreateUwpXrApp(std::move(holographicWindowAttachment));

            while (!m_windowClosed && app->Step()) {
                window.Dispatcher().ProcessEvents(windows::CoreProcessEventsOption::ProcessAllIfPresent);
            }
        }

        void SetWindow(windows::CoreWindow const& window) {
            sample::Trace("IFrameworkView::SetWindow");

            InitializeTextEditingContext();
            window.KeyDown({this, &AppView::OnKeyDown});
            window.Closed({this, &AppView::OnWindowClosed});
        }

        void InitializeTextEditingContext() {
            // This sample customizes the text input pane with manual display policy and email address scope.
            windows::CoreTextServicesManager manager = windows::CoreTextServicesManager::GetForCurrentView();
            windows::CoreTextEditContext editingContext = manager.CreateEditContext();
            editingContext.InputPaneDisplayPolicy(windows::CoreTextInputPaneDisplayPolicy::Manual);
            editingContext.InputScope(windows::CoreTextInputScope::EmailAddress);
        }

        void OnKeyDown(windows::CoreWindow const& sender, windows::KeyEventArgs const& args) {
            sample::Trace("OnKeyDown : 0x{:x}", args.VirtualKey());

            // This sample toggles the software keyboard in HMD using space key
            if (args.VirtualKey() == winrt::Windows::System::VirtualKey::Space) {
                windows::InputPane inputPane = windows::InputPane::GetForCurrentView();
                if (inputPane.Visible()) {
                    const bool hidden = inputPane.TryHide();
                    sample::Trace("InputPane::TryHide() -> {}", hidden);
                } else {
                    const bool shown = inputPane.TryShow();
                    sample::Trace("InputPane::TryShow() -> {}", shown);
                }
            }
        }

        void OnWindowClosed(windows::CoreWindow const& sender, windows::CoreWindowEventArgs const& args) {
            m_windowClosed = true;
        }

    private:
        bool m_windowClosed{false};
    };

    struct AppViewSource : winrt::implements<AppViewSource, windows::IFrameworkViewSource> {
        windows::IFrameworkView CreateView() {
            return winrt::make<AppView>();
        }
    };
} // namespace

int APIENTRY wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
    windows::CoreApplication::Run(winrt::make<AppViewSource>());
}
