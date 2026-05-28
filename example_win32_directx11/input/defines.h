#pragma once
#include <windows.h>

enum InjectedInputMouseOptions
{
    left_up = 4,
    left_down = 2,
    right_up = 8,
    right_down = 16,
    none = 0
};

struct InjectedInputMouseInfo
{
    int move_direction_x;
    int move_direction_y;
    unsigned int mouse_data;
    InjectedInputMouseOptions mouse_options;
    unsigned int time_offset_in_miliseconds;
    void* extra_info;
};