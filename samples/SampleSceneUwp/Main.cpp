#pragma comment(lib, "D3D11.lib")
#pragma comment(lib, "D3dcompiler.lib") // for shader compile
#pragma comment(lib, "Dxgi.lib")        // for CreateDXGIFactory1

// Tell OpenXR what platform code we'll be using
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <pch.h>
#include <d3d11.h>
#include <directxmath.h> // Matrix math functions and objects
#include <d3dcompiler.h> // For compiling shaders! D3DCompile
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <thread> // sleep_for
#include <vector>
#include <algorithm> // any_of
#include "SimpleRenderer.h"
using namespace std;
using namespace DirectX; // Matrix math

///////////////////////////////////////////

struct swapchain_surfdata_t {
    ID3D11DepthStencilView* depth_view;
    ID3D11RenderTargetView* target_view;
};

struct swapchain_t {
    XrSwapchain handle;
    int32_t width;
    int32_t height;
    vector<XrSwapchainImageD3D11KHR> surface_images;
    vector<swapchain_surfdata_t> surface_data;
};

struct input_state_t {
    XrActionSet actionSet;
    XrAction poseAction;
    XrAction selectAction;
    XrPath handSubactionPath[2];
    XrSpace handSpace[2];
    XrPosef handPose[2];
    XrBool32 renderHand[2];
    XrBool32 handSelect[2];
};

///////////////////////////////////////////

// Function pointers for some OpenXR extension methods we'll use.
PFN_xrGetD3D11GraphicsRequirementsKHR ext_xrGetD3D11GraphicsRequirementsKHR = nullptr;
PFN_xrCreateDebugUtilsMessengerEXT ext_xrCreateDebugUtilsMessengerEXT = nullptr;
PFN_xrDestroyDebugUtilsMessengerEXT ext_xrDestroyDebugUtilsMessengerEXT = nullptr;

///////////////////////////////////////////

struct app_transform_buffer_t {
    XMFLOAT4X4 world;
    XMFLOAT4X4 viewproj;
};

XrFormFactor app_config_form = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
XrViewConfigurationType app_config_view = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

ID3D11VertexShader* app_vshader;
ID3D11PixelShader* app_pshader;
ID3D11InputLayout* app_shader_layout;
ID3D11Buffer* app_constant_buffer;
ID3D11Buffer* app_vertex_buffer;
ID3D11Buffer* app_index_buffer;
ID3D11Buffer* quad_vertex_buffer;
ID3D11Buffer* quad_index_buffer;

vector<XrPosef> app_cubes;

void app_init();
void app_draw(XrCompositionLayerProjectionView& layerView);
void app_update();
void app_update_predicted();

///////////////////////////////////////////

const XrPosef xr_pose_identity = {{0, 0, 0, 1}, {0, 0, 0}};
XrInstance xr_instance = {};
XrSession xr_session = {};
XrSessionState xr_session_state = XR_SESSION_STATE_UNKNOWN;
bool xr_running = false;
XrSpace xr_app_space = {};
XrSystemId xr_system_id = XR_NULL_SYSTEM_ID;
input_state_t xr_input = {};
XrEnvironmentBlendMode xr_blend = {};
XrDebugUtilsMessengerEXT xr_debug = {};
EGLConfig mEglConfig;
EGLDisplay mEglDisplay;
EGLSurface mEglSurface;
SimpleRenderer* mCubeRenderer;
//vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> swap_texs;
vector<ID3D11Texture2D* > swap_texs;
vector<ID3D11ShaderResourceView*> shaderResourceViewMaps;
vector<ID3D11RenderTargetView*> render_target_views;



ID3D11SamplerState* m_sampleState;
vector<XrView> xr_views;
vector<XrViewConfigurationView> xr_config_views;
vector<swapchain_t> xr_swapchains;

bool openxr_init(const char* app_name, int64_t swapchain_format);
void openxr_make_actions();
void openxr_shutdown();
void openxr_poll_events(bool& exit);
void openxr_poll_actions();
void openxr_poll_predicted(XrTime predicted_time);
void openxr_render_frame();
bool openxr_render_layer(XrTime predictedTime,
                         vector<XrCompositionLayerProjectionView>& projectionViews,
                         XrCompositionLayerProjection& layer);

///////////////////////////////////////////

ID3D11Device* d3d_device = nullptr;
ID3D11DeviceContext* d3d_context = nullptr;
int64_t d3d_swapchain_fmt = DXGI_FORMAT_R8G8B8A8_UNORM;

bool d3d_init(LUID& adapter_luid);
void d3d_shutdown();
IDXGIAdapter1* d3d_get_adapter(LUID& adapter_luid);
swapchain_surfdata_t d3d_make_surface_data(XrBaseInStructure& swapchainImage);
void d3d_render_layer(XrCompositionLayerProjectionView& layerView,
                      swapchain_surfdata_t& surface,
                      ID3D11ShaderResourceView* shaderResourceViewMap);
void d3d_swapchain_destroy(swapchain_t& swapchain);
XMMATRIX d3d_xr_projection(XrFovf fov, float clip_near, float clip_far);
ID3DBlob* d3d_compile_shader(const char* hlsl, const char* entrypoint, const char* target);

///////////////////////////////////////////
/*
constexpr char app_shader_code[] = R"_(
cbuffer TransformBuffer : register(b0) {
	float4x4 world;
	float4x4 viewproj;
};
struct vsIn {
	float4 pos  : SV_POSITION;
	float3 norm : NORMAL;
};
struct psIn {
	float4 pos   : SV_POSITION;
	float3 color : COLOR0;
};

psIn vs(vsIn input) {
	psIn output;
	output.pos = mul(float4(input.pos.xyz, 1), world);
	output.pos = mul(output.pos, viewproj);

	float3 normal = normalize(mul(float4(input.norm, 0), world).xyz);

	output.color = saturate(dot(normal, float3(0,1,0))).xxx;
	return output;
}
float4 ps(psIn input) : SV_TARGET {
	return float4(input.color, 1);
})_";*/
constexpr char quad_shader_code[] = R"_(
Texture2D shaderTexture;
SamplerState SampleType;
cbuffer TransformBuffer : register(b0) {
	float4x4 world;
	float4x4 viewproj;
};
struct vsIn {
	float3 pos  : POS;
    float3 norm : NOR;
    //float2 tex: TEX;
};
struct psIn {
	float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

psIn vs(vsIn input) {
	psIn output;
	output.pos = mul(float4(input.pos.xyz, 1), world);
	output.pos = mul(output.pos, viewproj);
    output.tex = input.norm.xy;
	return output;
}
float4 ps(psIn input) : SV_TARGET {
    //return float4(input.tex, 1, 1);
	return shaderTexture.Sample(SampleType, input.tex);
})_";
float app_verts[] = {
    -1, -1, -1, -1, -1, -1,                                                                 // Bottom verts
    1,  -1, -1, 1,  -1, -1, 1, 1, -1, 1, 1, -1, -1, 1, -1, -1, 1, -1, -1, -1, 1, -1, -1, 1, // Top verts
    1,  -1, 1,  1,  -1, 1,  1, 1, 1,  1, 1, 1,  -1, 1, 1,  -1, 1, 1,
};

uint16_t app_inds[] = {
    1, 2, 0, 2, 3, 0, 4, 6, 5, 7, 6, 4, 6, 2, 1, 5, 6, 1, 3, 7, 4, 0, 3, 4, 4, 5, 1, 0, 4, 1, 2, 7, 3, 2, 6, 7,
};
float quad_verts[] = {
    -1, -1, .0f,    .0,.0,.0,    //.0,.0,
    -1.0, 1.0,.0f,   .0,1.0,.0,   //.0,1,
    1.0f, 1.0f,.0f,  1.0,1.0,.0,   //1.0,1.0,
    1.0f, -1.0f,.0f, 1.0,.0,.0,   // 1.0,.0,
};

uint16_t quad_inds[] = {
    0,1,3,1,2,3
};

///////////////////////////////////////////
// Main                                  //
///////////////////////////////////////////

int __stdcall wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    if (!openxr_init("Single file OpenXR", d3d_swapchain_fmt)) {
        d3d_shutdown();
        
        //MessageBox(nullptr, "OpenXR initialization failed\n", "Error", 1);
        return 1;
    }
    openxr_make_actions();
    app_init();

    bool quit = false;
    while (!quit) {
        openxr_poll_events(quit);

        if (xr_running) {
            openxr_poll_actions();
            app_update();
            openxr_render_frame();

            if (xr_session_state != XR_SESSION_STATE_VISIBLE && xr_session_state != XR_SESSION_STATE_FOCUSED) {
                this_thread::sleep_for(chrono::milliseconds(250));
            }
        }
    }

    openxr_shutdown();
    d3d_shutdown();
    return 0;
}

///////////////////////////////////////////
// OpenXR code                           //
///////////////////////////////////////////

bool openxr_init(const char* app_name, int64_t swapchain_format) {
    // OpenXR will fail to initialize if we ask for an extension that OpenXR
    // can't provide! So we need to check our all extensions before
    // initializing OpenXR with them. Note that even if the extension is
    // present, it's still possible you may not be able to use it. For
    // example: the hand tracking extension may be present, but the hand
    // sensor might not be plugged in or turned on. There are often
    // additional checks that should be made before using certain features!
    vector<const char*> use_extensions;
    const char* ask_extensions[] = {
        XR_KHR_D3D11_ENABLE_EXTENSION_NAME, // Use Direct3D11 for rendering
        XR_EXT_DEBUG_UTILS_EXTENSION_NAME,  // Debug utils for extra info
    };

    // We'll get a list of extensions that OpenXR provides using this
    // enumerate pattern. OpenXR often uses a two-call enumeration pattern
    // where the first call will tell you how much memory to allocate, and
    // the second call will provide you with the actual data!
    uint32_t ext_count = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &ext_count, nullptr);
    vector<XrExtensionProperties> xr_exts(ext_count, {XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, ext_count, &ext_count, xr_exts.data());

    printf("OpenXR extensions available:\n");
    for (size_t i = 0; i < xr_exts.size(); i++) {
        printf("- %s\n", xr_exts[i].extensionName);

        // Check if we're asking for this extensions, and add it to our use
        // list!
        for (int32_t ask = 0; ask < _countof(ask_extensions); ask++) {
            if (strcmp(ask_extensions[ask], xr_exts[i].extensionName) == 0) {
                use_extensions.push_back(ask_extensions[ask]);
                break;
            }
        }
    }
    // If a required extension isn't present, you want to ditch out here!
    // It's possible something like your rendering API might not be provided
    // by the active runtime. APIs like OpenGL don't have universal support.
    if (!std::any_of(use_extensions.begin(), use_extensions.end(), [](const char* ext) {
            return strcmp(ext, XR_KHR_D3D11_ENABLE_EXTENSION_NAME) == 0;
        }))
        return false;

    // Initialize OpenXR with the extensions we've found!
    XrInstanceCreateInfo createInfo = {XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.enabledExtensionCount = use_extensions.size();
    createInfo.enabledExtensionNames = use_extensions.data();
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    strcpy_s(createInfo.applicationInfo.applicationName, app_name);
    xrCreateInstance(&createInfo, &xr_instance);

    // Check if OpenXR is on this system, if this is null here, the user
    // needs to install an OpenXR runtime and ensure it's active!
    if (xr_instance == nullptr)
        return false;

    // Load extension methods that we'll need for this application! There's a
    // couple ways to do this, and this is a fairly manual one. Chek out this
    // file for another way to do it:
    // https://github.com/maluoi/StereoKit/blob/master/StereoKitC/systems/platform/openxr_extensions.h
    xrGetInstanceProcAddr(xr_instance, "xrCreateDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)(&ext_xrCreateDebugUtilsMessengerEXT));
    xrGetInstanceProcAddr(xr_instance, "xrDestroyDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)(&ext_xrDestroyDebugUtilsMessengerEXT));
    xrGetInstanceProcAddr(xr_instance, "xrGetD3D11GraphicsRequirementsKHR", (PFN_xrVoidFunction*)(&ext_xrGetD3D11GraphicsRequirementsKHR));

    // Set up a really verbose debug log! Great for dev, but turn this off or
    // down for final builds. WMR doesn't produce much output here, but it
    // may be more useful for other runtimes?
    // Here's some extra information about the message types and severities:
    // https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#debug-message-categorization
    XrDebugUtilsMessengerCreateInfoEXT debug_info = {XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug_info.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
    debug_info.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                   XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_info.userCallback = [](XrDebugUtilsMessageSeverityFlagsEXT severity,
                                 XrDebugUtilsMessageTypeFlagsEXT types,
                                 const XrDebugUtilsMessengerCallbackDataEXT* msg,
                                 void* user_data) {
        // Print the debug message we got! There's a bunch more info we could
        // add here too, but this is a pretty good start, and you can always
        // add a breakpoint this line!
        printf("%s: %s\n", msg->functionName, msg->message);

        // Output to debug window
        char text[512];
        sprintf_s(text, "%s: %s", msg->functionName, msg->message);
        OutputDebugStringA(text);

        // Returning XR_TRUE here will force the calling function to fail
        return (XrBool32)XR_FALSE;
    };
    // Start up the debug utils!
    if (ext_xrCreateDebugUtilsMessengerEXT)
        ext_xrCreateDebugUtilsMessengerEXT(xr_instance, &debug_info, &xr_debug);

    // Request a form factor from the device (HMD, Handheld, etc.)
    XrSystemGetInfo systemInfo = {XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = app_config_form;
    xrGetSystem(xr_instance, &systemInfo, &xr_system_id);

    // Check what blend mode is valid for this device (opaque vs transparent displays)
    // We'll just take the first one available!
    uint32_t blend_count = 0;
    xrEnumerateEnvironmentBlendModes(xr_instance, xr_system_id, app_config_view, 1, &blend_count, &xr_blend);

    // OpenXR wants to ensure apps are using the correct graphics card, so this MUST be called
    // before xrCreateSession. This is crucial on devices that have multiple graphics cards,
    // like laptops with integrated graphics chips in addition to dedicated graphics cards.
    XrGraphicsRequirementsD3D11KHR requirement = {XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    ext_xrGetD3D11GraphicsRequirementsKHR(xr_instance, xr_system_id, &requirement);
    if (!d3d_init(requirement.adapterLuid))
        return false;

    // A session represents this application's desire to display things! This is where we hook up our graphics API.
    // This does not start the session, for that, you'll need a call to xrBeginSession, which we do in openxr_poll_events
    XrGraphicsBindingD3D11KHR binding = {XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    binding.device = d3d_device;
    XrSessionCreateInfo sessionInfo = {XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next = &binding;
    sessionInfo.systemId = xr_system_id;
    xrCreateSession(xr_instance, &sessionInfo, &xr_session);

    // Unable to start a session, may not have an MR device attached or ready
    if (xr_session == nullptr)
        return false;

    // OpenXR uses a couple different types of reference frames for positioning content, we need to choose one for
    // displaying our content! STAGE would be relative to the center of your guardian system's bounds, and LOCAL
    // would be relative to your device's starting location. HoloLens doesn't have a STAGE, so we'll use LOCAL.
    XrReferenceSpaceCreateInfo ref_space = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    ref_space.poseInReferenceSpace = xr_pose_identity;
    ref_space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    xrCreateReferenceSpace(xr_session, &ref_space, &xr_app_space);

    // Now we need to find all the viewpoints we need to take care of! For a stereo headset, this should be 2.
    // Similarly, for an AR phone, we'll need 1, and a VR cave could have 6, or even 12!
    uint32_t view_count = 0;
    xrEnumerateViewConfigurationViews(xr_instance, xr_system_id, app_config_view, 0, &view_count, nullptr);
    xr_config_views.resize(view_count, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    xr_views.resize(view_count, {XR_TYPE_VIEW});
    xrEnumerateViewConfigurationViews(xr_instance, xr_system_id, app_config_view, view_count, &view_count, xr_config_views.data());
    for (uint32_t i = 0; i < view_count; i++) {
        // Create a swapchain for this viewpoint! A swapchain is a set of texture buffers used for displaying to screen,
        // typically this is a backbuffer and a front buffer, one for rendering data to, and one for displaying on-screen.
        // A note about swapchain image format here! OpenXR doesn't create a concrete image format for the texture, like
        // DXGI_FORMAT_R8G8B8A8_UNORM. Instead, it switches to the TYPELESS variant of the provided texture format, like
        // DXGI_FORMAT_R8G8B8A8_TYPELESS. When creating an ID3D11RenderTargetView for the swapchain texture, we must specify
        // a concrete type like DXGI_FORMAT_R8G8B8A8_UNORM, as attempting to create a TYPELESS view will throw errors, so
        // we do need to store the format separately and remember it later.
        XrViewConfigurationView& view = xr_config_views[i];
        XrSwapchainCreateInfo swapchain_info = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
        XrSwapchain handle;
        swapchain_info.arraySize = 1;
        swapchain_info.mipCount = 1;
        swapchain_info.faceCount = 1;
        swapchain_info.format = swapchain_format;
        swapchain_info.width = view.recommendedImageRectWidth;
        swapchain_info.height = view.recommendedImageRectHeight;
        swapchain_info.sampleCount = view.recommendedSwapchainSampleCount;
        swapchain_info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        xrCreateSwapchain(xr_session, &swapchain_info, &handle);

        // Find out how many textures were generated for the swapchain
        uint32_t surface_count = 0;
        xrEnumerateSwapchainImages(handle, 0, &surface_count, nullptr);

        // We'll want to track our own information about the swapchain, so we can draw stuff onto it! We'll also create
        // a depth buffer for each generated texture here as well with make_surfacedata.
        swapchain_t swapchain = {};
        swapchain.width = swapchain_info.width;
        swapchain.height = swapchain_info.height;
        swapchain.handle = handle;
        swapchain.surface_images.resize(surface_count, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        swapchain.surface_data.resize(surface_count);
        xrEnumerateSwapchainImages(
            swapchain.handle, surface_count, &surface_count, (XrSwapchainImageBaseHeader*)swapchain.surface_images.data());
        for (uint32_t i = 0; i < surface_count; i++) {
            swapchain.surface_data[i] = d3d_make_surface_data((XrBaseInStructure&)swapchain.surface_images[i]);
        }
        xr_swapchains.push_back(swapchain);
    }

    return true;
}

///////////////////////////////////////////

void openxr_make_actions() {
    XrActionSetCreateInfo actionset_info = {XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy_s(actionset_info.actionSetName, "gameplay");
    strcpy_s(actionset_info.localizedActionSetName, "Gameplay");
    xrCreateActionSet(xr_instance, &actionset_info, &xr_input.actionSet);
    xrStringToPath(xr_instance, "/user/hand/left", &xr_input.handSubactionPath[0]);
    xrStringToPath(xr_instance, "/user/hand/right", &xr_input.handSubactionPath[1]);

    // Create an action to track the position and orientation of the hands! This is
    // the controller location, or the center of the palms for actual hands.
    XrActionCreateInfo action_info = {XR_TYPE_ACTION_CREATE_INFO};
    action_info.countSubactionPaths = _countof(xr_input.handSubactionPath);
    action_info.subactionPaths = xr_input.handSubactionPath;
    action_info.actionType = XR_ACTION_TYPE_POSE_INPUT;
    strcpy_s(action_info.actionName, "hand_pose");
    strcpy_s(action_info.localizedActionName, "Hand Pose");
    xrCreateAction(xr_input.actionSet, &action_info, &xr_input.poseAction);

    // Create an action for listening to the select action! This is primary trigger
    // on controllers, and an airtap on HoloLens
    action_info.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
    strcpy_s(action_info.actionName, "select");
    strcpy_s(action_info.localizedActionName, "Select");
    xrCreateAction(xr_input.actionSet, &action_info, &xr_input.selectAction);

    // Bind the actions we just created to specific locations on the Khronos simple_controller
    // definition! These are labeled as 'suggested' because they may be overridden by the runtime
    // preferences. For example, if the runtime allows you to remap buttons, or provides input
    // accessibility settings.
    XrPath profile_path;
    XrPath pose_path[2];
    XrPath select_path[2];
    xrStringToPath(xr_instance, "/user/hand/left/input/grip/pose", &pose_path[0]);
    xrStringToPath(xr_instance, "/user/hand/right/input/grip/pose", &pose_path[1]);
    xrStringToPath(xr_instance, "/user/hand/left/input/select/click", &select_path[0]);
    xrStringToPath(xr_instance, "/user/hand/right/input/select/click", &select_path[1]);
    xrStringToPath(xr_instance, "/interaction_profiles/khr/simple_controller", &profile_path);
    XrActionSuggestedBinding bindings[] = {
        {xr_input.poseAction, pose_path[0]},
        {xr_input.poseAction, pose_path[1]},
        {xr_input.selectAction, select_path[0]},
        {xr_input.selectAction, select_path[1]},
    };
    XrInteractionProfileSuggestedBinding suggested_binds = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggested_binds.interactionProfile = profile_path;
    suggested_binds.suggestedBindings = &bindings[0];
    suggested_binds.countSuggestedBindings = _countof(bindings);
    xrSuggestInteractionProfileBindings(xr_instance, &suggested_binds);

    // Create frames of reference for the pose actions
    for (int32_t i = 0; i < 2; i++) {
        XrActionSpaceCreateInfo action_space_info = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
        action_space_info.action = xr_input.poseAction;
        action_space_info.poseInActionSpace = xr_pose_identity;
        action_space_info.subactionPath = xr_input.handSubactionPath[i];
        xrCreateActionSpace(xr_session, &action_space_info, &xr_input.handSpace[i]);
    }

    // Attach the action set we just made to the session
    XrSessionActionSetsAttachInfo attach_info = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attach_info.countActionSets = 1;
    attach_info.actionSets = &xr_input.actionSet;
    xrAttachSessionActionSets(xr_session, &attach_info);
}

///////////////////////////////////////////

void openxr_shutdown() {
    // We used a graphics API to initialize the swapchain data, so we'll
    // give it a chance to release anythig here!
    for (int32_t i = 0; i < xr_swapchains.size(); i++) {
        xrDestroySwapchain(xr_swapchains[i].handle);
        d3d_swapchain_destroy(xr_swapchains[i]);
    }
    xr_swapchains.clear();

    // Release all the other OpenXR resources that we've created!
    // What gets allocated, must get deallocated!
    if (xr_input.actionSet != XR_NULL_HANDLE) {
        if (xr_input.handSpace[0] != XR_NULL_HANDLE)
            xrDestroySpace(xr_input.handSpace[0]);
        if (xr_input.handSpace[1] != XR_NULL_HANDLE)
            xrDestroySpace(xr_input.handSpace[1]);
        xrDestroyActionSet(xr_input.actionSet);
    }
    if (xr_app_space != XR_NULL_HANDLE)
        xrDestroySpace(xr_app_space);
    if (xr_session != XR_NULL_HANDLE)
        xrDestroySession(xr_session);
    if (xr_debug != XR_NULL_HANDLE)
        ext_xrDestroyDebugUtilsMessengerEXT(xr_debug);
    if (xr_instance != XR_NULL_HANDLE)
        xrDestroyInstance(xr_instance);
}

///////////////////////////////////////////

void openxr_poll_events(bool& exit) {
    exit = false;

    XrEventDataBuffer event_buffer = {XR_TYPE_EVENT_DATA_BUFFER};

    while (xrPollEvent(xr_instance, &event_buffer) == XR_SUCCESS) {
        switch (event_buffer.type) {
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            XrEventDataSessionStateChanged* changed = (XrEventDataSessionStateChanged*)&event_buffer;
            xr_session_state = changed->state;

            // Session state change is where we can begin and end sessions, as well as find quit messages!
            switch (xr_session_state) {
            case XR_SESSION_STATE_READY: {
                XrSessionBeginInfo begin_info = {XR_TYPE_SESSION_BEGIN_INFO};
                begin_info.primaryViewConfigurationType = app_config_view;
                xrBeginSession(xr_session, &begin_info);
                xr_running = true;
            } break;
            case XR_SESSION_STATE_STOPPING: {
                xr_running = false;
                xrEndSession(xr_session);
            } break;
            case XR_SESSION_STATE_EXITING:
                exit = true;
                break;
            case XR_SESSION_STATE_LOSS_PENDING:
                exit = true;
                break;
            }
        } break;
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
            exit = true;
            return;
        }
        event_buffer = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

///////////////////////////////////////////

void openxr_poll_actions() {
    if (xr_session_state != XR_SESSION_STATE_FOCUSED)
        return;

    // Update our action set with up-to-date input data!
    XrActiveActionSet action_set = {};
    action_set.actionSet = xr_input.actionSet;
    action_set.subactionPath = XR_NULL_PATH;

    XrActionsSyncInfo sync_info = {XR_TYPE_ACTIONS_SYNC_INFO};
    sync_info.countActiveActionSets = 1;
    sync_info.activeActionSets = &action_set;

    xrSyncActions(xr_session, &sync_info);

    // Now we'll get the current states of our actions, and store them for later use
    for (uint32_t hand = 0; hand < 2; hand++) {
        XrActionStateGetInfo get_info = {XR_TYPE_ACTION_STATE_GET_INFO};
        get_info.subactionPath = xr_input.handSubactionPath[hand];

        XrActionStatePose pose_state = {XR_TYPE_ACTION_STATE_POSE};
        get_info.action = xr_input.poseAction;
        xrGetActionStatePose(xr_session, &get_info, &pose_state);
        xr_input.renderHand[hand] = pose_state.isActive;

        // Events come with a timestamp
        XrActionStateBoolean select_state = {XR_TYPE_ACTION_STATE_BOOLEAN};
        get_info.action = xr_input.selectAction;
        xrGetActionStateBoolean(xr_session, &get_info, &select_state);
        xr_input.handSelect[hand] = select_state.currentState && select_state.changedSinceLastSync;

        // If we have a select event, update the hand pose to match the event's timestamp
        if (xr_input.handSelect[hand]) {
            XrSpaceLocation space_location = {XR_TYPE_SPACE_LOCATION};
            XrResult res = xrLocateSpace(xr_input.handSpace[hand], xr_app_space, select_state.lastChangeTime, &space_location);
            if (XR_UNQUALIFIED_SUCCESS(res) && (space_location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                (space_location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
                xr_input.handPose[hand] = space_location.pose;
            }
        }
    }
}

///////////////////////////////////////////

void openxr_poll_predicted(XrTime predicted_time) {
    if (xr_session_state != XR_SESSION_STATE_FOCUSED)
        return;

    // Update hand position based on the predicted time of when the frame will be rendered! This
    // should result in a more accurate location, and reduce perceived lag.
    for (size_t i = 0; i < 2; i++) {
        if (!xr_input.renderHand[i])
            continue;
        XrSpaceLocation spaceRelation = {XR_TYPE_SPACE_LOCATION};
        XrResult res = xrLocateSpace(xr_input.handSpace[i], xr_app_space, predicted_time, &spaceRelation);
        if (XR_UNQUALIFIED_SUCCESS(res) && (spaceRelation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
            (spaceRelation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
            xr_input.handPose[i] = spaceRelation.pose;
        }
    }
}

///////////////////////////////////////////

void openxr_render_frame() {
    // Block until the previous frame is finished displaying, and is ready for another one.
    // Also returns a prediction of when the next frame will be displayed, for use with predicting
    // locations of controllers, viewpoints, etc.
    XrFrameState frame_state = {XR_TYPE_FRAME_STATE};
    xrWaitFrame(xr_session, nullptr, &frame_state);
    // Must be called before any rendering is done! This can return some interesting flags, like
    // XR_SESSION_VISIBILITY_UNAVAILABLE, which means we could skip rendering this frame and call
    // xrEndFrame right away.
    xrBeginFrame(xr_session, nullptr);

    // Execute any code that's dependant on the predicted time, such as updating the location of
    // controller models.
    openxr_poll_predicted(frame_state.predictedDisplayTime);
    app_update_predicted();

    // If the session is active, lets render our layer in the compositor!
    XrCompositionLayerBaseHeader* layer = nullptr;
    XrCompositionLayerProjection layer_proj = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    vector<XrCompositionLayerProjectionView> views;
    bool session_active = xr_session_state == XR_SESSION_STATE_VISIBLE || xr_session_state == XR_SESSION_STATE_FOCUSED;
    if (session_active && openxr_render_layer(frame_state.predictedDisplayTime, views, layer_proj)) {
        layer = (XrCompositionLayerBaseHeader*)&layer_proj;
    }

    // We're finished with rendering our layer, so send it off for display!
    XrFrameEndInfo end_info{XR_TYPE_FRAME_END_INFO};
    end_info.displayTime = frame_state.predictedDisplayTime;
    end_info.environmentBlendMode = xr_blend;
    end_info.layerCount = layer == nullptr ? 0 : 1;
    end_info.layers = &layer;
    xrEndFrame(xr_session, &end_info);
}
void check_pixel_d3d(int id, uint32_t img_id) {
    auto mOffscreenSurfaceD3D11Texture = swap_texs[id];
    D3D11_TEXTURE2D_DESC textureDesc = {0};
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    mOffscreenSurfaceD3D11Texture->GetDesc(&textureDesc);
    mOffscreenSurfaceD3D11Texture->GetDevice(&device);
    device->GetImmediateContext(&context);
    ID3D11Texture2D* cpuTexture = nullptr;
    SUCCEEDED(device->CreateTexture2D(&textureDesc, nullptr, &cpuTexture));

    context->CopyResource(cpuTexture, mOffscreenSurfaceD3D11Texture);

    D3D11_MAPPED_SUBRESOURCE mappedSubresource;
    context->Map(cpuTexture, 0, D3D11_MAP_READ, 0, &mappedSubresource);
    
}
    ///////////////////////////////////////////
void render_to_texture(int id, uint32_t img_id) {
    d3d_context->OMSetRenderTargets(1, &render_target_views[id], xr_swapchains[id].surface_data[img_id].depth_view);
    mCubeRenderer->Draw();
    //check pixel using d3d
    //check_pixel_d3d(id, img_id);
    d3d_context->OMSetRenderTargets(1, &xr_swapchains[id].surface_data[img_id].target_view, xr_swapchains[id].surface_data[img_id].depth_view);
}
bool openxr_render_layer(XrTime predictedTime, vector<XrCompositionLayerProjectionView>& views, XrCompositionLayerProjection& layer) {
    // Find the state and location of each viewpoint at the predicted time
    uint32_t view_count = 0;
    XrViewState view_state = {XR_TYPE_VIEW_STATE};
    XrViewLocateInfo locate_info = {XR_TYPE_VIEW_LOCATE_INFO};
    locate_info.viewConfigurationType = app_config_view;
    locate_info.displayTime = predictedTime;
    locate_info.space = xr_app_space;
    xrLocateViews(xr_session, &locate_info, &view_state, (uint32_t)xr_views.size(), &view_count, xr_views.data());
    views.resize(view_count);

    // And now we'll iterate through each viewpoint, and render it!
    for (uint32_t i = 0; i < view_count; i++) {

        
        // We need to ask which swapchain image to use for rendering! Which one will we get?
        // Who knows! It's up to the runtime to decide.
        uint32_t img_id;
        XrSwapchainImageAcquireInfo acquire_info = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        xrAcquireSwapchainImage(xr_swapchains[i].handle, &acquire_info, &img_id);
        
        // Wait until the image is available to render to. The compositor could still be
        // reading from it.
        XrSwapchainImageWaitInfo wait_info = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wait_info.timeout = XR_INFINITE_DURATION;
        xrWaitSwapchainImage(xr_swapchains[i].handle, &wait_info);

        // Set up our rendering information for the viewpoint we're using right now!
        views[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        views[i].pose = xr_views[i].pose;
        views[i].fov = xr_views[i].fov;
        views[i].subImage.swapchain = xr_swapchains[i].handle;
        views[i].subImage.imageRect.offset = {0, 0};
        views[i].subImage.imageRect.extent = {xr_swapchains[i].width, xr_swapchains[i].height};
        render_to_texture(i, img_id);
        // Call the rendering callback with our view and swapchain info
        d3d_render_layer(views[i], xr_swapchains[i].surface_data[img_id], shaderResourceViewMaps[i]);

        // And tell OpenXR we're done with rendering to this one!
        XrSwapchainImageReleaseInfo release_info = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        xrReleaseSwapchainImage(xr_swapchains[i].handle, &release_info);
    }

    layer.space = xr_app_space;
    layer.viewCount = (uint32_t)views.size();
    layer.views = views.data();
    return true;
}

///////////////////////////////////////////
// DirectX code                          //
///////////////////////////////////////////

bool d3d_init(LUID& adapter_luid) {
    IDXGIAdapter1* adapter = d3d_get_adapter(adapter_luid);
    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_0};

    if (adapter == nullptr)
        return false;
    if (FAILED(D3D11CreateDevice(adapter,
                                 D3D_DRIVER_TYPE_UNKNOWN,
                                 0,
                                 0,
                                 featureLevels,
                                 _countof(featureLevels),
                                 D3D11_SDK_VERSION,
                                 &d3d_device,
                                 nullptr,
                                 &d3d_context)))
        return false;

    adapter->Release();
    return true;
}

///////////////////////////////////////////

IDXGIAdapter1* d3d_get_adapter(LUID& adapter_luid) {
    // Turn the LUID into a specific graphics device adapter
    IDXGIAdapter1* final_adapter = nullptr;
    IDXGIAdapter1* curr_adapter = nullptr;
    IDXGIFactory1* dxgi_factory;
    DXGI_ADAPTER_DESC1 adapter_desc;

    CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)(&dxgi_factory));

    int curr = 0;
    while (dxgi_factory->EnumAdapters1(curr++, &curr_adapter) == S_OK) {
        curr_adapter->GetDesc1(&adapter_desc);

        if (memcmp(&adapter_desc.AdapterLuid, &adapter_luid, sizeof(&adapter_luid)) == 0) {
            final_adapter = curr_adapter;
            break;
        }
        curr_adapter->Release();
        curr_adapter = nullptr;
    }
    dxgi_factory->Release();
    return final_adapter;
}

///////////////////////////////////////////

void d3d_shutdown() {
    if (d3d_context) {
        d3d_context->Release();
        d3d_context = nullptr;
    }
    if (d3d_device) {
        d3d_device->Release();
        d3d_device = nullptr;
    }
}

///////////////////////////////////////////
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
HANDLE GetHandle(ID3D11Texture2D* D3D11Texture2D) {
    IDXGIResource* DXGIResource;

    const HRESULT DXGIResourceResult = D3D11Texture2D->QueryInterface(__uuidof(IDXGIResource), reinterpret_cast<void**>(&DXGIResource));

    if (FAILED(DXGIResourceResult)) {
        return 0;
    }

    HANDLE SharedHandle;
    const HRESULT SharedHandleResult = DXGIResource->GetSharedHandle(&SharedHandle);
    return SharedHandle;
    // DXGIResource->Release();

    // if (FAILED(SharedHandleResult)) {
    //    return 0;
    //}
    // return HandleToULong(SharedHandle);

    /*Microsoft::WRL::ComPtr<IDXGIResource> dxgiResource;
    HANDLE sharedHandle;
    auto hr = mtex.As(&dxgiResource);
    if FAILED (hr) {
        // error handling code
    }

    hr = dxgiResource->GetSharedHandle(&sharedHandle);
    if FAILED (hr) {
        // error handling code
    }

    hr = dxgiResource->GetSharedHandle(&sharedHandle);
    return sharedHandle;*/
}

void InitializeEGL(ID3D11Texture2D* d3dTex, int width, int height) {
    HANDLE sharedHandle = GetHandle(d3dTex);
    mEglSurface = EGL_NO_SURFACE;

    initialize_gl_contex();
    EGLint pBufferAttributes[] = {
        EGL_WIDTH, width, EGL_HEIGHT, height, EGL_TEXTURE_TARGET, EGL_TEXTURE_2D, EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA, EGL_NONE};

    mEglSurface =
        eglCreatePbufferFromClientBuffer(mEglDisplay, EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE, sharedHandle, mEglConfig, pBufferAttributes);
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
}

swapchain_surfdata_t d3d_make_surface_data(XrBaseInStructure& swapchain_img) {
    swapchain_surfdata_t result = {};

    // Get information about the swapchain image that OpenXR made for us!
    XrSwapchainImageD3D11KHR& d3d_swapchain_img = (XrSwapchainImageD3D11KHR&)swapchain_img;
    D3D11_TEXTURE2D_DESC texDesc;
    d3d_swapchain_img.texture->GetDesc(&texDesc);

    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.Usage = D3D11_USAGE_DYNAMIC;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    ID3D11Texture2D* mOffscreenSurfaceD3D11Texture;

    HRESULT hr = d3d_device->CreateTexture2D(&texDesc, nullptr, &mOffscreenSurfaceD3D11Texture);
    InitializeEGL(mOffscreenSurfaceD3D11Texture, texDesc.Width, texDesc.Height);

    swap_texs.push_back(mOffscreenSurfaceD3D11Texture);

    D3D11_BOX destRegion;
    destRegion.left = 120;
    destRegion.right = 200;
    destRegion.top = 100;
    destRegion.bottom = 220;
    destRegion.front = 0;
    destRegion.back = 1;
    // Set the row pitch of the targa image data.
    auto rowPitch = (texDesc.Width * 4) * sizeof(unsigned char);
    auto imageSize = texDesc.Width * texDesc.Height * 4;
    unsigned char* m_targaData = new unsigned char[imageSize];
    for (int i = 0; i < imageSize; i += 4) {
        m_targaData[i] = (unsigned char)255;
        m_targaData[i+1] = 0;
        m_targaData[i+2] = 0;
        m_targaData[i + 3] = (unsigned char)255;

    }
    d3d_context->UpdateSubresource(swap_texs.back(), 0, NULL, m_targaData, rowPitch, 0);
    //mCubeRenderer->UpdateWindowSize(0,0,texDesc.Width, texDesc.Height);
    // Create the shader resource view.
    ID3D11ShaderResourceView* shaderResourceViewMap;
    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
    shaderResourceViewDesc.Format = texDesc.Format;
    shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
    shaderResourceViewDesc.Texture2D.MipLevels = 1;
    d3d_device->CreateShaderResourceView(swap_texs.back(), &shaderResourceViewDesc, &shaderResourceViewMap);
    shaderResourceViewMaps.push_back(shaderResourceViewMap);
    ID3D11RenderTargetView* m_renderTargetView;
    // Setup the description of the render target view.
    D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
    
    renderTargetViewDesc.Format = texDesc.Format;
    renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    renderTargetViewDesc.Texture2D.MipSlice = 0;

    auto mhr = d3d_device->CreateRenderTargetView(mOffscreenSurfaceD3D11Texture, &renderTargetViewDesc, &m_renderTargetView);
    
    render_target_views.push_back(m_renderTargetView);

    // Create a view resource for the swapchain image target that we can use to set up rendering.
    D3D11_RENDER_TARGET_VIEW_DESC target_desc = {};
    target_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    // NOTE: Why not use color_desc.Format? Check the notes over near the xrCreateSwapchain call!
    // Basically, the color_desc.Format of the OpenXR created swapchain is TYPELESS, but in order to
    // create a View for the texture, we need a concrete variant of the texture format like UNORM.
    target_desc.Format = (DXGI_FORMAT)d3d_swapchain_fmt;
    d3d_device->CreateRenderTargetView(d3d_swapchain_img.texture, &target_desc, &result.target_view);
    // Create a depth buffer that matches
    ID3D11Texture2D* depth_texture;
    D3D11_TEXTURE2D_DESC depth_desc = {};
    depth_desc.SampleDesc.Count = 1;
    depth_desc.MipLevels = 1;
    depth_desc.Width = texDesc.Width;
    depth_desc.Height = texDesc.Height;
    depth_desc.ArraySize = texDesc.ArraySize;
    depth_desc.Format = DXGI_FORMAT_R32_TYPELESS;
    depth_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
    d3d_device->CreateTexture2D(&depth_desc, nullptr, &depth_texture);

    // And create a view resource for the depth buffer, so we can set that up for rendering to as well!
    D3D11_DEPTH_STENCIL_VIEW_DESC stencil_desc = {};
    stencil_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    stencil_desc.Format = DXGI_FORMAT_D32_FLOAT;
    d3d_device->CreateDepthStencilView(depth_texture, &stencil_desc, &result.depth_view);

    // We don't need direct access to the ID3D11Texture2D object anymore, we only need the view
    depth_texture->Release();

    return result;
}

///////////////////////////////////////////

void d3d_render_layer(XrCompositionLayerProjectionView& view,
                      swapchain_surfdata_t& surface,
                      ID3D11ShaderResourceView* shaderResourceViewMap) {
    // Set up where on the render target we want to draw, the view has a
    XrRect2Di& rect = view.subImage.imageRect;
    D3D11_VIEWPORT viewport =
        CD3D11_VIEWPORT((float)rect.offset.x, (float)rect.offset.y, (float)rect.extent.width, (float)rect.extent.height);
    d3d_context->RSSetViewports(1, &viewport);
    //mCubeRenderer->UpdateWindowSize((int)rect.offset.x, (int)rect.offset.y, (float)rect.extent.width, (float)rect.extent.height);
    
    
    // Wipe our swapchain color and depth target clean, and then set them up for rendering!
    float clear[] = {0, 0, 0, 1};
    d3d_context->ClearRenderTargetView(surface.target_view, clear);
    d3d_context->ClearDepthStencilView(surface.depth_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    d3d_context->OMSetRenderTargets(1, &surface.target_view, surface.depth_view);
    d3d_context->PSSetShaderResources(0, 1, &shaderResourceViewMap);
    // And now that we're set up, pass on the rest of our rendering to the application
    app_draw(view);
}

///////////////////////////////////////////

void d3d_swapchain_destroy(swapchain_t& swapchain) {
    for (uint32_t i = 0; i < swapchain.surface_data.size(); i++) {
        swapchain.surface_data[i].depth_view->Release();
        swapchain.surface_data[i].target_view->Release();
    }
}

///////////////////////////////////////////

XMMATRIX d3d_xr_projection(XrFovf fov, float clip_near, float clip_far) {
    const float left = clip_near * tanf(fov.angleLeft);
    const float right = clip_near * tanf(fov.angleRight);
    const float down = clip_near * tanf(fov.angleDown);
    const float up = clip_near * tanf(fov.angleUp);

    return XMMatrixPerspectiveOffCenterRH(left, right, down, up, clip_near, clip_far);
}

///////////////////////////////////////////

ID3DBlob* d3d_compile_shader(const char* hlsl, const char* entrypoint, const char* target) {
    DWORD flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ID3DBlob *compiled, *errors;
    if (FAILED(D3DCompile(hlsl, strlen(hlsl), nullptr, nullptr, nullptr, entrypoint, target, flags, 0, &compiled, &errors)))
        printf("Error: D3DCompile failed %s", (char*)errors->GetBufferPointer());
    if (errors)
        errors->Release();

    return compiled;
}

///////////////////////////////////////////
// App                                   //
///////////////////////////////////////////

void app_init() {
    // Compile our shader code, and turn it into a shader resource!
    ID3DBlob* vert_shader_blob = d3d_compile_shader(quad_shader_code, "vs", "vs_5_0");
    ID3DBlob* pixel_shader_blob = d3d_compile_shader(quad_shader_code, "ps", "ps_5_0");
    d3d_device->CreateVertexShader(vert_shader_blob->GetBufferPointer(), vert_shader_blob->GetBufferSize(), nullptr, &app_vshader);
    d3d_device->CreatePixelShader(pixel_shader_blob->GetBufferPointer(), pixel_shader_blob->GetBufferSize(), nullptr, &app_pshader);

    // Describe how our mesh is laid out in memory
    D3D11_INPUT_ELEMENT_DESC vert_desc[] = {
        {"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        //{"TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    d3d_device->CreateInputLayout(vert_desc, ARRAYSIZE(vert_desc),
                                  vert_shader_blob->GetBufferPointer(),
                                  vert_shader_blob->GetBufferSize(),
                                  &app_shader_layout);

    // Create GPU resources for our mesh's vertices and indices! Constant buffers are for passing transform
    // matrices into the shaders, so make a buffer for them too!
    /*D3D11_SUBRESOURCE_DATA vert_buff_data = {app_verts};
    D3D11_SUBRESOURCE_DATA ind_buff_data = {app_inds};
    CD3D11_BUFFER_DESC vert_buff_desc(sizeof(app_verts), D3D11_BIND_VERTEX_BUFFER);
    CD3D11_BUFFER_DESC ind_buff_desc(sizeof(app_inds), D3D11_BIND_INDEX_BUFFER);*/
    
     D3D11_SUBRESOURCE_DATA vert_buff_data = {quad_verts};
     D3D11_SUBRESOURCE_DATA ind_buff_data = {quad_inds};
     CD3D11_BUFFER_DESC vert_buff_desc(sizeof(quad_verts), D3D11_BIND_VERTEX_BUFFER);
     CD3D11_BUFFER_DESC ind_buff_desc(sizeof(quad_inds), D3D11_BIND_INDEX_BUFFER);

    CD3D11_BUFFER_DESC const_buff_desc(sizeof(app_transform_buffer_t), D3D11_BIND_CONSTANT_BUFFER);
    d3d_device->CreateBuffer(&vert_buff_desc, &vert_buff_data, &app_vertex_buffer);
    d3d_device->CreateBuffer(&ind_buff_desc, &ind_buff_data, &app_index_buffer);
    d3d_device->CreateBuffer(&const_buff_desc, nullptr, &app_constant_buffer);

    D3D11_SAMPLER_DESC samplerDesc;
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samplerDesc.BorderColor[0] = 0;
    samplerDesc.BorderColor[1] = 0;
    samplerDesc.BorderColor[2] = 0;
    samplerDesc.BorderColor[3] = 0;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    // Create the texture sampler state.
    auto result = d3d_device->CreateSamplerState(&samplerDesc, &m_sampleState);
    if (FAILED(result)) {
        throw std::exception("fail to create sampler state");
    }
    
    //D3D11_SUBRESOURCE_DATA vert_buff_data = {quad_verts};
    //D3D11_SUBRESOURCE_DATA ind_buff_data = {quad_inds};
    //CD3D11_BUFFER_DESC vert_buff_desc(sizeof(quad_verts), D3D11_BIND_VERTEX_BUFFER);
    //CD3D11_BUFFER_DESC ind_buff_desc(sizeof(quad_inds), D3D11_BIND_INDEX_BUFFER);
    //CD3D11_BUFFER_DESC const_buff_desc(sizeof(app_transform_buffer_t), D3D11_BIND_CONSTANT_BUFFER);
    //d3d_device->CreateBuffer(&vert_buff_desc, &vert_buff_data, &quad_vertex_buffer);
    //d3d_device->CreateBuffer(&ind_buff_desc, &ind_buff_data, &quad_index_buffer);

}

///////////////////////////////////////////

void app_draw(XrCompositionLayerProjectionView& view) {
    XMMATRIX mat_projection = d3d_xr_projection(view.fov, 0.05f, 100.0f);
    XMFLOAT4X4 fView;
    XMStoreFloat4x4(&fView, mat_projection);
    float fView_11 = fView._11;

   MathHelper::Matrix4 projectionMatrix(
       fView._11, fView._12, fView._13, fView._13, 
       fView._21, fView._22, fView._23, fView._24,
                                         fView._31,
                                         fView._32,
                                         fView._33,
                                         fView._34,
                                         fView._41,
                                         fView._42,
                                         fView._43,
                                         fView._44
   );

    //mCubeRenderer->Draw(projectionMatrix);
   //mCubeRenderer->Draw();

    // Set up camera matrices based on OpenXR's predicted viewpoint information
    XMMATRIX mat_view = XMMatrixInverse(nullptr,
                                        XMMatrixAffineTransformation(DirectX::g_XMOne,
                                                                     DirectX::g_XMZero,
                                                                     XMLoadFloat4((XMFLOAT4*)&view.pose.orientation),
                                                                     XMLoadFloat3((XMFLOAT3*)&view.pose.position)));

    // Set the active shaders and constant buffers.
    d3d_context->VSSetConstantBuffers(0, 1, &app_constant_buffer);
    d3d_context->VSSetShader(app_vshader, nullptr, 0);
    d3d_context->PSSetShader(app_pshader, nullptr, 0);

    // Set up the cube mesh's information
    UINT strides[] = {sizeof(float) * 6};
    UINT offsets[] = {0};
    d3d_context->IASetVertexBuffers(0, 1, &app_vertex_buffer, strides, offsets);
    d3d_context->IASetIndexBuffer(app_index_buffer, DXGI_FORMAT_R16_UINT, 0);
    d3d_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    d3d_context->IASetInputLayout(app_shader_layout);

    // Put camera matrices into the shader's constant buffer
    app_transform_buffer_t transform_buffer;
    XMStoreFloat4x4(&transform_buffer.viewproj, XMMatrixTranspose(mat_view * mat_projection));

    // Draw all the cubes we have in our list!
    for (size_t i = 0; i < app_cubes.size(); i++) {
        // Create a translate, rotate, scale matrix for the cube's world location
        XMMATRIX mat_model = XMMatrixAffineTransformation(DirectX::g_XMOne * 0.05f,
                                                          DirectX::g_XMZero,
                                                          XMLoadFloat4((XMFLOAT4*)&app_cubes[i].orientation),
                                                          XMLoadFloat3((XMFLOAT3*)&app_cubes[i].position));

        // Update the shader's constant buffer with the transform matrix info, and then draw the mesh!
        XMStoreFloat4x4(&transform_buffer.world, XMMatrixTranspose(mat_model));
        d3d_context->UpdateSubresource(app_constant_buffer, 0, nullptr, &transform_buffer, 0, 0);
        d3d_context->DrawIndexed((UINT)_countof(app_inds), 0, 0);
    }
}

///////////////////////////////////////////

void app_update() {
    // If the user presses the select action, lets add a cube at that location!
    for (uint32_t i = 0; i < 2; i++) {
        if (xr_input.handSelect[i])
            app_cubes.push_back(xr_input.handPose[i]);
    }
}

///////////////////////////////////////////

void app_update_predicted() {
    // Update the location of the hand cubes. This is done after the inputs have been updated to
    // use the predicted location, but during the render code, so we have the most up-to-date location.
    if (app_cubes.size() < 2)
        app_cubes.resize(2, xr_pose_identity);
    for (uint32_t i = 0; i < 2; i++) {
        app_cubes[i] = xr_input.renderHand[i] ? xr_input.handPose[i] : xr_pose_identity;
    }
}