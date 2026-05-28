#include "mainA.h"
#include "opencv2/opencv.hpp"
#include "main.h"
#include <Windows.h>
#include <cmath>
#include "mouse_interface.h"

static mouse_interface mouse;

void aimbot::trigger_shoot(bool target_in_zone)
{
    if (!var::triggerbot) return;
    if (!(GetAsyncKeyState(var::trigger_key) & 0x8000)) return;

    // anti-spray: pause if user is manually holding LMB (not us)
    if (var::anti_spray && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) && !var::triggerbot_firing)
        return;

    static int confirm_frames = 0;
    static DWORD last_shot = 0;

    if (target_in_zone)
        confirm_frames++;
    else
        confirm_frames = 0;

    const DWORD now = GetTickCount();
    const DWORD cooldown = 150;

    if (confirm_frames >= 3 && (now - last_shot) > cooldown)
    {
        if (var::trigger_delay_ms > 0)
            Sleep(static_cast<DWORD>(var::trigger_delay_ms));
        var::triggerbot_firing = true;
        mouse.left_down();
        Sleep(12);
        mouse.left_up();
        var::triggerbot_firing = false;
        last_shot = now;
        confirm_frames = 0;
    }
}

void aimbot::no_recoil()
{
    if (!var::no_recoil) return;

    static double accumulator = 0.0;

    if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000))
    {
        accumulator = 0.0;
        return;
    }

    accumulator += var::recoil_strength;
    const int move = static_cast<int>(accumulator);
    if (move > 0)
    {
        mouse.move(0, move);
        accumulator -= move;
    }
}

void aimbot::aim_to(int x, int y, int box_w, int box_h)
{
    static const int screen_width  = GetSystemMetrics(SM_CXSCREEN);
    static const int screen_height = GetSystemMetrics(SM_CYSCREEN);

    x = screen_width  / 2 - ACTIVATION_RANGE / 2 + x + box_w / 2;
    y = screen_height / 2 - ACTIVATION_RANGE / 2 + y + (int)(box_h * (1.0f - var::aim_height / 100.0f));

    const int x_off = x - screen_width  / 2;
    const int y_off = y - screen_height / 2;

    static double x_acc  = 0.0, y_acc  = 0.0;  // smooth accumulator
    static double frac_x = 0.0, frac_y = 0.0;  // sub-pixel accumulator
    static double vel_x  = 0.0, vel_y  = 0.0;  // velocity
    static int    prev_x = 0,   prev_y = 0;
    static DWORD  last_t = 0;

    const DWORD now = GetTickCount();
    if (now - last_t > 150)
    {
        x_acc = y_acc = frac_x = frac_y = vel_x = vel_y = 0.0;
        prev_x = x_off;
        prev_y = y_off;
    }
    last_t = now;

    // velocity (responsive EMA)
    vel_x = 0.5 * vel_x + 0.5 * (x_off - prev_x);
    vel_y = 0.5 * vel_y + 0.5 * (y_off - prev_y);
    prev_x = x_off;
    prev_y = y_off;

    // deadzone
    if (abs(x_off) <= 2 && abs(y_off) <= 2)
    {
        x_acc = y_acc = frac_x = frac_y = 0.0;
        return;
    }

    const double dist = sqrt((double)(x_off * x_off + y_off * y_off));
    const double lead = var::sticky_aim ? 0.65 : 0.40;
    const double spd  = (var::smooth > 0.1 ? var::smooth : 0.1) / 8.0
                        / (var::sensitivity > 0.01f ? (double)var::sensitivity : 0.01);

    const double raw_x = (x_off + vel_x * lead) / spd;
    const double raw_y = (y_off + vel_y * lead) / spd;

    // single clean EMA (no dynamic_smooth complexity)
    x_acc = var::smoothing_factor * x_acc + (1.0 - var::smoothing_factor) * raw_x;
    y_acc = var::smoothing_factor * y_acc + (1.0 - var::smoothing_factor) * raw_y;

    // overshoot prevention — cap step to 95% of remaining distance
    const double step = sqrt(x_acc * x_acc + y_acc * y_acc);
    const double cap  = dist * 0.95;
    if (step > cap && step > 0.01)
    {
        const double s = cap / step;
        x_acc *= s;
        y_acc *= s;
    }

    // natural movement — smooth sinusoidal micro-variation
    double out_x = x_acc, out_y = y_acc;
    if (var::natural_aim && dist > 8.0)
    {
        static const DWORD t0 = GetTickCount();
        const double t = (GetTickCount() - t0) / 1000.0;
        out_x += sin(t * 7.3 + 1.1) * 0.30;
        out_y += sin(t * 6.1 + 2.7) * 0.30;
    }

    // sub-pixel accumulator — never lose <1px movements
    frac_x += out_x;
    frac_y += out_y;
    const int mx = static_cast<int>(frac_x);
    const int my = static_cast<int>(frac_y);
    frac_x -= mx;
    frac_y -= my;

    if (mx != 0 || my != 0)
        mouse.move(mx, my);
}
