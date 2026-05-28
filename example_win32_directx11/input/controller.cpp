#include <Windows.h>
#include <Xinput.h>
#pragma comment(lib, "XInput.lib")
#include "main.h"

class XboxController
{
private:
    XINPUT_STATE m_controllerState = { 0 };
    int m_controllerNum = 0;

public:
    XboxController(int playerNumber) : m_controllerNum(playerNumber - 1) { }

    XINPUT_STATE GetControllerState()
    {
        ZeroMemory(&m_controllerState, sizeof(XINPUT_STATE));
        XInputGetState(m_controllerNum, &m_controllerState);
        return m_controllerState;
    }

    bool CheckConnection()
    {
        ZeroMemory(&m_controllerState, sizeof(XINPUT_STATE));
        return XInputGetState(m_controllerNum, &m_controllerState) == ERROR_SUCCESS;
    }
};

static XboxController* s_player1 = new XboxController(1);

void thread1::POC()
{
    XINPUT_STATE state;
    ZeroMemory(&state, sizeof(XINPUT_STATE));
    if (XInputGetState(0, &state) == ERROR_SUCCESS)
    {
        var::RTrigger = state.Gamepad.bRightTrigger > 0;
        var::LTrigger = state.Gamepad.bLeftTrigger > 0;
    }
}
