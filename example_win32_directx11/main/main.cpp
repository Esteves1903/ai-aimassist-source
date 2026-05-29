
#include "main.h"
#include <dwmapi.h>
#include "bind.h"
#include "auth.hpp"
#include "credentials.h"
#include <sstream>
#include <fstream>

static bool animated_background = false;
static ID3D11ShaderResourceView* dr = nullptr;
static ID3D11ShaderResourceView* dr1 = nullptr;
static RECT rc = { 0 };
static bool open = true;

void Particles()
{
    const ImVec2 screen_size = { static_cast<float>(GetSystemMetrics(SM_CXSCREEN)), static_cast<float>(GetSystemMetrics(SM_CYSCREEN)) };

    static ImVec2 particle_pos[50];
    static ImVec2 particle_target_pos[50];
    static float particle_speed[50];
    static float particle_radius[50];

    for (int i = 0; i < 50; ++i)
    {
        if (particle_pos[i].x == 0.0f || particle_pos[i].y == 0.0f)
        {
            particle_pos[i].x = static_cast<float>(rand() % static_cast<int>(screen_size.x) + 1);
            particle_pos[i].y = 15.0f;
            particle_speed[i] = static_cast<float>(1 + rand() % 25);
            particle_radius[i] = static_cast<float>(rand() % 4);

            particle_target_pos[i].x = static_cast<float>(rand() % static_cast<int>(screen_size.x));
            particle_target_pos[i].y = screen_size.y * 2.0f;
        }

        particle_pos[i] = ImLerp(particle_pos[i], particle_target_pos[i], ImGui::GetIO().DeltaTime * (particle_speed[i] / 60.0f));

        if (particle_pos[i].y > screen_size.y)
        {
            particle_pos[i] = ImVec2(0.0f, 0.0f);
        }

        ImGui::GetBackgroundDrawList()->AddCircleFilled(particle_pos[i], particle_radius[i], ImColor(255, 255, 255, 255));
    }
}
static HWND hwnd = nullptr;
static bool check = true;
void hide()
{
    if (GetAsyncKeyState(var::key4) && check)
    {
        check = false;
        open = !open;
        const LONG exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        if (!open)
            SetWindowLong(hwnd, GWL_EXSTYLE, exstyle | WS_EX_TRANSPARENT);
        else
            SetWindowLong(hwnd, GWL_EXSTYLE, exstyle & ~WS_EX_TRANSPARENT);
    }
    else if (!GetAsyncKeyState(var::key4))
        check = true;
}

std::string app_name = KEYAUTH_APP_NAME;
std::string owner_id = KEYAUTH_OWNER_ID;
std::string path     = "";
std::string version  = KEYAUTH_VERSION;
std::string url      = KEYAUTH_URL;

KeyAuth::api KeyAuthApp(app_name, owner_id, version, url, path);

static bool login = false;
static int login_tab = 0;
static char login_username[256] = "";
static char login_password[256] = "";
static char register_username[256] = "";
static char register_password[256] = "";
static char register_key[256] = "";
static char register_email[256] = "";
static char login_error_message[512] = "";
static std::string get_cfg_path()
{
    char appdata[MAX_PATH];
    GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH);
    const std::string dir = std::string(appdata) + "\\Keyvex";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir + "\\session.dat";
}

static void save_session(const char* user, const char* pass)
{
    std::string data = std::string(user) + "\n" + std::string(pass);
    for (char& c : data) c ^= 0x6B;
    std::ofstream f(get_cfg_path(), std::ios::binary);
    if (f) f.write(data.c_str(), (std::streamsize)data.size());
}

static bool load_session(char* user, char* pass)
{
    std::ifstream f(get_cfg_path(), std::ios::binary);
    if (!f) return false;
    std::string data((std::istreambuf_iterator<char>(f)), {});
    for (char& c : data) c ^= 0x6B;
    const size_t sep = data.find('\n');
    if (sep == std::string::npos) return false;
    strncpy_s(user, 256, data.substr(0, sep).c_str(), _TRUNCATE);
    strncpy_s(pass, 256, data.substr(sep + 1).c_str(), _TRUNCATE);
    return user[0] != '\0' && pass[0] != '\0';
}

void move_window()
{
    GetWindowRect(hwnd, &rc);
    const ImVec2 window_pos = ImGui::GetWindowPos();

    if (window_pos.x != 0.0f || window_pos.y != 0.0f)
    {
        MoveWindow(hwnd, rc.left + static_cast<int>(window_pos.x), rc.top + static_cast<int>(window_pos.y), 855, 650, TRUE);
        ImGui::SetWindowPos(ImVec2(0.0f, 0.0f));
    }
}

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    WNDCLASSEXA wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = nullptr;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = "ImGui";
    wc.lpszClassName = "Example";
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExA(&wc);
    
    const int screen_width = GetSystemMetrics(SM_CXSCREEN);
    const int screen_height = GetSystemMetrics(SM_CYSCREEN);
    hwnd = CreateWindowExA(WS_EX_LAYERED | WS_EX_TOPMOST, wc.lpszClassName, "overlay", WS_POPUP,
        0, 0, screen_width, screen_height, nullptr, nullptr, nullptr, nullptr);

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);

    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    const auto* glyph_ranges = io.Fonts->GetGlyphRangesCyrillic();
    io.Fonts->AddFontFromMemoryTTF(&inter, sizeof(inter), 16.0f * dpi_scale, nullptr, glyph_ranges);
    tab_text1 = io.Fonts->AddFontFromMemoryTTF(&inter, sizeof(inter), 12.0f * dpi_scale, nullptr, glyph_ranges);
    tab_text2 = io.Fonts->AddFontFromMemoryTTF(&inter, sizeof(inter), 24.0f * dpi_scale, nullptr, glyph_ranges);
    tab_text3 = io.Fonts->AddFontFromMemoryTTF(&inter, sizeof(inter), 40.0f * dpi_scale, nullptr, glyph_ranges);
    ico = io.Fonts->AddFontFromMemoryTTF(&icon, sizeof(icon), 25.0f * dpi_scale, nullptr, glyph_ranges);
    ico_2 = io.Fonts->AddFontFromMemoryTTF(&Menuicon, sizeof(Menuicon), 20.0f * dpi_scale, nullptr, glyph_ranges);
    ico_subtab = io.Fonts->AddFontFromMemoryTTF(&icon, sizeof(icon), 35.0f * dpi_scale, nullptr, glyph_ranges);
    ico_logo = io.Fonts->AddFontFromMemoryTTF(&icon, sizeof(icon), 31.0f * dpi_scale, nullptr, glyph_ranges);
    tab_text = io.Fonts->AddFontFromMemoryTTF(&inter, sizeof(inter), 19.0f * dpi_scale, nullptr, glyph_ranges);
    ico_minimize = io.Fonts->AddFontFromMemoryTTF(&icon, sizeof(icon), 27.0f * dpi_scale, nullptr, glyph_ranges);
    ImGui::StyleColorsDark();

    if (var::debug_console)
    {
        AllocConsole();
        FILE* f;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        std::cout << "[Debug] Console Allocated" << std::endl;
    }

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // auto-login with saved session
    if (load_session(login_username, login_password))
    {
        KeyAuthApp.init();
        KeyAuthApp.login(login_username, login_password);
        if (KeyAuthApp.response.success)
            login = true;
        else
            strcpy_s(login_error_message, KeyAuthApp.response.message.c_str());
    }

    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }

        hide();

        if (done || GetAsyncKeyState(VK_END))
            break;
        if (var::iteration == 0)
            thread1::threadstart();
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (login)
        {
            ImDrawList* bg = ImGui::GetBackgroundDrawList();
            const float cx = screen_width / 2.0f;
            const float cy = screen_height / 2.0f;

            if (var::fovCircle)
            {
                bg->AddCircle(ImVec2(cx, cy), var::fov_radius, ImColor(255, 255, 255, 55), 128, 1.2f);
                if (var::triggerbot)
                    bg->AddCircle(ImVec2(cx, cy), var::trigger_fov * 1.3f, ImColor(255, 100, 100, 100), 32, 1.0f);
                bg->AddCircleFilled(ImVec2(cx, cy), 2.5f, ImColor(255, 255, 255, 200));
                bg->AddCircle(ImVec2(cx, cy), 4.5f, ImColor(0, 0, 0, 160), 8, 1.0f);
            }

            // sniper crosshair overlay
            if (var::weapon_mode == 1)
            {
                const float gap = 7.0f, len = 14.0f, w = 1.5f;
                const ImColor sc(255, 255, 255, 210);
                bg->AddLine(ImVec2(cx - gap - len, cy), ImVec2(cx - gap, cy), sc, w);
                bg->AddLine(ImVec2(cx + gap,       cy), ImVec2(cx + gap + len, cy), sc, w);
                bg->AddLine(ImVec2(cx, cy - gap - len), ImVec2(cx, cy - gap), sc, w);
                bg->AddLine(ImVec2(cx, cy + gap),       ImVec2(cx, cy + gap + len), sc, w);
                bg->AddCircleFilled(ImVec2(cx, cy), 1.5f, sc);
            }

            if (var::esp && var::esp_box_count > 0)
            {
                for (int ei = 0; ei < var::esp_box_count; ++ei)
                {
                    const float bx = (float)var::esp_boxes[ei].x;
                    const float by = (float)var::esp_boxes[ei].y;
                    const float bw = (float)var::esp_boxes[ei].w;
                    const float bh = (float)var::esp_boxes[ei].h;

                    const float depth = (bw < bh ? bw : bh) * 0.22f;
                    const float dx    =  depth * 0.8f;
                    const float dy    = -depth * 0.8f;

                    const ImVec2 bb_tl(bx + dx,      by + dy);
                    const ImVec2 bb_tr(bx + bw + dx, by + dy);
                    const ImVec2 bb_br(bx + bw + dx, by + bh + dy);
                    const ImVec2 bb_bl(bx + dx,      by + bh + dy);

                    const ImVec2 bf_tl(bx,      by);
                    const ImVec2 bf_tr(bx + bw, by);
                    const ImVec2 bf_br(bx + bw, by + bh);
                    const ImVec2 bf_bl(bx,      by + bh);

                    const ImColor c_back(220, 60, 60, 100);
                    bg->AddLine(bb_tl, bb_tr, c_back, 1.0f);
                    bg->AddLine(bb_tr, bb_br, c_back, 1.0f);
                    bg->AddLine(bb_br, bb_bl, c_back, 1.0f);
                    bg->AddLine(bb_bl, bb_tl, c_back, 1.0f);

                    const ImColor c_depth(200, 50, 50, 80);
                    bg->AddLine(bf_tl, bb_tl, c_depth, 1.0f);
                    bg->AddLine(bf_tr, bb_tr, c_depth, 1.0f);
                    bg->AddLine(bf_br, bb_br, c_depth, 1.0f);
                    bg->AddLine(bf_bl, bb_bl, c_depth, 1.0f);

                    // targeted enemy (being aimed at) gets bright white; others slightly dimmer
                    const bool targeted = var::screen_box_w > 0 &&
                        abs(var::esp_boxes[ei].x - var::screen_box_x) < 3 &&
                        abs(var::esp_boxes[ei].y - var::screen_box_y) < 3;
                    const ImColor c_front = targeted ? ImColor(255, 255, 255, 235) : ImColor(200, 200, 200, 160);
                    const float   lw      = targeted ? 2.0f : 1.5f;
                    const float cs = (bw < bh ? bw : bh) * 0.28f;
                    bg->AddLine(bf_tl, ImVec2(bx + cs,      by),       c_front, lw);
                    bg->AddLine(bf_tl, ImVec2(bx,           by + cs),  c_front, lw);
                    bg->AddLine(bf_tr, ImVec2(bx + bw - cs, by),       c_front, lw);
                    bg->AddLine(bf_tr, ImVec2(bx + bw,      by + cs),  c_front, lw);
                    bg->AddLine(bf_br, ImVec2(bx + bw - cs, by + bh),  c_front, lw);
                    bg->AddLine(bf_br, ImVec2(bx + bw,      by+bh-cs), c_front, lw);
                    bg->AddLine(bf_bl, ImVec2(bx + cs,      by + bh),  c_front, lw);
                    bg->AddLine(bf_bl, ImVec2(bx,           by+bh-cs), c_front, lw);
                }
            }

            // radar overlay — bottom-right corner, player at center, enemies as red dots
            if (var::radar)
            {
                const float rsize   = 160.0f;
                const float rmargin = 12.0f;
                const float rx  = (float)screen_width  - rsize - rmargin;
                const float ry  = (float)screen_height - rsize - rmargin;
                const float rcx = rx + rsize * 0.5f;
                const float rcy = ry + rsize * 0.5f;

                bg->AddRectFilled(ImVec2(rx, ry), ImVec2(rx + rsize, ry + rsize), ImColor(0, 0, 0, 170), 8.0f);
                bg->AddRect(ImVec2(rx, ry), ImVec2(rx + rsize, ry + rsize), ImColor(80, 80, 80, 200), 8.0f, 0, 1.2f);
                bg->AddLine(ImVec2(rcx, ry + 6.0f), ImVec2(rcx, ry + rsize - 6.0f), ImColor(40, 40, 40, 120), 0.5f);
                bg->AddLine(ImVec2(rx + 6.0f, rcy), ImVec2(rx + rsize - 6.0f, rcy), ImColor(40, 40, 40, 120), 0.5f);

                // scale: yolo frame (416px) fits into radar with a small margin
                const float dot_scale = (rsize - 20.0f) / 416.0f;

                if (var::fovCircle)
                    bg->AddCircle(ImVec2(rcx, rcy), var::fov_radius * dot_scale, ImColor(255, 255, 255, 45), 48, 0.8f);

                const int cnt = var::radar_enemy_count;
                for (int i = 0; i < cnt; ++i)
                {
                    const float ex = rcx + var::radar_enemies[i].dx * dot_scale;
                    const float ey = rcy + var::radar_enemies[i].dy * dot_scale;
                    if (ex >= rx && ex <= rx + rsize && ey >= ry && ey <= ry + rsize)
                        bg->AddCircleFilled(ImVec2(ex, ey), 3.5f, ImColor(255, 55, 55, 235));
                }

                bg->AddCircleFilled(ImVec2(rcx, rcy), 3.0f, ImColor(80, 210, 80, 235));
                bg->AddText(nullptr, 10.0f, ImVec2(rx + 5.0f, ry + 4.0f), ImColor(130, 130, 130, 200), "RADAR");
            }
        }

        CustomStyleColor();

        D3DX11_IMAGE_LOAD_INFO info;
        ID3DX11ThreadPump* pump = nullptr;

        if (us == nullptr)
            D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, user, sizeof(user), &info, pump, &us, nullptr);

        ImGui::SetNextWindowSize(ImVec2(855.0f * dpi_scale, 650.0f * dpi_scale));
        if (dr1 == nullptr)
            D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, roll, sizeof(roll), &info, pump, &dr1, nullptr);

        if (open)
        {
            if (!login)
            {
                ImGui::SetNextWindowPos(ImVec2((screen_width  - 855.0f * dpi_scale) / 2.0f,
                                              (screen_height - 650.0f * dpi_scale) / 2.0f), ImGuiCond_Appearing);
                ImGui::Begin("Login", &menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                
                const ImVec2& p = ImGui::GetWindowPos();
                ImGuiStyle& s = ImGui::GetStyle();
                
                ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0.0f + p.x, 0.0f + p.y), ImVec2(855.0f * dpi_scale + p.x, 650.0f * dpi_scale + p.y), ImGui::GetColorU32(colors::main_color), s.WindowRounding);
                
                ImGui::SetCursorPos(ImVec2(327.5f * dpi_scale, 50.0f * dpi_scale));
                ImGui::BeginChild("LoginWindow", ImVec2(400.0f * dpi_scale, 550.0f * dpi_scale), true);
                {
                    ImGui::SetCursorPos(ImVec2(150.0f * dpi_scale, 20.0f * dpi_scale));
                    ImGui::Text("Authentication");
                    
                    ImGui::SetCursorPos(ImVec2(50.0f * dpi_scale, 60.0f * dpi_scale));
                    if (ImGui::Button("Login", ImVec2(150.0f * dpi_scale, 40.0f * dpi_scale)))
                        login_tab = 0;
                    
                    ImGui::SameLine();
                    if (ImGui::Button("Register", ImVec2(150.0f * dpi_scale, 40.0f * dpi_scale)))
                        login_tab = 1;
                    
                    ImGui::SetCursorPos(ImVec2(20.0f * dpi_scale, 120.0f * dpi_scale));
                    ImGui::BeginChild("LoginContent", ImVec2(360.0f * dpi_scale, 400.0f * dpi_scale), false);
                    {
                        if (login_tab == 0)
                        {
                            ImGui::Text("Username:");
                            ImGui::InputText("##username", login_username, IM_ARRAYSIZE(login_username));
                            
                            ImGui::Spacing();
                            ImGui::Text("Password:");
                            ImGui::InputText("##password", login_password, IM_ARRAYSIZE(login_password), ImGuiInputTextFlags_Password);
                            
                            ImGui::Spacing();
                            if (strlen(login_error_message) > 0)
                            {
                                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), login_error_message);
                            }
                            
                            ImGui::SetCursorPos(ImVec2(100.0f * dpi_scale, 200.0f * dpi_scale));
                            if (ImGui::Button("Login", ImVec2(160.0f * dpi_scale, 40.0f * dpi_scale)))
                            {
                                KeyAuthApp.init();
                                KeyAuthApp.login(login_username, login_password);
                                
                                if (KeyAuthApp.response.success)
                                {
                                    login = true;
                                    save_session(login_username, login_password);
                                    strcpy_s(login_error_message, "");
                                }
                                else
                                {
                                    strcpy_s(login_error_message, KeyAuthApp.response.message.c_str());
                                }
                            }
                        }
                        else
                        {
                            ImGui::Text("Username:");
                            ImGui::InputText("##reg_username", register_username, IM_ARRAYSIZE(register_username));
                            
                            ImGui::Spacing();
                            ImGui::Text("Password:");
                            ImGui::InputText("##reg_password", register_password, IM_ARRAYSIZE(register_password), ImGuiInputTextFlags_Password);
                            
                            ImGui::Spacing();
                            ImGui::Text("License Key:");
                            ImGui::InputText("##reg_key", register_key, IM_ARRAYSIZE(register_key));
                            
                            ImGui::Spacing();
                            ImGui::Text("Email (Optional):");
                            ImGui::InputText("##reg_email", register_email, IM_ARRAYSIZE(register_email));
                            
                            ImGui::Spacing();
                            if (strlen(login_error_message) > 0)
                            {
                                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), login_error_message);
                            }
                            
                            ImGui::SetCursorPos(ImVec2(100.0f * dpi_scale, 300.0f * dpi_scale));
                            if (ImGui::Button("Register", ImVec2(160.0f * dpi_scale, 40.0f * dpi_scale)))
                            {
                                KeyAuthApp.init();
                                std::string email_str = strlen(register_email) > 0 ? register_email : "";
                                KeyAuthApp.regstr(register_username, register_password, register_key, email_str);
                                
                                if (KeyAuthApp.response.success)
                                {
                                    KeyAuthApp.login(register_username, register_password);
                                    if (KeyAuthApp.response.success)
                                    {
                                        login = true;
                                        save_session(register_username, register_password);
                                        strcpy_s(login_error_message, "");
                                    }
                                    else
                                    {
                                        strcpy_s(login_error_message, "Registration successful but login failed");
                                    }
                                }
                                else
                                {
                                    strcpy_s(login_error_message, KeyAuthApp.response.message.c_str());
                                }
                            }
                        }
                    }
                    ImGui::EndChild();
                }
                ImGui::EndChild();
                ImGui::End();
            }
            else if (login)
            {
                ImGui::SetNextWindowSize(ImVec2(855.0f * dpi_scale, 650.0f * dpi_scale));
                ImGui::SetNextWindowPos(ImVec2((screen_width  - 855.0f * dpi_scale) / 2.0f,
                                              (screen_height - 650.0f * dpi_scale) / 2.0f), ImGuiCond_Appearing);
                ImGui::Begin("Menu", &menu, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                const ImVec2& p = ImGui::GetWindowPos();
                ImGuiStyle& s = ImGui::GetStyle();

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
                ImGui::BeginChild("G-Tab", ImVec2(173.0f * dpi_scale, 790.0f * dpi_scale), false);
                {
                    ImGui::GetForegroundDrawList()->AddText(tab_text3, 20.0f * dpi_scale, ImVec2(20.0f * dpi_scale + p.x, 12.0f * dpi_scale + p.y), ImColor(255, 255, 255, 255), "       Keyvex");
                    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0.0f + p.x, 0.0f + p.y), ImVec2(273.0f * dpi_scale + p.x, 790.0f * dpi_scale + p.y), ImGui::GetColorU32(colors::Tab_Child), s.WindowRounding);

                    ImGui::SetCursorPosY(60.0f);
                    ImGui::SetWindowFontScale(dpi_scale);
                    
                    if (ImGui::Tab("H", "Aimbot", "AI based aimbot", tabs == 0, ImVec2(150.0f * dpi_scale, 42.0f * dpi_scale)))
                        tabs = 0;
                    if (ImGui::Tab("E", "Misc", "Other settings", tabs == 1, ImVec2(150.0f * dpi_scale, 42.0f * dpi_scale)))
                        tabs = 1;
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();

                ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0.0f + p.x, 0.0f + p.y), ImVec2(855.0f * dpi_scale + p.x, 790.0f * dpi_scale + p.y), ImGui::GetColorU32(colors::main_color), s.WindowRounding);
                ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0.0f + p.x, 755.0f * dpi_scale + p.y), ImVec2(855.0f * dpi_scale + p.x, 755.0f * dpi_scale + p.y), ImGui::GetColorU32(colors::lite_color), s.WindowRounding);

                const float delta_time = ImGui::GetIO().DeltaTime;
                tab_alpha = ImClamp(tab_alpha + (7.0f * delta_time * (tabs == active_tab ? 1.0f : -1.0f)), 0.0f, 1.0f);
                tab_add = ImClamp(tab_add + (50.0f * delta_time * (tabs == active_tab ? 1.0f : -1.0f)), 0.0f, 1.0f);

                if (tab_alpha == 0.0f && tab_add == 0.0f)
                    active_tab = tabs;

                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab_alpha * s.Alpha);
                ImGui::SetCursorPos(ImVec2(203.0f * dpi_scale, 30.0f * dpi_scale));

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
                ImGui::BeginChild("General", ImVec2(717.0f * dpi_scale, 650.0f * dpi_scale), false);
                {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetColorU32(colors::lite_color));
                    switch (active_tab)
                    {
                    case 0:
                {
                    ImGui::SetCursorPosY(5.0f * dpi_scale);
                    ImGui::BeginChildPos("ambt", ImVec2(300.0f * dpi_scale, 580.0f * dpi_scale));
                    {
                        ImGui::SetWindowFontScale(dpi_scale);

                        // weapon mode selector
                        ImGui::PushStyleColor(ImGuiCol_Button,        var::weapon_mode == 0 ? ImGui::GetColorU32(ImGuiCol_ButtonActive) : ImGui::GetColorU32(ImGuiCol_Button));
                        if (ImGui::Button("Rifle##wm",  ImVec2(120.0f * dpi_scale, 26.0f * dpi_scale))) var::weapon_mode = 0;
                        ImGui::PopStyleColor();
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Button,        var::weapon_mode == 1 ? ImGui::GetColorU32(ImGuiCol_ButtonActive) : ImGui::GetColorU32(ImGuiCol_Button));
                        if (ImGui::Button("Sniper##wm", ImVec2(120.0f * dpi_scale, 26.0f * dpi_scale))) var::weapon_mode = 1;
                        ImGui::PopStyleColor();

                        ImGui::Checkbox("Aimbot", &var::checkbox);
                        ImGui::Keybind("Keybind", &var::key0, true);

                        // hold / toggle mode
                        ImGui::PushStyleColor(ImGuiCol_Button, !var::aim_toggle ? ImGui::GetColorU32(ImGuiCol_ButtonActive) : ImGui::GetColorU32(ImGuiCol_Button));
                        if (ImGui::Button("Hold##am",   ImVec2(108.0f * dpi_scale, 22.0f * dpi_scale))) var::aim_toggle = false;
                        ImGui::PopStyleColor();
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Button,  var::aim_toggle ? ImGui::GetColorU32(ImGuiCol_ButtonActive) : ImGui::GetColorU32(ImGuiCol_Button));
                        if (ImGui::Button("Toggle##am", ImVec2(108.0f * dpi_scale, 22.0f * dpi_scale))) var::aim_toggle = true;
                        ImGui::PopStyleColor();

                        ImGui::Separator();
                        ImGui::SliderFloat("Sensitivity",   &var::sensitivity,       0.1f, 5.0f,  "%.2fx", 0);
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                            ImGui::SetTooltip("Mais alto = mais rapido. >=2.0 com Smooth<=15 = snap instantaneo");
                        ImGui::SliderFloat("Smoothness",    &var::smooth,            1.0f, 100.0f,"%.1f",  0);
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                            ImGui::SetTooltip("Mais baixo = mais rapido. 1-15 = snap, 30-60 = suave, 60+ = humano");
                        ImGui::Checkbox("Natural Movement", &var::natural_aim);
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                            ImGui::SetTooltip("Micro-variacao sinusoidal — parece movimento humano");
                        ImGui::Checkbox("Sticky Aim", &var::sticky_aim);
                        if (var::sticky_aim)
                            ImGui::SliderFloat("Sticky Radius", &var::sticky_radius, 20.0f, 200.0f, "%.0f px", 0);
                        ImGui::SliderFloat("Aim Height",    &var::aim_height,        1.0f, 100.0f,"%.0f%%", 0);

                        ImGui::Separator();
                        ImGui::SliderFloat("FOV Radius",    &var::fov_radius,        50.0f,500.0f,"%.0f px", 0);
                        ImGui::SliderFloat("Confidence",    &var::confidence,        0.05f,0.80f, "%.2f",    0);
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                            ImGui::SetTooltip("Mais baixo = deteta mais (mais falsos positivos). Mais alto = so alvos seguros.");
                        ImGui::SliderInt("Scan FPS",        &var::scannFPS,          1,    250,   "%d FPS",  0);
                        ImGui::Checkbox("Color Aim",        &var::color_aim);
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                            ImGui::SetTooltip("Afina cabeca via name tag vermelho / head class do modelo");
                        ImGui::Checkbox("Show ESP",         &var::esp);
                        ImGui::Checkbox("Show FOV",         &var::fovCircle);
                        ImGui::Checkbox("Show Radar",       &var::radar);

                        ImGui::Separator();
                        ImGui::Checkbox("Anti-Spray",       &var::no_recoil);
                        if (var::no_recoil)
                        {
                            ImGui::SliderInt("Fire Rate",     &var::fire_rate,       100, 1200, "%d RPM", 0);
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                                ImGui::SetTooltip("RPM dentro do burst (Type 95 = 800)");
                            ImGui::SliderInt("Burst Size",    &var::burst_size,        0,   10, var::burst_size == 0 ? "Auto" : "%d shots", 0);
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                                ImGui::SetTooltip("Tiros por burst — 0 = full auto, 3 = Type 95");
                            ImGui::SliderFloat("Spray Y",     &var::recoil_strength, 0.5f, 8.0f, "%.1f", 0);
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                                ImGui::SetTooltip("Compensacao vertical — aumenta ate os tiros nao subirem");
                            ImGui::SliderFloat("Spray X",     &var::recoil_x,        -4.0f, 4.0f, "%.2f", 0);
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                                ImGui::SetTooltip("Deriva horizontal: negativo = esquerda, positivo = direita");
                        }

                        ImGui::Separator();
                        ImGui::Checkbox("Triggerbot",       &var::triggerbot);
                        if (var::triggerbot)
                        {
                            ImGui::Keybind("Trigger Key",     &var::trigger_key, true);
                            ImGui::SliderFloat("Trigger FOV", &var::trigger_fov,      1.0f, 30.0f, "%.1f px", 0);
                            ImGui::SliderInt("Trigger Delay", &var::trigger_delay_ms, 0,    500,   "%d ms",   0);
                            ImGui::Checkbox("Pause on LMB",  &var::anti_spray);
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                                ImGui::SetTooltip("Pausa o triggerbot quando seguras LMB manualmente");
                        }
                    }
                    ImGui::EndChild();

                    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0.0f + p.x, 0.0f + p.y), ImVec2(855.0f + p.x, 790.0f + p.y), ImGui::GetColorU32(colors::main_color), s.WindowRounding);
                    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0.0f + p.x, 759.0f + p.y), ImVec2(855.0f + p.x, 790.0f + p.y), ImGui::GetColorU32(colors::lite_color), s.WindowRounding);

                    if (dr == nullptr)
                        D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, dragon, sizeof(dragon), &info, pump, &dr, nullptr);
                    
                    ImGui::SetCursorPos(ImVec2(320.0f * dpi_scale, 5.0f * dpi_scale));
                    ImGui::BeginChildPos("Visual Preview", ImVec2(300.0f * dpi_scale, 580.0f * dpi_scale));
                    {
                        ImGui::SetWindowFontScale(dpi_scale);
                        ImGui::SetCursorPosY(135.0f);
                        ImGui::SetCursorPosX(10.0f);
                        ImGui::VSliderFloat(" ", ImVec2(30.0f, 415.0f), &var::aim_height, 0.0f, 100.0f, "", 0);
                        
                        ImVec2 pos = ImGui::GetWindowPos();
                        ImDrawList* draw = ImGui::GetWindowDrawList();

                        // --- Soldado FPS desenhado com ImGui ---
                        {
                            const float cx  = pos.x + 150.0f;
                            const float ct2 = pos.y + 85.0f;
                            const float ch  = 480.0f;

                            // fundo / sombra
                            draw->AddRectFilled(ImVec2(cx - 70, ct2), ImVec2(cx + 70, ct2 + ch), ImColor(20, 22, 28, 200), 8.0f);

                            // capacete
                            const float hr = ch * 0.072f;
                            const float hcy = ct2 + hr + 2.0f;
                            draw->AddRectFilled(ImVec2(cx - hr * 1.1f, ct2), ImVec2(cx + hr * 1.1f, hcy - hr * 0.3f), ImColor(45, 55, 35, 255), 4.0f);
                            // viseira
                            draw->AddRectFilled(ImVec2(cx - hr * 0.9f, hcy - hr * 0.6f), ImVec2(cx + hr * 0.9f, hcy - hr * 0.1f), ImColor(30, 35, 25, 255));
                            // cara
                            draw->AddCircleFilled(ImVec2(cx, hcy), hr, ImColor(195, 165, 130, 255));
                            draw->AddCircle(ImVec2(cx, hcy), hr, ImColor(60, 50, 40, 180), 32, 1.2f);

                            // pescoço
                            const float neck_t = hcy + hr;
                            const float neck_b = neck_t + ch * 0.025f;
                            draw->AddRectFilled(ImVec2(cx - hr * 0.35f, neck_t), ImVec2(cx + hr * 0.35f, neck_b), ImColor(195, 165, 130, 220));

                            // corpo / colete
                            const float body_t  = neck_b;
                            const float body_b  = ct2 + ch * 0.58f;
                            const float body_hw = ch * 0.13f;
                            draw->AddRectFilled(ImVec2(cx - body_hw, body_t), ImVec2(cx + body_hw, body_b), ImColor(55, 70, 40, 255), 5.0f);
                            // detalhe colete
                            draw->AddLine(ImVec2(cx, body_t + 4), ImVec2(cx, body_b - 4), ImColor(40, 50, 28, 180), 1.5f);
                            draw->AddLine(ImVec2(cx - body_hw + 4, body_t + ch*0.08f), ImVec2(cx + body_hw - 4, body_t + ch*0.08f), ImColor(40, 50, 28, 150), 1.0f);

                            // braços
                            const float arm_w  = ch * 0.055f;
                            const float arm_t  = body_t + ch * 0.01f;
                            const float arm_b  = body_t + ch * 0.28f;
                            draw->AddRectFilled(ImVec2(cx - body_hw - arm_w, arm_t), ImVec2(cx - body_hw, arm_b), ImColor(55, 70, 40, 240), 3.0f);
                            draw->AddRectFilled(ImVec2(cx + body_hw, arm_t), ImVec2(cx + body_hw + arm_w, arm_b), ImColor(55, 70, 40, 240), 3.0f);
                            // luvas
                            draw->AddRectFilled(ImVec2(cx - body_hw - arm_w, arm_b), ImVec2(cx - body_hw, arm_b + ch*0.04f), ImColor(30, 30, 30, 240), 2.0f);
                            draw->AddRectFilled(ImVec2(cx + body_hw, arm_b), ImVec2(cx + body_hw + arm_w, arm_b + ch*0.04f), ImColor(30, 30, 30, 240), 2.0f);

                            // pernas
                            const float leg_t   = body_b;
                            const float leg_b   = ct2 + ch * 0.94f;
                            const float leg_hw  = ch * 0.065f;
                            const float leg_gap = ch * 0.018f;
                            draw->AddRectFilled(ImVec2(cx - leg_hw - leg_gap, leg_t), ImVec2(cx - leg_gap, leg_b), ImColor(40, 50, 30, 255), 3.0f);
                            draw->AddRectFilled(ImVec2(cx + leg_gap, leg_t), ImVec2(cx + leg_hw + leg_gap, leg_b), ImColor(40, 50, 30, 255), 3.0f);
                            // botas
                            const float boot_b = ct2 + ch * 0.98f;
                            draw->AddRectFilled(ImVec2(cx - leg_hw - leg_gap - 4, leg_b), ImVec2(cx - leg_gap + 6, boot_b), ImColor(25, 25, 25, 255), 2.0f);
                            draw->AddRectFilled(ImVec2(cx + leg_gap - 6, leg_b), ImVec2(cx + leg_hw + leg_gap + 4, boot_b), ImColor(25, 25, 25, 255), 2.0f);
                        }

                        // aim height indicator
                        {
                            const float img_top    = pos.y + 85.0f;
                            const float img_bottom = pos.y + 85.0f + 480.0f;
                            const float img_left   = pos.x + 60.0f;
                            const float img_right  = pos.x + 240.0f;
                            const float aim_y = img_top + (1.0f - var::aim_height / 100.0f) * (img_bottom - img_top);

                            draw->AddLine(ImVec2(img_left, aim_y), ImVec2(img_right, aim_y), ImColor(255, 50, 50, 200), 1.5f);
                            draw->AddTriangleFilled(
                                ImVec2(img_left, aim_y),
                                ImVec2(img_left + 10.0f, aim_y - 5.0f),
                                ImVec2(img_left + 10.0f, aim_y + 5.0f),
                                ImColor(255, 80, 80, 240));

                            const char* label =
                                var::aim_height >= 90.f ? "Head" :
                                var::aim_height >= 78.f ? "Neck" :
                                var::aim_height >= 58.f ? "Chest" :
                                var::aim_height >= 38.f ? "Stomach" : "Legs";
                            draw->AddText(ImVec2(img_left + 14.0f, aim_y - 13.0f), ImColor(255, 120, 120, 240), label);
                        }

                        const ImVec2 preview_center = ImVec2(pos.x + 150.0f, pos.y + 330.0f);

                        if (var::fovCircle)
                        {
                            const float preview_scale = 260.0f / (float)screen_width;
                            draw->AddCircle(preview_center, var::fov_radius * preview_scale, ImColor(255, 255, 255, 60), 64, 1.0f);
                            if (var::triggerbot)
                                draw->AddCircle(preview_center, var::trigger_fov * 1.3f * preview_scale, ImColor(255, 100, 100, 100), 32, 1.0f);
                            draw->AddCircleFilled(preview_center, 2.0f, ImColor(255, 255, 255, 180));
                        }

                        if (var::esp)
                        {
                            ImGui::SetCursorPos(ImVec2(55.0f, 90.0f));
                            const ImVec2 p1 = ImGui::GetCursorScreenPos();
                            const float pw = 180.0f, ph = 400.0f;
                            const float pdx = pw * 0.14f, pdy = -pw * 0.14f;
                            const ImColor pc_back(220, 60, 60, 100);
                            const ImColor pc_dep(200, 50, 50, 70);
                            const ImColor pc_front(255, 255, 255, 200);
                            draw->AddLine(ImVec2(p1.x+pdx,    p1.y+pdy),    ImVec2(p1.x+pw+pdx, p1.y+pdy),    pc_back, 1.0f);
                            draw->AddLine(ImVec2(p1.x+pw+pdx, p1.y+pdy),    ImVec2(p1.x+pw+pdx, p1.y+ph+pdy), pc_back, 1.0f);
                            draw->AddLine(ImVec2(p1.x+pw+pdx, p1.y+ph+pdy), ImVec2(p1.x+pdx,    p1.y+ph+pdy), pc_back, 1.0f);
                            draw->AddLine(ImVec2(p1.x+pdx,    p1.y+ph+pdy), ImVec2(p1.x+pdx,    p1.y+pdy),    pc_back, 1.0f);
                            draw->AddLine(p1,                         ImVec2(p1.x+pdx,    p1.y+pdy),    pc_dep, 1.0f);
                            draw->AddLine(ImVec2(p1.x+pw, p1.y),     ImVec2(p1.x+pw+pdx, p1.y+pdy),    pc_dep, 1.0f);
                            draw->AddLine(ImVec2(p1.x+pw, p1.y+ph),  ImVec2(p1.x+pw+pdx, p1.y+ph+pdy), pc_dep, 1.0f);
                            draw->AddLine(ImVec2(p1.x,    p1.y+ph),  ImVec2(p1.x+pdx,    p1.y+ph+pdy), pc_dep, 1.0f);
                            const float cs2 = pw * 0.22f;
                            draw->AddLine(p1,                        ImVec2(p1.x+cs2, p1.y),       pc_front, 2.0f);
                            draw->AddLine(p1,                        ImVec2(p1.x,     p1.y+cs2),   pc_front, 2.0f);
                            draw->AddLine(ImVec2(p1.x+pw, p1.y),    ImVec2(p1.x+pw-cs2, p1.y),    pc_front, 2.0f);
                            draw->AddLine(ImVec2(p1.x+pw, p1.y),    ImVec2(p1.x+pw, p1.y+cs2),    pc_front, 2.0f);
                            draw->AddLine(ImVec2(p1.x+pw, p1.y+ph), ImVec2(p1.x+pw-cs2, p1.y+ph), pc_front, 2.0f);
                            draw->AddLine(ImVec2(p1.x+pw, p1.y+ph), ImVec2(p1.x+pw, p1.y+ph-cs2), pc_front, 2.0f);
                            draw->AddLine(ImVec2(p1.x,    p1.y+ph), ImVec2(p1.x+cs2, p1.y+ph),    pc_front, 2.0f);
                            draw->AddLine(ImVec2(p1.x,    p1.y+ph), ImVec2(p1.x,     p1.y+ph-cs2),pc_front, 2.0f);
                        }
                    }
                    ImGui::EndChild();
                }
                break;
                case 1:
                {
                    ImGui::BeginChildPos("", ImVec2(620.0f * dpi_scale, 100.0f * dpi_scale));
                    {
                        ImGui::GetForegroundDrawList()->AddText(tab_text3, 26.0f * dpi_scale, ImVec2(450.0f * dpi_scale + p.x, 55.0f * dpi_scale + p.y), ImColor(255, 255, 255, 255), "Miscellaneous");
                        ImGui::GetForegroundDrawList()->AddText(tab_text3, 16.0f * dpi_scale, ImVec2(390.0f * dpi_scale + p.x, 85.0f * dpi_scale + p.y), ImColor(255, 255, 255, 255), "Modify menu games and other functions");
                    }
                    ImGui::EndChild();
                    ImGui::SetCursorPosY(120.0f * dpi_scale);
                    ImGui::BeginChildPos("Misc", ImVec2(620.0f * dpi_scale, 490.0f * dpi_scale));
                    {
                        ImGui::SetWindowFontScale(dpi_scale);
                        ImGui::Checkbox("Animated background", &animated_background);
                        ImGui::Keybind("Hide menu", &var::key4, true);

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Model:    %s", var::model_name.c_str());
                        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Backend:  %s", var::detection_backend.c_str());
                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.7f), "Insert = show/hide menu");
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.7f), "End    = exit");
                    }
                    ImGui::EndChild();
                }
                break;
                }
                    ImGui::PopStyleColor();
                    ImGui::Spacing();
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                }

                if (animated_background)
                    Particles();

                ImGui::PopStyleVar();
                
                const ImVec2 window_pos = ImGui::GetWindowPos();
                const ImVec2 window_size = ImGui::GetWindowSize();
                ImGui::GetForegroundDrawList()->AddText(nullptr, 11.0f * dpi_scale, 
                    ImVec2(window_pos.x + window_size.x - 120.0f * dpi_scale, window_pos.y + window_size.y - 20.0f * dpi_scale),
                    ImColor(100, 100, 100, 150), var::detection_backend.c_str());
                
                ImGui::End();
            }
        }

        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassA(wc.lpszClassName, wc.hInstance);
    return 0;
}

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = { 0 };
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    
    if (FAILED(res))
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain)
    {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dDeviceContext)
    {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView)
    {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return 1;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = static_cast<UINT>(LOWORD(lParam));
        g_ResizeHeight = static_cast<UINT>(HIWORD(lParam));
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
