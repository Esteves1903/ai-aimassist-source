#pragma once
#include <Windows.h>
#include "defines.h"

class aimbot
{
public:
    static void aim_to(int x, int y, int box_w, int box_h);
    static void no_recoil();
    static void trigger_shoot(bool target_in_zone);
};
