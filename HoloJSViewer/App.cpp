﻿#include "pch.h"
#include "app.h"
#include <WindowsNumerics.h>

using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Foundation::Numerics;
using namespace Windows::Graphics::Display;
using namespace Windows::Graphics::Holographic;
using namespace Windows::Perception::Spatial;
using namespace Microsoft::WRL;
using namespace Platform;

using namespace HoloJsViewer;

// Implementation of the IFrameworkViewSource interface, necessary to run our app.
ref class SimpleApplicationSource sealed : Windows::ApplicationModel::Core::IFrameworkViewSource
{
public:
	virtual Windows::ApplicationModel::Core::IFrameworkView^ CreateView()
	{
		return ref new App();
	}
};

// The main function creates an IFrameworkViewSource for our app, and runs the app.
[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
	auto simpleApplicationSource = ref new SimpleApplicationSource();
	CoreApplication::Run(simpleApplicationSource);
	return 0;
}

App::App() :
	mWindowClosed(false),
	mWindowVisible(true),
	mEglDisplay(EGL_NO_DISPLAY),
	mEglContext(EGL_NO_CONTEXT),
	mEglSurface(EGL_NO_SURFACE)
{
}

// The first method called when the IFrameworkView is being created.
void App::Initialize(CoreApplicationView^ applicationView)
{
	CoreApplication::Suspending
		+= ref new EventHandler<SuspendingEventArgs^>(this, &App::OnSuspending);

	CoreApplication::EnteredBackground
		+= ref new EventHandler<EnteredBackgroundEventArgs^>(this, &App::OnEnteredBackground);

	// Register event handlers for app lifecycle. This example includes Activated, so that we
	// can make the CoreWindow active and start rendering on the window.
	applicationView->Activated +=
		ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &App::OnActivated);

	// Logic for other event handlers could go here.
	// Information about the Suspending and Resuming event handlers can be found here:
	// http://msdn.microsoft.com/en-us/library/windows/apps/xaml/hh994930.aspx
}

// Called when the CoreWindow object is created (or re-created).
void App::SetWindow(CoreWindow^ window)
{
	window->VisibilityChanged +=
		ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &App::OnVisibilityChanged);

	window->Closed +=
		ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &App::OnWindowClosed);

	try
	{
		// Create a holographic space for the core window for the current view.
		mHolographicSpace = HolographicSpace::CreateForCoreWindow(window);

		// Get the default SpatialLocator.
		SpatialLocator^ mLocator = SpatialLocator::GetDefault();

		// Create a stationary frame of reference.
		mStationaryReferenceFrame = mLocator->CreateStationaryFrameOfReferenceAtCurrentLocation(float3(0, 0, -2));

		// The HolographicSpace has been created, so EGL can be initialized in holographic mode.
		InitializeEGL(mHolographicSpace);
	}
	catch (Platform::Exception^ ex)
	{
		if (ex->HResult == HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED))
		{
			// Device does not support holographic spaces. Initialize EGL to use the CoreWindow instead.
			InitializeEGL(window);
		}
	}
}

void App::LoadAndExecuteScript(String^ uri, bool local, bool debug)
{
	// Create a host for the app now that we have a window
	if (m_holoScriptHost != nullptr)
	{
		m_holoScriptHost->Shutdown();
	}

	m_holoScriptHost = ref new HologramJS::HologramScriptHost();
	m_holoScriptHost->Initialize(debug);

	if (mHolographicSpace != nullptr)
	{
		m_holoScriptHost->EnableHolographicExperimental(mStationaryReferenceFrame);
	}

	eglQuerySurface(mEglDisplay, mEglSurface, EGL_WIDTH, &mPanelWidth);
	eglQuerySurface(mEglDisplay, mEglSurface, EGL_HEIGHT, &mPanelHeight);

	m_holoScriptHost->ResizeWindow(mPanelWidth, mPanelHeight);

	if (local)
	{
		m_holoScriptHost->RunLocalScriptAppAsync(uri);
	}
	else
	{
		m_holoScriptHost->RunWebScriptAppAsync(uri);
	}
	
}

// Initializes scene resources
void App::Load(Platform::String^ entryPoint)
{
	RecreateRenderer();
}

void App::RecreateRenderer()
{

}

#define GLEXT_ANGLE_HOLOGRAPHIC_MONO_VIEW_MATRIX 0x8FE6
#define GLEXT_ANGLE_HOLOGRAPHIC_MONO_PROJECTION_MATRIX 0x8FE9
#define GLEXT_ANGLE_HOLOGRAPHIC_LEFT_VIEW_MATRIX 0x8FEA
#define GLEXT_ANGLE_HOLOGRAPHIC_LEFT_PROJECTION_MATRIX 0x8FEB
#define GLEXT_ANGLE_HOLOGRAPHIC_RIGHT_VIEW_MATRIX 0x8FEC
#define GLEXT_ANGLE_HOLOGRAPHIC_RIGHT_PROJECTION_MATRIX 0x8FED

// This method is called after the window becomes active.
void App::Run()
{
	Windows::Foundation::Numerics::float4x4 holographicViewMatrixMid;
	Windows::Foundation::Numerics::float4x4 holographicProjectionMatrixMid;
	Windows::Foundation::Numerics::float4x4 holographicViewMatrixLeft;
	Windows::Foundation::Numerics::float4x4 holographicProjectionMatrixLeft;
	Windows::Foundation::Numerics::float4x4 holographicViewMatrixRight;
	Windows::Foundation::Numerics::float4x4 holographicProjectionMatrixRight;

	while (!mWindowClosed)
	{
		if (mWindowVisible)
		{
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

			EGLint panelWidth = 0;
			EGLint panelHeight = 0;
			eglQuerySurface(mEglDisplay, mEglSurface, EGL_WIDTH, &panelWidth);
			eglQuerySurface(mEglDisplay, mEglSurface, EGL_HEIGHT, &panelHeight);

			if (mPanelWidth != panelWidth || mPanelHeight != panelHeight)
			{
				m_holoScriptHost->ResizeWindow(panelWidth, panelHeight);
				mPanelHeight = panelHeight;
				mPanelWidth = panelWidth;
			}

			glClear(GL_COLOR_BUFFER_BIT);
			glGetFloatv(GLEXT_ANGLE_HOLOGRAPHIC_MONO_VIEW_MATRIX, &holographicViewMatrixMid.m11);
			glGetFloatv(GLEXT_ANGLE_HOLOGRAPHIC_MONO_PROJECTION_MATRIX, &holographicProjectionMatrixMid.m11);
			glGetFloatv(GLEXT_ANGLE_HOLOGRAPHIC_LEFT_VIEW_MATRIX, &holographicViewMatrixLeft.m11);
			glGetFloatv(GLEXT_ANGLE_HOLOGRAPHIC_LEFT_PROJECTION_MATRIX, &holographicProjectionMatrixLeft.m11);
			glGetFloatv(GLEXT_ANGLE_HOLOGRAPHIC_RIGHT_VIEW_MATRIX, &holographicViewMatrixRight.m11);
			glGetFloatv(GLEXT_ANGLE_HOLOGRAPHIC_RIGHT_PROJECTION_MATRIX, &holographicProjectionMatrixRight.m11);

			m_holoScriptHost->VSync(holographicViewMatrixMid, holographicProjectionMatrixMid, holographicViewMatrixLeft, holographicProjectionMatrixLeft, holographicViewMatrixRight, holographicProjectionMatrixRight);


			// The call to eglSwapBuffers might not be successful (e.g. due to Device Lost)
			// If the call fails, then we must reinitialize EGL and the GL resources.
			if (eglSwapBuffers(mEglDisplay, mEglSurface) != GL_TRUE)
			{
				CleanupEGL();


				if (mHolographicSpace != nullptr)
				{
					InitializeEGL(mHolographicSpace);
				}
				else
				{
					InitializeEGL(CoreWindow::GetForCurrentThread());
				}

				RecreateRenderer();
			}
		}
		else
		{
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
		}
	}

	CleanupEGL();
}

// Terminate events do not cause Uninitialize to be called. It will be called if your IFrameworkView
// class is torn down while the app is in the foreground.
void App::Uninitialize()
{
}

void App::OnSuspending(Platform::Object ^sender, Windows::ApplicationModel::SuspendingEventArgs ^args)
{
	CoreApplication::Exit();
}

void App::OnEnteredBackground(Platform::Object ^sender, Windows::ApplicationModel::EnteredBackgroundEventArgs ^args)
{
	CoreApplication::Exit();
}

// Application lifecycle event handler.
void App::OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args)
{
	if (args->Kind == ActivationKind::Protocol)
	{
		ProtocolActivatedEventArgs^ eventArgs = dynamic_cast<ProtocolActivatedEventArgs^>(args);
		String^ uri = eventArgs->Uri->RawUri;
		std::wstring str = uri->Data();
		bool debug = false;
		if (str.compare(0, 11, L"holojsdebug") == 0) {
			str = str.substr(12, std::wstring::npos);
			debug = true;
		}
		else {
			str = str.substr(7, std::wstring::npos);
		}
		String^ parsedUri = ref new String(str.data());
		String^ debugString = L"Protocol activation" + (debug ? L" (Debug)" : L"") + ". Loading uri [" + parsedUri + L"]";
		OutputDebugString(debugString->Data());
		LoadAndExecuteScript(parsedUri, false, debug);
	}
	else
	{
		OutputDebugString(L"Non-protocol activation");
		LoadAndExecuteScript(L"Default\\index.json", true, false);
	}
	// Run() won't start until the CoreWindow is activated.
	CoreWindow::GetForCurrentThread()->Activate();
}

// Window event handlers.
void App::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
{
	mWindowVisible = args->Visible;
}

void App::OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args)
{
	mWindowClosed = true;
}

void App::InitializeEGL(Windows::UI::Core::CoreWindow^ window)
{
	App::InitializeEGLInner(window);
}

void App::InitializeEGL(Windows::Graphics::Holographic::HolographicSpace^ holographicSpace)
{
	App::InitializeEGLInner(holographicSpace);
}

void App::InitializeEGLInner(Platform::Object^ windowBasis)
{
	const EGLint configAttributes[] =
	{
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 8,
		EGL_STENCIL_SIZE, 8,
		EGL_NONE
	};

	const EGLint contextAttributes[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE
	};

	const EGLint surfaceAttributes[] =
	{
		// EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER is part of the same optimization as EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER (see above).
		// If you have compilation issues with it then please update your Visual Studio templates.
		EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER, EGL_TRUE,
		EGL_NONE
	};

	const EGLint defaultDisplayAttributes[] =
	{
		// These are the default display attributes, used to request ANGLE's D3D11 renderer.
		// eglInitialize will only succeed with these attributes if the hardware supports D3D11 Feature Level 10_0+.
		EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,

		// EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER is an optimization that can have large performance benefits on mobile devices.
		// Its syntax is subject to change, though. Please update your Visual Studio templates if you experience compilation issues with it.
		EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE,

		// EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE is an option that enables ANGLE to automatically call 
		// the IDXGIDevice3::Trim method on behalf of the application when it gets suspended. 
		// Calling IDXGIDevice3::Trim when an application is suspended is a Windows Store application certification requirement.
		EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
		EGL_NONE,
	};

	const EGLint fl9_3DisplayAttributes[] =
	{
		// These can be used to request ANGLE's D3D11 renderer, with D3D11 Feature Level 9_3.
		// These attributes are used if the call to eglInitialize fails with the default display attributes.
		EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
		EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, 9,
		EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, 3,
		EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE,
		EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
		EGL_NONE,
	};

	const EGLint warpDisplayAttributes[] =
	{
		// These attributes can be used to request D3D11 WARP.
		// They are used if eglInitialize fails with both the default display attributes and the 9_3 display attributes.
		EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
		EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE,
		EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE,
		EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
		EGL_NONE,
	};

	EGLConfig config = NULL;

	// eglGetPlatformDisplayEXT is an alternative to eglGetDisplay. It allows us to pass in display attributes, used to configure D3D11.
	PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
	if (!eglGetPlatformDisplayEXT)
	{
		throw Exception::CreateException(E_FAIL, L"Failed to get function eglGetPlatformDisplayEXT");
	}

	//
	// To initialize the display, we make three sets of calls to eglGetPlatformDisplayEXT and eglInitialize, with varying 
	// parameters passed to eglGetPlatformDisplayEXT:
	// 1) The first calls uses "defaultDisplayAttributes" as a parameter. This corresponds to D3D11 Feature Level 10_0+.
	// 2) If eglInitialize fails for step 1 (e.g. because 10_0+ isn't supported by the default GPU), then we try again 
	//    using "fl9_3DisplayAttributes". This corresponds to D3D11 Feature Level 9_3.
	// 3) If eglInitialize fails for step 2 (e.g. because 9_3+ isn't supported by the default GPU), then we try again 
	//    using "warpDisplayAttributes".  This corresponds to D3D11 Feature Level 11_0 on WARP, a D3D11 software rasterizer.
	//

	// This tries to initialize EGL to D3D11 Feature Level 10_0+. See above comment for details.
	mEglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, defaultDisplayAttributes);
	if (mEglDisplay == EGL_NO_DISPLAY)
	{
		throw Exception::CreateException(E_FAIL, L"Failed to get EGL display");
	}

	if (eglInitialize(mEglDisplay, NULL, NULL) == EGL_FALSE)
	{
		// This tries to initialize EGL to D3D11 Feature Level 9_3, if 10_0+ is unavailable (e.g. on some mobile devices).
		mEglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, fl9_3DisplayAttributes);
		if (mEglDisplay == EGL_NO_DISPLAY)
		{
			throw Exception::CreateException(E_FAIL, L"Failed to get EGL display");
		}

		if (eglInitialize(mEglDisplay, NULL, NULL) == EGL_FALSE)
		{
			// This initializes EGL to D3D11 Feature Level 11_0 on WARP, if 9_3+ is unavailable on the default GPU.
			mEglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, warpDisplayAttributes);
			if (mEglDisplay == EGL_NO_DISPLAY)
			{
				throw Exception::CreateException(E_FAIL, L"Failed to get EGL display");
			}

			if (eglInitialize(mEglDisplay, NULL, NULL) == EGL_FALSE)
			{
				// If all of the calls to eglInitialize returned EGL_FALSE then an error has occurred.
				throw Exception::CreateException(E_FAIL, L"Failed to initialize EGL");
			}
		}
	}

	EGLint numConfigs = 0;
	if ((eglChooseConfig(mEglDisplay, configAttributes, &config, 1, &numConfigs) == EGL_FALSE) || (numConfigs == 0))
	{
		throw Exception::CreateException(E_FAIL, L"Failed to choose first EGLConfig");
	}

	// Create a PropertySet and initialize with the EGLNativeWindowType.
	PropertySet^ surfaceCreationProperties = ref new PropertySet();
	surfaceCreationProperties->Insert(ref new String(EGLNativeWindowTypeProperty), windowBasis);
	if (mStationaryReferenceFrame != nullptr)
	{
		surfaceCreationProperties->Insert(ref new String(EGLBaseCoordinateSystemProperty), mStationaryReferenceFrame);
		surfaceCreationProperties->Insert(ref new String(EGLAutomaticStereoRenderingProperty), PropertyValue::CreateBoolean(false));
		surfaceCreationProperties->Insert(ref new String(EGLAutomaticDepthBasedImageStabilizationProperty), PropertyValue::CreateBoolean(false));
	}

	// You can configure the surface to render at a lower resolution and be scaled up to
	// the full window size. This scaling is often free on mobile hardware.
	//
	// One way to configure the SwapChainPanel is to specify precisely which resolution it should render at.
	// Size customRenderSurfaceSize = Size(800, 600);
	// surfaceCreationProperties->Insert(ref new String(EGLRenderSurfaceSizeProperty), PropertyValue::CreateSize(customRenderSurfaceSize));
	//
	// Another way is to tell the SwapChainPanel to render at a certain scale factor compared to its size.
	// e.g. if the SwapChainPanel is 1920x1280 then setting a factor of 0.5f will make the app render at 960x640
	// float customResolutionScale = 0.5f;
	// surfaceCreationProperties->Insert(ref new String(EGLRenderResolutionScaleProperty), PropertyValue::CreateSingle(customResolutionScale));

	mEglSurface = eglCreateWindowSurface(mEglDisplay, config, reinterpret_cast<IInspectable*>(surfaceCreationProperties), surfaceAttributes);
	if (mEglSurface == EGL_NO_SURFACE)
	{
		throw Exception::CreateException(E_FAIL, L"Failed to create EGL fullscreen surface");
	}

	mEglContext = eglCreateContext(mEglDisplay, config, EGL_NO_CONTEXT, contextAttributes);
	if (mEglContext == EGL_NO_CONTEXT)
	{
		throw Exception::CreateException(E_FAIL, L"Failed to create EGL context");
	}

	if (eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext) == EGL_FALSE)
	{
		throw Exception::CreateException(E_FAIL, L"Failed to make fullscreen EGLSurface current");
	}

	// By default, allow HolographicFrame::PresentUsingCurrentPrediction() to wait for the current frame to 
	// finish before it returns.
	eglSurfaceAttrib(mEglDisplay, mEglSurface, EGLEXT_WAIT_FOR_VBLANK_ANGLE, false);
}

void App::CleanupEGL()
{
	if (mEglDisplay != EGL_NO_DISPLAY && mEglSurface != EGL_NO_SURFACE)
	{
		eglDestroySurface(mEglDisplay, mEglSurface);
		mEglSurface = EGL_NO_SURFACE;
	}

	if (mEglDisplay != EGL_NO_DISPLAY && mEglContext != EGL_NO_CONTEXT)
	{
		eglDestroyContext(mEglDisplay, mEglContext);
		mEglContext = EGL_NO_CONTEXT;
	}

	if (mEglDisplay != EGL_NO_DISPLAY)
	{
		eglTerminate(mEglDisplay);
		mEglDisplay = EGL_NO_DISPLAY;
	}
}
