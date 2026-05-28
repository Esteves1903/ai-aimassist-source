#include <opencv2/opencv.hpp>
#include "screenshot.h"
#include "scanner.h"
#include "mainA.h"
#include <filesystem>
#include "defines.h"
#include "main.h"

static std::wstring GetCurrentDirectory()
{
    wchar_t buffer[MAX_PATH] = { 0 };
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
    return std::wstring(buffer).substr(0, pos);
}

static DWORD WINAPI RunningThread(LPVOID)
{
    screenshot screen;
    cv::Mat image = screen.get();
    const std::wstring current_directory = GetCurrentDirectory();
    const std::string str_exe_path(current_directory.begin(), current_directory.end());

    detector detect(
        str_exe_path + "\\" + LABELS_FILE_NAME,
        str_exe_path + "\\" + YOLO_CFG_FILE_NAME,
        str_exe_path + "\\" + YOLO_WEIGHTS_FILE_NAME
    );

    var::detection_backend = detect.getBackendName();

    Sleep(1000);

    // warm up GPU so first real detection has no delay
    for (int i = 0; i < 10; ++i)
    {
        image = screen.get();
        detect.start(image);
        cv::waitKey(1);
    }

    DWORD last_frame = 0;

    while (true)
    {
        thread1::POC();

        const DWORD now = GetTickCount();
        const DWORD interval = 1000 / (var::scannFPS > 0 ? var::scannFPS : 60);
        if (now - last_frame < interval)
        {
            Sleep(1);
            continue;
        }
        last_frame = now;

        aimbot::no_recoil();

        {
            static bool key_was_down = false;
            static bool aim_toggled  = false;
            const bool key_down = (GetAsyncKeyState(var::key0) & 0x8000) || (var::LTrigger && var::checkbox);
            const bool manual_lmb = var::anti_spray && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) && !var::triggerbot_firing;

            if (var::aim_toggle)
            {
                if (key_down && !key_was_down)
                    aim_toggled = !aim_toggled;
                var::aim_active = aim_toggled && !manual_lmb;
            }
            else
            {
                var::aim_active = key_down && !manual_lmb;
            }
            key_was_down = key_down;
        }

        image = screen.get();
        detect.start(image);
        cv::waitKey(1);
    }

    return 0;
}

void thread1::threadstart()
{
    if (var::iteration == 0)
    {
        var::iteration = 1;
        CreateThread(nullptr, 0, RunningThread, nullptr, 0, nullptr);
    }
}
