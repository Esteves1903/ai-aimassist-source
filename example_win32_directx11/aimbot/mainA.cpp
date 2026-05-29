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

    const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

    static bool   was_lmb     = false;
    static DWORD  fire_start  = 0;
    static int    last_shot_n = -1;
    static double acc_x       = 0.0;
    static double acc_y       = 0.0;

    if (!lmb)
    {
        was_lmb     = false;
        last_shot_n = -1;
        acc_x = acc_y = 0.0;
        return;
    }

    const DWORD now = GetTickCount();

    if (!was_lmb)
    {
        fire_start  = now;
        last_shot_n = -1;
        was_lmb     = true;
        acc_x = acc_y = 0.0;
        return; // primeiro frame = primeiro tiro, sem recoil ainda
    }

    const double shot_ms    = 60000.0 / (var::fire_rate > 1 ? (double)var::fire_rate : 1.0);
    const int    total_shot = (int)((now - fire_start) / shot_ms);

    // já compensámos este tiro
    if (total_shot == last_shot_n) return;
    last_shot_n = total_shot;

    // posição dentro do burst (0 = primeiro tiro = sem recoil)
    const int burst      = var::burst_size > 0 ? var::burst_size : 999;
    const int in_burst   = total_shot % burst;

    if (in_burst == 0) return; // primeiro tiro de cada burst — sem recoil

    // recoil escala por posição no burst: tiro 2 = 1.0x, tiro 3 = 1.4x, etc.
    const double scale = 1.0 + (in_burst - 1) * 0.40;

    acc_y += var::recoil_strength * scale;
    acc_x += var::recoil_x        * scale;

    const int move_y = static_cast<int>(acc_y);
    const int move_x = static_cast<int>(acc_x);
    if (move_y != 0 || move_x != 0)
    {
        mouse.move(move_x, move_y);
        acc_y -= move_y;
        acc_x -= move_x;
    }
}

void aimbot::aim_to(int x, int y, int box_w, int box_h)
{
    const int screen_width  = GetSystemMetrics(SM_CXSCREEN);
    const int screen_height = GetSystemMetrics(SM_CYSCREEN);

    x = screen_width  / 2 - ACTIVATION_RANGE / 2 + x + box_w / 2;
    y = screen_height / 2 - ACTIVATION_RANGE / 2 + y + (int)(box_h * (1.0f - var::aim_height / 100.0f));

    const int x_off = x - screen_width  / 2;
    const int y_off = y - screen_height / 2;

    static double x_acc  = 0.0, y_acc  = 0.0;
    static double frac_x = 0.0, frac_y = 0.0;
    static double vel_x  = 0.0, vel_y  = 0.0;
    static int    prev_x = 0,   prev_y = 0;
    static DWORD  last_t = 0;

    const DWORD now = GetTickCount();
    const double jump = sqrt((double)((x_off - prev_x) * (x_off - prev_x) + (y_off - prev_y) * (y_off - prev_y)));

    // reset completo se target perdido (>200ms) ou troca brusca de alvo (>180px)
    if (now - last_t > 200 || jump > 180.0)
    {
        x_acc = y_acc = frac_x = frac_y = vel_x = vel_y = 0.0;
        prev_x = x_off;
        prev_y = y_off;
    }
    last_t = now;

    // velocity EMA — 0.6/0.4 mais estável que 0.5/0.5
    vel_x = 0.6 * vel_x + 0.4 * (x_off - prev_x);
    vel_y = 0.6 * vel_y + 0.4 * (y_off - prev_y);
    prev_x = x_off;
    prev_y = y_off;

    // deadzone 3px — evita micro-oscilações no alvo
    if (abs(x_off) <= 3 && abs(y_off) <= 3)
    {
        x_acc = y_acc = frac_x = frac_y = 0.0;
        return;
    }

    const double dist = sqrt((double)(x_off * x_off + y_off * y_off));
    const double spd  = (var::smooth > 0.1 ? var::smooth : 0.1) / 8.0
                        / (var::sensitivity > 0.01f ? (double)var::sensitivity : 0.01);

    // lead constante e pequeno — seguro com YOLO jitter (contribui <1px),
    // útil para movimento real (target a 10px/frame → +1.5px de previsão)
    const double lead  = var::sticky_aim ? 0.20 : 0.15;
    const double raw_x = (x_off + vel_x * lead) / spd;
    const double raw_y = (y_off + vel_y * lead) / spd;

    x_acc = (double)var::smoothing_factor * x_acc + (1.0 - (double)var::smoothing_factor) * raw_x;
    y_acc = (double)var::smoothing_factor * y_acc + (1.0 - (double)var::smoothing_factor) * raw_y;

    // overshoot cap — máximo 95% da distância restante por frame
    const double step = sqrt(x_acc * x_acc + y_acc * y_acc);
    const double cap  = dist * 0.95;
    if (step > cap && step > 0.01)
    {
        const double s = cap / step;
        x_acc *= s;
        y_acc *= s;
    }

    // micro-variação natural
    double out_x = x_acc, out_y = y_acc;
    if (var::natural_aim && dist > 8.0)
    {
        static const DWORD t0 = GetTickCount();
        const double t = (GetTickCount() - t0) / 1000.0;
        out_x += sin(t * 7.3 + 1.1) * 0.25;
        out_y += sin(t * 6.1 + 2.7) * 0.25;
    }

    // sub-pixel accumulator
    frac_x += out_x;
    frac_y += out_y;
    const int mx = static_cast<int>(frac_x);
    const int my = static_cast<int>(frac_y);
    frac_x -= mx;
    frac_y -= my;

    if (mx != 0 || my != 0)
        mouse.move(mx, my);
}
