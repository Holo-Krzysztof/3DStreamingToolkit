#include "pch.h"

#include <stdlib.h>
#include <shellapi.h>
#include <fstream>

#include "DeviceResources.h"
#include "CubeRenderer.h"

#ifdef TEST_RUNNER
#include "test_runner.h"
#else // TEST_RUNNER
#include "server_renderer.h"
#include "webrtc.h"
#endif // TEST_RUNNER

// Required app libs
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "usp10.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "winmm.lib")

using namespace DX;
using namespace Toolkit3DLibrary;
using namespace Toolkit3DSample;

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HWND				g_hWnd = nullptr;
DeviceResources*	g_deviceResources = nullptr;
CubeRenderer*		g_cubeRenderer = nullptr;
#ifdef TEST_RUNNER
VideoTestRunner*	g_videoTestRunner = nullptr;
#else // TEST_RUNNER
VideoHelper*		g_videoHelper = nullptr;
#endif // TESTRUNNER

//--------------------------------------------------------------------------------------
// Global Methods
//--------------------------------------------------------------------------------------
void FrameUpdate()
{
	g_cubeRenderer->Update();
	g_cubeRenderer->Render();
}

#ifdef REMOTE_RENDERING

// Handles input from client.
void InputUpdate(const std::string& message)
{
	char data[1024];
	Json::Reader reader;
	Json::Value msg = NULL;
	reader.parse(message, msg, false);

	if (msg.isMember("type"))
	{
		strcpy(data, msg.get("type", "").asCString());

		if (strcmp(data, "camera-transform-lookat") == 0)
		{
			if (msg.isMember("body"))
			{
				strcpy(data, msg.get("body", "").asCString());

				// Parses the camera transformation data.
				std::istringstream datastream(data);
				std::string token;

				// Eye point.
				getline(datastream, token, ',');
				float eyeX = stof(token);
				getline(datastream, token, ',');
				float eyeY = stof(token);
				getline(datastream, token, ',');
				float eyeZ = stof(token);

				// Focus point.
				getline(datastream, token, ',');
				float focusX = stof(token);
				getline(datastream, token, ',');
				float focusY = stof(token);
				getline(datastream, token, ',');
				float focusZ = stof(token);

				// Up vector.
				getline(datastream, token, ',');
				float upX = stof(token);
				getline(datastream, token, ',');
				float upY = stof(token);
				getline(datastream, token, ',');
				float upZ = stof(token);

				const DirectX::XMVECTORF32 lookAt = { focusX, focusY, focusZ, 0.f };
				const DirectX::XMVECTORF32 up = { upX, upY, upZ, 0.f };

				// Initializes the eye position vector.
				const DirectX::XMVECTORF32 eye = { eyeX, eyeY, eyeZ, 0.f };
				g_cubeRenderer->UpdateView(eye, lookAt, up);
			}
		}
	}
}

//--------------------------------------------------------------------------------------
// WebRTC
//--------------------------------------------------------------------------------------
int InitWebRTC(char* server, int port)
{
	rtc::EnsureWinsockInit();
	rtc::Win32Thread w32_thread;
	rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

#ifdef NO_UI
	DefaultMainWindow wnd(server, port, true, true, true);
#else // NO_UI
	DefaultMainWindow wnd(server, port, FLAG_autoconnect, FLAG_autocall, false, 1280, 720);
#endif // NO_UI

	if (!wnd.Create())
	{
		RTC_NOTREACHED();
		return -1;
	}

	// Initializes the device resources.
	g_deviceResources = new DeviceResources();
	g_deviceResources->SetWindow(wnd.handle());

	// Initializes the cube renderer.
	g_cubeRenderer = new CubeRenderer(g_deviceResources);

	// Creates and initializes the video helper library.
	g_videoHelper = new VideoHelper(
		g_deviceResources->GetD3DDevice(),
		g_deviceResources->GetD3DDeviceContext());

	g_videoHelper->Initialize(g_deviceResources->GetSwapChain());

	rtc::InitializeSSL();
	PeerConnectionClient client;

	rtc::scoped_refptr<Conductor> conductor(
		new rtc::RefCountedObject<Conductor>(
			&client, &wnd, &FrameUpdate, &InputUpdate, g_videoHelper));

	// Main loop.
	MSG msg;
	BOOL gm;
	while ((gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1)
	{
		if (!wnd.PreTranslateMessage(&msg))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}

	if (conductor->connection_active() || client.is_connected())
	{
		while ((conductor->connection_active() || client.is_connected()) &&
			(gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1)
		{
			if (!wnd.PreTranslateMessage(&msg))
			{
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}
		}
	}

	rtc::CleanupSSL();

	// Cleanup.
	delete g_videoHelper;
	delete g_cubeRenderer;
	delete g_deviceResources;

	return 0;
}

#else // REMOTE_RENDERING

//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

		// Note that this tutorial does not handle resizing (WM_SIZE) requests,
		// so we created the window without the resize border.

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

//--------------------------------------------------------------------------------------
// Registers class and creates window
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Registers class.
	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = 0;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"SpinningCubeClass";
	if (!RegisterClassEx(&wcex))
	{
		return E_FAIL;
	}

	// Creates window.
#ifdef STEREO_OUTPUT_MODE
	RECT rc = { 0, 0, 2560, 720 };
#else
	RECT rc = { 0, 0, 1280, 720 };
#endif
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindow(
		L"SpinningCubeClass",
		L"SpinningCube",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr);

	if (!g_hWnd)
	{
		return E_FAIL;
	}

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Render the frame
//--------------------------------------------------------------------------------------
void Render()
{
	FrameUpdate();
	g_deviceResources->Present();
}

#endif // REMOTE_RENDERING

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

#ifndef REMOTE_RENDERING
	if (FAILED(InitWindow(hInstance, nCmdShow)))
	{
		return 0;
	}

	// Initializes the device resources.
	g_deviceResources = new DeviceResources();
	g_deviceResources->SetWindow(g_hWnd);

	// Initializes the cube renderer.
	g_cubeRenderer = new CubeRenderer(g_deviceResources);

	RECT rc;
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

#ifdef TEST_RUNNER
	// Creates and initializes the video test runner library.
	g_videoTestRunner = new VideoTestRunner(
		g_deviceResources->GetD3DDevice(),
		g_deviceResources->GetD3DDeviceContext()); 

	g_videoTestRunner->StartTestRunner(g_deviceResources->GetSwapChain());
#else // TEST_RUNNER
	// Creates and initializes the video helper library.
	g_videoHelper = new VideoHelper(
		g_deviceResources->GetD3DDevice(),
		g_deviceResources->GetD3DDeviceContext());

	g_videoHelper->Initialize(g_deviceResources->GetSwapChain());
#endif // TEST_RUNNER
	
	// Main message loop.
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			Render();
#ifdef TEST_RUNNER
			if (g_videoTestRunner->TestsComplete())
			{
				break;
			}

			g_videoTestRunner->TestCapture();
			if (g_videoTestRunner->IsNewTest()) 
			{
				delete g_cubeRenderer;
				g_cubeRenderer = new CubeRenderer(g_deviceResources);
			}
#endif // TEST_RUNNER
		}
	}

	delete g_cubeRenderer;
	delete g_deviceResources;

	return (int)msg.wParam;
#else // REMOTE_RENDERING
	int nArgs;
	char server[1024];
	strcpy(server, FLAG_server);
	int port = FLAG_port;
	LPWSTR* szArglist = CommandLineToArgvW(lpCmdLine, &nArgs);

	// Try parsing command line arguments.
	if (szArglist && nArgs == 2)
	{
		wcstombs(server, szArglist[0], sizeof(server));
		port = _wtoi(szArglist[1]);
	}
	else // Try parsing config file.
	{
		std::string configFilePath = ExePath("webrtcConfig.json");
		std::ifstream configFile(configFilePath);
		Json::Reader reader;
		Json::Value root = NULL;
		if (configFile.good())
		{
			reader.parse(configFile, root, true);
			if (root.isMember("server"))
			{
				strcpy(server, root.get("server", FLAG_server).asCString());
			}

			if (root.isMember("port"))
			{
				port = root.get("port", FLAG_port).asInt();
			}
		}
	}

	return InitWebRTC(server, port);
#endif // REMOTE_RENDERING
}