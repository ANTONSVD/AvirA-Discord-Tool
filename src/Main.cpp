#include "App.hpp"
#include "Theme.hpp"
#include <Windows.h>
#include <d3d11.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace AvirA
{
	struct S_Frame
	{
		HWND m_window = nullptr;
		WNDCLASSEXW m_class = {};
		ID3D11Device* m_device = nullptr;
		ID3D11DeviceContext* m_context = nullptr;
		IDXGISwapChain* m_swap = nullptr;
		ID3D11RenderTargetView* m_target = nullptr;
		C_App m_app;
		bool m_done = false;
	};

	static S_Frame m_frame;

	static bool CreateDevice(HWND window)
	{
		DXGI_SWAP_CHAIN_DESC desc = {};
		desc.BufferCount = 2;
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BufferDesc.RefreshRate.Numerator = 60;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.OutputWindow = window;
		desc.SampleDesc.Count = 1;
		desc.Windowed = TRUE;
		desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		UINT flags = 0;
		D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
		D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_11_0;
		if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, &level, 1, D3D11_SDK_VERSION, &desc, &m_frame.m_swap, &m_frame.m_device, &got, &m_frame.m_context) != S_OK)
			return false;
		ID3D11Texture2D* back = nullptr;
		m_frame.m_swap->GetBuffer(0, IID_PPV_ARGS(&back));
		if (back)
		{
			m_frame.m_device->CreateRenderTargetView(back, nullptr, &m_frame.m_target);
			back->Release();
		}
		return m_frame.m_target != nullptr;
	}

	static void CleanupDevice()
	{
		if (m_frame.m_target)
		{
			m_frame.m_target->Release();
			m_frame.m_target = nullptr;
		}
		if (m_frame.m_swap)
		{
			m_frame.m_swap->Release();
			m_frame.m_swap = nullptr;
		}
		if (m_frame.m_context)
		{
			m_frame.m_context->Release();
			m_frame.m_context = nullptr;
		}
		if (m_frame.m_device)
		{
			m_frame.m_device->Release();
			m_frame.m_device = nullptr;
		}
	}

	static LRESULT WINAPI WindowProc(HWND window, UINT message, WPARAM width, LPARAM height)
	{
		if (::ImGui_ImplWin32_WndProcHandler(window, message, width, height))
			return true;
		if (message == WM_SIZE && width != SIZE_MINIMIZED && m_frame.m_device)
		{
			if (m_frame.m_target)
			{
				m_frame.m_target->Release();
				m_frame.m_target = nullptr;
			}
			m_frame.m_swap->ResizeBuffers(0, (UINT)LOWORD(height), (UINT)HIWORD(height), DXGI_FORMAT_UNKNOWN, 0);
			ID3D11Texture2D* back = nullptr;
			m_frame.m_swap->GetBuffer(0, IID_PPV_ARGS(&back));
			if (back)
			{
				m_frame.m_device->CreateRenderTargetView(back, nullptr, &m_frame.m_target);
				back->Release();
			}
			return 0;
		}
		if (message == WM_SYSCOMMAND && (width & 0xFFF0) == SC_KEYMENU)
			return 0;
		if (message == WM_DESTROY)
		{
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProcW(window, message, width, height);
	}
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmd, int show)
{
	using namespace AvirA;
	(void)prev;
	(void)cmd;
	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(wc);
	wc.style = CS_CLASSDC;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = instance;
	wc.lpszClassName = L"AvirA Discord Tool";
	RegisterClassExW(&wc);
	HWND window = CreateWindowExW(0, wc.lpszClassName, L"AvirA Discord Tool", WS_OVERLAPPEDWINDOW, 100, 100, 980, 760, nullptr, nullptr, wc.hInstance, nullptr);
	if (!CreateDevice(window))
	{
		CleanupDevice();
		UnregisterClassW(wc.lpszClassName, wc.hInstance);
		return 1;
	}
	ShowWindow(window, show);
	UpdateWindow(window);
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.Fonts->AddFontDefault();
	{
		ImFontConfig merge;
		merge.MergeMode = true;
		merge.PixelSnapH = true;
		static const ImWchar cyrillic[] = { 0x0400, 0x04FF, 0 };
		io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Tahoma.ttf", 13.0f, &merge, cyrillic);
		io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Consola.ttf", 13.0f, &merge, cyrillic);
		static const ImWchar emoji[] = { 0x200D, 0x200D, 0x20D0, 0x20FF, 0x2100, 0x214F, 0x2190, 0x21FF, 0x2300, 0x23FF, 0x2460, 0x24FF, 0x25A0, 0x25FF, 0x2600, 0x27BF, 0x2B00, 0x2BFF, 0xFE00, 0xFE0F, 0x1F000, 0x1FAFF, 0xE0020, 0xE007F, 0 };
		io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisym.ttf", 13.0f, &merge, emoji);
		io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguiemj.ttf", 13.0f, &merge, emoji);
	}
	C_Theme::Apply();
	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(m_frame.m_device, m_frame.m_context);
	m_frame.m_app.Initialize();
	MSG msg = {};
	while (msg.message != WM_QUIT && !m_frame.m_done)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		m_frame.m_app.Draw();
		ImGui::Render();
		ImVec4 clear = ImVec4(0.05f, 0.05f, 0.055f, 1.0f);
		m_frame.m_context->OMSetRenderTargets(1, &m_frame.m_target, nullptr);
		m_frame.m_context->ClearRenderTargetView(m_frame.m_target, (float*)&clear);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		m_frame.m_swap->Present(1, 0);
	}
	m_frame.m_app.Shutdown();
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	CleanupDevice();
	DestroyWindow(window);
	UnregisterClassW(wc.lpszClassName, wc.hInstance);
	return 0;
}
