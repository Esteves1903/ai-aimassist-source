#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>
#include <iostream>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

// Data
inline static ID3D11Device* g_pd3dDevice = nullptr;
inline static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
inline static IDXGISwapChain* g_pSwapChain = nullptr;
inline static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
inline static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#include "imgui_internal.h"

#include <D3DX11tex.h>
#pragma comment(lib, "D3DX11.lib")

#include "user_circle.h"
#include "inter.h"
#include "ico_pack.h"

#include "color.h"


inline ImFont* ico = nullptr;
inline ImFont* ico_2 = nullptr;
inline ImFont* ico_minimize = nullptr;
inline ImFont* tab_text = nullptr;
inline ImFont* tab_text1 = nullptr;
inline ImFont* tab_text2 = nullptr;
inline ImFont* tab_text3 = nullptr;
inline ImFont* ico_logo = nullptr;
inline ImFont* ico_subtab = nullptr;
inline ID3D11ShaderResourceView* us = nullptr;

namespace thread1
{
   void threadstart();
   void POC();
  
}
namespace var {

   inline float frequency_factor_x = 0.1; // Ajustez le facteur de fr�quence selon vos pr�f�rences pour le mouvement en x
    inline float frequency_factor_y = 0.1; // Ajustez le facteur de fr�quence selon vos pr�f�rences pour le mouvement en y



    inline float randomness_factor = 0.02;
    inline float kp = 0.2, ki = 0.005, kd = 0.02;
    inline bool RTrigger;
    inline bool LTrigger ;
    inline float smoothing_factor = 0.05;
    inline float x_smooth = 0, y_smooth = 0;
   inline int iteration = 0;
  inline float jitter_factor = 1;
   inline int boxY;
   inline int boxX;
   inline int Width;
   inline int Height;
    inline bool fovCircle = false;
    inline bool checkbox = true;
    inline bool esp = false;
    inline int scannFPS = 60;
    inline float smooth = 30.0f;
    inline float aim_height = 88.0f;
    inline int key0 = 0;
    inline int key4 = VK_INSERT;
    inline std::string detection_backend = "CPU";
    inline bool debug_console = false;
    inline bool aim_active = false;
    inline bool no_recoil = false;
    inline float recoil_strength = 2.5f;
    inline float recoil_x = 0.0f;
    inline bool triggerbot = false;
    inline float trigger_fov = 8.0f;
    inline int  trigger_key = VK_RBUTTON;
    inline int  trigger_delay_ms = 0;
    inline bool anti_spray = true;
    inline bool triggerbot_firing = false;
    inline bool aim_toggle = false;
    inline int  weapon_mode = 0;            // 0 = Rifle, 1 = Sniper
    inline float fov_radius = 187.0f;
    inline float confidence = 0.06f;
    inline float sensitivity = 1.0f;
    inline bool  natural_aim = false;
    inline bool  sticky_aim = false;
    inline float sticky_radius = 80.0f;
    inline int screen_box_x = 0, screen_box_y = 0, screen_box_w = 0, screen_box_h = 0;
    struct EspBox { int x, y, w, h; };
    inline EspBox esp_boxes[32];
    inline int esp_box_count = 0;
    inline bool radar = false;
    struct RadarDot { float dx, dy; };  // offset from frame center in YOLO pixels
    inline RadarDot radar_enemies[32];
    inline int radar_enemy_count = 0;
}

inline static float tab_alpha = 0.0f;
inline static float tab_add = 0.0f;
inline static int active_tab = 0;
inline int tabs = 0;

inline bool menu = true;
inline float dpi_scale = 1.0f;

inline void CustomStyleColor()
{
    ImGuiStyle& s = ImGui::GetStyle();

    s.Colors[ImGuiCol_WindowBg] = ImColor(60, 65, 80, 60);
    s.Colors[ImGuiCol_ChildBg] = ImColor(20, 20, 20, 255);
    s.Colors[ImGuiCol_PopupBg] = ImColor(26, 26, 26, 255);
    s.Colors[ImGuiCol_Text] = ImColor(120, 120, 120, 255);
    s.Colors[ImGuiCol_TextDisabled] = ImColor(100, 100, 100, 255);
    s.Colors[ImGuiCol_Border] = ImColor(28, 28, 28, 255);
    s.Colors[ImGuiCol_TextSelectedBg] = ImColor(25, 22, 33, 100);

    s.Colors[ImGuiCol_ScrollbarGrab] = ImColor(24, 24, 24, 255);
    s.Colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(24, 24, 24, 255);
    s.Colors[ImGuiCol_ScrollbarGrabActive] = ImColor(24, 24, 24, 255);

    s.WindowBorderSize = 0;
    s.WindowPadding = ImVec2(0, 0);
    s.WindowRounding = 5.f;
    s.PopupBorderSize = 0.f;
    s.PopupRounding = 5.f;
    s.ChildRounding = 7;
    s.ChildBorderSize = 1.f;
    s.FrameBorderSize = 1.0f;
    s.ScrollbarSize = 3.0f;
    s.FrameRounding = 5.f;
    s.ItemSpacing = ImVec2(0, 20);
    s.ItemInnerSpacing = ImVec2(10, 0);
}
