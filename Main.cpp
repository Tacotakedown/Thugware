#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "MinHook/MinHook.h"
#if _WIN64 
#pragma comment(lib, "MinHook/libMinHook.x64.lib")
#else
#pragma comment(lib, "MinHook/libMinHook.x86.lib")
#endif

#include "font_awesome.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"
#include <vector>
#include <string>

#include"font_awesome.cpp"

#include <d3d11.h>
#include <io.h>
#include "ImGui/imgui_internal.h"
#pragma comment(lib, "d3d11.lib")

// Globals
HINSTANCE dll_handle;

typedef long(__stdcall* present)(IDXGISwapChain*, UINT, UINT);
present p_present;
present p_present_target;
bool get_present_pointer()
{
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = GetForegroundWindow();
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	IDXGISwapChain* swap_chain;
	ID3D11Device* device;

	const D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	if (D3D11CreateDeviceAndSwapChain(
		NULL, 
		D3D_DRIVER_TYPE_HARDWARE, 
		NULL, 
		0, 
		feature_levels, 
		2, 
		D3D11_SDK_VERSION, 
		&sd, 
		&swap_chain, 
		&device, 
		nullptr, 
		nullptr) == S_OK)
	{
		void** p_vtable = *reinterpret_cast<void***>(swap_chain);
		swap_chain->Release();
		device->Release();
		//context->Release();
		p_present_target = (present)p_vtable[8];
		return true;
	}
	return false;
}

WNDPROC oWndProc;
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (true && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

bool init = false;
HWND window = NULL;
ID3D11Device* p_device = NULL;
ID3D11DeviceContext* p_context = NULL;
ID3D11RenderTargetView* mainRenderTargetView = NULL;

ImGuiStyle& style = ImGui::GetStyle();

inline void CenterButtons(std::vector<std::string>names, std::vector<int>indexes, int& selected_index) {
	std::vector<ImVec2> sizes = {};
	float total_area = 0.0f;
	const auto& style =  ImGui::GetStyle();

	for (std::string& name: names)
	{
		const ImVec2 label_size = ImGui::CalcTextSize(name.c_str(), 0, true);
		ImVec2 size = ImGui::CalcItemSize(ImVec2(), label_size.x + style.FramePadding.x, label_size.y + style.FramePadding.y * 2.0f);

		size.x += 30.0f;
		size.y += 10.0f;

		sizes.push_back(size);
		total_area += size.x;
	}
	ImGui::SameLine((ImGui::GetContentRegionAvail().x / 2) - (total_area / 2));
	for (uint32_t i = 0; i < names.size(); i++)
	{
		ImGui::SetCursorPosY(ImGui::GetCursorPosY());
		
		if (selected_index == indexes[i]) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImColor(189, 0, 0, 244).Value);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor(189, 0, 0, 244).Value);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor(189, 0, 0, 244).Value);
			if (ImGui::Button(names[i].c_str(), sizes[i]))
				selected_index = indexes[i];
			ImGui::PopStyleColor(3);
		}
		else
		{
			if (ImGui::Button(names[i].c_str(), sizes[i]))
				selected_index = indexes[i];
		}
		ImGui::PopStyleVar();
		if (i != names.size())
			ImGui::SameLine();
	}

}
inline void centerText(const char* format, const float y_padding = 0.0f, ImColor color = ImColor(255,255,255))
{
	const ImVec2 text_size = ImGui::CalcTextSize(format);

	ImGui::SameLine(ImGui::GetContentRegionAvail().x / 2 - (text_size.x / 2));
	if (y_padding > 0.0f)
	{
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y_padding);
	}
	ImGui::TextColored(color, format);
}

inline void checkbox(const char* format, bool* value)
{
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.5f, 1.5f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImColor(189, 0, 0, 255).Value);
	ImGui::Checkbox(format, value);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
	ImGui::Dummy(ImVec2());
}

static long __stdcall detour_present(IDXGISwapChain* p_swap_chain, UINT sync_interval, UINT flags) {
	
	if (!init) {
		if (SUCCEEDED(p_swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&p_device)))
		{
			p_device->GetImmediateContext(&p_context);
			DXGI_SWAP_CHAIN_DESC sd;
			p_swap_chain->GetDesc(&sd);
			window = sd.OutputWindow;
			ID3D11Texture2D* pBackBuffer;
			p_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
			p_device->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
			pBackBuffer->Release();
			oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
			
			ImGui_ImplWin32_Init(window);
			ImGui_ImplDX11_Init(p_device, p_context);
		
			init = true;
		}
		else
			return p_present(p_swap_chain, sync_interval, flags);
	}
	ImGuiIO& io2 = ImGui::GetIO();
	io2.Fonts->AddFontFromFileTTF("C:\\titillium.ttf", 26.0f);
	

	static const ImWchar icon_ranges[]{ 0xf000,0xf3ff,0 };
	ImFontConfig icons_config;
	icons_config.MergeMode = true;
	icons_config.PixelSnapH = true;
	icons_config.OversampleH = 3;
	icons_config.OversampleV = 3;

	ImFont* icons_font = io2.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_data, font_awesome_size, 19.5f, &icons_config, icon_ranges);
	

	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.01f, 0.01f, 0.87f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.04f, 0.02f, 0.48f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.98f, 0.26f, 0.26f, 0.09f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.98f, 0.26f, 0.26f, 0.09f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.02f, 0.02f, 0.96f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.85f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.53f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.43f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.88f, 0.24f, 0.24f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.35f, 0.35f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.98f, 0.26f, 0.26f, 0.40f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 0.44f, 0.44f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 0.33f, 0.33f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.31f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.16f, 0.00f, 0.00f, 0.80f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.16f, 0.00f, 0.00f, 0.80f);
	colors[ImGuiCol_Separator] = ImVec4(0.59f, 0.00f, 0.00f, 0.23f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.59f, 0.00f, 0.00f, 0.23f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.59f, 0.00f, 0.00f, 0.23f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.00f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.00f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.00f);
	colors[ImGuiCol_Tab] = ImVec4(0.14f, 0.01f, 0.01f, 0.86f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.01f, 0.01f, 0.98f);
	colors[ImGuiCol_TabActive] = ImVec4(0.68f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(9.0f, 9.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9.0f, 7.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 6.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(20.0f, 3.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(4.0f, 4.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 15.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 12.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 15.0f);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 12.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 12.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 12.0f);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, ImVec2(0.5f, 0.5f));
	ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir_None;


	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	static int index = 0;
	ImGui::NewFrame();

	ImGui::SetNextWindowSize({ 1100,800 });

	ImGui::Begin("Thugware", nullptr,  ImGuiWindowFlags_NoCollapse);

	CenterButtons({ ICON_FA_CROSSHAIRS" Aimbot", ICON_FA_EYE" Visuals",ICON_FA_HANDS" Thugmode",ICON_FA_ADDRESS_BOOK" Item Spawner",ICON_FA_CAMERA" FOV", " Skin Changer"}, {0,1,2,3,4,5}, index);
	
	ImGui::PushStyleColor(ImGuiCol_Separator, ImColor(0, 0, 0, 0).Value);
	ImGui::Separator();
	ImGui::PopStyleColor();
	ImGui::NewLine();

	ImGui::BeginChild("##left", ImVec2(ImGui::GetContentRegionAvail().x / 2.0f, ImGui::GetContentRegionAvail().y));
	{
		if (index == 1) {
			static bool GlobalEsp = false;
			checkbox("Players", &GlobalEsp);
			static bool ShipEsp = false;
			checkbox("Ships", &ShipEsp);
		}
		
	}
	
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Separator, ImColor(189, 0, 0, 200).Value);
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::BeginChild("##right", ImVec2(ImGui::GetContentRegionAvail().x / 1.0f, ImGui::GetContentRegionAvail().y));
	{
		if (index == 1) {
			static ImVec4 color;
			ImGui::Text("Colors");
			ImGui::Separator();
			ImGui::Text("Bones");
			ImGui::SameLine();
			ImGui::ColorEdit4("MyColor##3", (float*)&color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
		}
	}
	ImGui::EndChild();


	ImGui::ShowDemoWindow();
	ImGui::EndFrame();
	ImGui::Render();

	p_context->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	return p_present(p_swap_chain, sync_interval, flags);
}

DWORD __stdcall EjectThread(LPVOID lpParameter) {
	Sleep(100);
	FreeLibraryAndExitThread(dll_handle, 0);
	Sleep(100);
	return 0;
}


//"main" loop
int WINAPI main()
{

	if (!get_present_pointer()) 
	{
		return 1;
	}

	MH_STATUS status = MH_Initialize();
	if (status != MH_OK)
	{
		return 1;
	}

	if (MH_CreateHook(reinterpret_cast<void**>(p_present_target), &detour_present, reinterpret_cast<void**>(&p_present)) != MH_OK) {
		return 1;
	}

	if (MH_EnableHook(p_present_target) != MH_OK) {
		return 1;
	}

	while (true) {
		Sleep(50);
		
		if (GetAsyncKeyState(VK_RCONTROL) & 1) {

		}

		if (GetAsyncKeyState(VK_RSHIFT)) {
			break;
		}
	}

	//Cleanup
	if (MH_DisableHook(MH_ALL_HOOKS) != MH_OK) {
		return 1;
	}
	if (MH_Uninitialize() != MH_OK) {
		return 1;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	if (mainRenderTargetView) { mainRenderTargetView->Release(); mainRenderTargetView = NULL; }
	if (p_context) { p_context->Release(); p_context = NULL; }
	if (p_device) { p_device->Release(); p_device = NULL; }
	SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)(oWndProc));

	CreateThread(0, 0, EjectThread, 0, 0, 0);

	return 0;
}



BOOL __stdcall DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		dll_handle = hModule;
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)main, NULL, 0, NULL);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{

	}
	return TRUE;
}