﻿#pragma once

#include <string>
#include "pch.h"

namespace HoloJsViewer
{

	ref class App sealed : public Windows::ApplicationModel::Core::IFrameworkView
	{
	public:
		App();

		// IFrameworkView Methods.
		virtual void Initialize(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView);
		virtual void SetWindow(Windows::UI::Core::CoreWindow^ window);
		virtual void Load(Platform::String^ entryPoint);
		virtual void Run();
	
		virtual void Uninitialize();

	private:
		void RecreateRenderer();

		// Application lifecycle event handlers.
		void OnActivated(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView, Windows::ApplicationModel::Activation::IActivatedEventArgs^ args);

		// Window event handlers.
		void OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args);
		void OnWindowClosed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CoreWindowEventArgs^ args);

		void InitializeEGL(Windows::Graphics::Holographic::HolographicSpace^ holographicSpace);
		void InitializeEGL(Windows::UI::Core::CoreWindow^ window);
		void App::InitializeEGLInner(Platform::Object^ windowBasis);
		void CleanupEGL();

		bool mWindowClosed;
		bool mWindowVisible;

		EGLDisplay mEglDisplay;
		EGLContext mEglContext;
		EGLSurface mEglSurface;

		// The holographic space the app will use for rendering.
		Windows::Graphics::Holographic::HolographicSpace^ mHolographicSpace = nullptr;

		// The world coordinate system. In this example, a reference frame placed in the environment.
		Windows::Perception::Spatial::SpatialStationaryFrameOfReference^ mStationaryReferenceFrame = nullptr;

		EGLint mPanelWidth = 0;
		EGLint mPanelHeight = 0;

		HologramJS::HologramScriptHost^ m_holoScriptHost;
		void LoadAndExecuteScript(Platform::String^ uri, bool local, bool debug);
		void OnSuspending(Platform::Object ^sender, Windows::ApplicationModel::SuspendingEventArgs ^args);
		void OnEnteredBackground(Platform::Object ^ sender, Windows::ApplicationModel::EnteredBackgroundEventArgs ^ args);
	};

}