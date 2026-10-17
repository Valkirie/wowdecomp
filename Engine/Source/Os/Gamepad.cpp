#include "Engine/Source/Os/Gamepad.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include "Engine/Source/Event/Types.h"
#include "Engine/Source/Os/Input.h"
#include "Engine/Source/Os/TimeManager.h"

namespace {

struct OsGamepad {
	uint32_t m_device;
	SDL_Gamepad* m_gamepad;
	GAMEPAD_TYPE m_type;
	std::array<bool, GAMEPAD_BUTTONS> m_buttons;
	std::array<int16_t, GAMEPAD_AXES> m_axes;
	std::string m_mapping;
};

bool s_initialized;
uint32_t s_selectedDevice = 0xFFFFFFFF;
std::vector<OsGamepad> s_gamepads;

constexpr std::array<SDL_GamepadButton, GAMEPAD_BUTTON_LEFT_TRIGGER> s_sdlButtons = {
	SDL_GAMEPAD_BUTTON_SOUTH,
	SDL_GAMEPAD_BUTTON_EAST,
	SDL_GAMEPAD_BUTTON_WEST,
	SDL_GAMEPAD_BUTTON_NORTH,
	SDL_GAMEPAD_BUTTON_BACK,
	SDL_GAMEPAD_BUTTON_GUIDE,
	SDL_GAMEPAD_BUTTON_START,
	SDL_GAMEPAD_BUTTON_LEFT_STICK,
	SDL_GAMEPAD_BUTTON_RIGHT_STICK,
	SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
	SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
	SDL_GAMEPAD_BUTTON_DPAD_UP,
	SDL_GAMEPAD_BUTTON_DPAD_DOWN,
	SDL_GAMEPAD_BUTTON_DPAD_LEFT,
	SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
	SDL_GAMEPAD_BUTTON_MISC1,
	SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,
	SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
	SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,
	SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,
	SDL_GAMEPAD_BUTTON_TOUCHPAD,
	SDL_GAMEPAD_BUTTON_MISC2,
	SDL_GAMEPAD_BUTTON_MISC3,
	SDL_GAMEPAD_BUTTON_MISC4,
	SDL_GAMEPAD_BUTTON_MISC5,
	SDL_GAMEPAD_BUTTON_MISC6,
};

constexpr std::array<SDL_GamepadAxis, GAMEPAD_AXES> s_sdlAxes = {
	SDL_GAMEPAD_AXIS_LEFTX,
	SDL_GAMEPAD_AXIS_LEFTY,
	SDL_GAMEPAD_AXIS_RIGHTX,
	SDL_GAMEPAD_AXIS_RIGHTY,
	SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
	SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
};

GAMEPAD_TYPE GetGamepadType(SDL_Gamepad* gamepad) {
	switch (SDL_GetGamepadType(gamepad)) {
		case SDL_GAMEPAD_TYPE_STANDARD:
			return GAMEPAD_TYPE_STANDARD;
		case SDL_GAMEPAD_TYPE_XBOX360:
			return GAMEPAD_TYPE_XBOX360;
		case SDL_GAMEPAD_TYPE_XBOXONE:
			return GAMEPAD_TYPE_XBOXONE;
		case SDL_GAMEPAD_TYPE_PS3:
			return GAMEPAD_TYPE_PS3;
		case SDL_GAMEPAD_TYPE_PS4:
			return GAMEPAD_TYPE_PS4;
		case SDL_GAMEPAD_TYPE_PS5:
			return GAMEPAD_TYPE_PS5;
		case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
			return GAMEPAD_TYPE_SWITCH_PRO;
		case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
			return GAMEPAD_TYPE_JOYCON_LEFT;
		case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
			return GAMEPAD_TYPE_JOYCON_RIGHT;
		case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
			return GAMEPAD_TYPE_JOYCON_PAIR;
		default:
			return GAMEPAD_TYPE_UNKNOWN;
	}
}

std::string GetGamepadMapping(SDL_Gamepad* gamepad) {
	char* mapping = SDL_GetGamepadMapping(gamepad);

	if (!mapping) {
		return {};
	}

	std::string result(mapping);
	SDL_free(mapping);
	return result;
}

OsGamepad* FindGamepad(uint32_t device) {
	for (OsGamepad& gamepad : s_gamepads) {
		if (gamepad.m_device == device) {
			return &gamepad;
		}
	}

	return nullptr;
}

bool ContainsDevice(const SDL_JoystickID* devices, int32_t count, uint32_t device) {
	for (int32_t i = 0; i < count; i++) {
		if (static_cast<uint32_t>(devices[i]) == device) {
			return true;
		}
	}

	return false;
}

void ReleaseGamepadState(OsGamepad& gamepad, uint32_t time) {
	for (uint32_t button = 0; button < GAMEPAD_BUTTONS; button++) {
		if (gamepad.m_buttons[button]) {
			gamepad.m_buttons[button] = false;
			OsQueuePut(OS_INPUT_GAMEPAD_BUTTON_UP, gamepad.m_device, button, 0, time);
		}
	}

	for (uint32_t axis = 0; axis < GAMEPAD_AXES; axis++) {
		if (gamepad.m_axes[axis]) {
			gamepad.m_axes[axis] = 0;
			OsQueuePut(OS_INPUT_GAMEPAD_AXIS, gamepad.m_device, axis, 0, time);
		}
	}
}

void AddGamepad(SDL_JoystickID device, uint32_t time) {
	if (FindGamepad(static_cast<uint32_t>(device))) {
		return;
	}

	SDL_Gamepad* handle = SDL_OpenGamepad(device);

	if (!handle) {
		return;
	}

	OsGamepad gamepad = {};
	gamepad.m_device = static_cast<uint32_t>(device);
	gamepad.m_gamepad = handle;
	gamepad.m_type = GetGamepadType(handle);
	gamepad.m_mapping = GetGamepadMapping(handle);
	s_gamepads.push_back(gamepad);

	if (s_selectedDevice == 0xFFFFFFFF) {
		s_selectedDevice = gamepad.m_device;
	}

	OsQueuePut(OS_INPUT_GAMEPAD_DEVICE, gamepad.m_device, GAMEPAD_DEVICE_ADDED, gamepad.m_type, time);
}

void RemoveDisconnectedGamepads(const SDL_JoystickID* devices, int32_t count, uint32_t time) {
	for (auto it = s_gamepads.begin(); it != s_gamepads.end();) {
		if (SDL_GamepadConnected(it->m_gamepad) && ContainsDevice(devices, count, it->m_device)) {
			++it;
			continue;
		}

		ReleaseGamepadState(*it, time);
		OsQueuePut(OS_INPUT_GAMEPAD_DEVICE, it->m_device, GAMEPAD_DEVICE_REMOVED, it->m_type, time);
		SDL_CloseGamepad(it->m_gamepad);
		it = s_gamepads.erase(it);

		if (s_selectedDevice != 0xFFFFFFFF && !FindGamepad(s_selectedDevice)) {
			s_selectedDevice = s_gamepads.empty() ? 0xFFFFFFFF : s_gamepads.front().m_device;
		}
	}
}

void PollGamepad(OsGamepad& gamepad, uint32_t time) {
	std::string mapping = GetGamepadMapping(gamepad.m_gamepad);

	if (mapping != gamepad.m_mapping) {
		gamepad.m_mapping = std::move(mapping);
		gamepad.m_type = GetGamepadType(gamepad.m_gamepad);
		OsQueuePut(OS_INPUT_GAMEPAD_DEVICE, gamepad.m_device, GAMEPAD_DEVICE_REMAPPED, gamepad.m_type, time);
	}

	for (uint32_t button = 0; button < s_sdlButtons.size(); button++) {
		bool pressed = SDL_GetGamepadButton(gamepad.m_gamepad, s_sdlButtons[button]);

		if (pressed != gamepad.m_buttons[button]) {
			gamepad.m_buttons[button] = pressed;
			OsQueuePut(pressed ? OS_INPUT_GAMEPAD_BUTTON_DOWN : OS_INPUT_GAMEPAD_BUTTON_UP, gamepad.m_device, button, pressed, time);
		}
	}

	for (uint32_t axis = 0; axis < GAMEPAD_AXES; axis++) {
		int16_t value = SDL_GetGamepadAxis(gamepad.m_gamepad, s_sdlAxes[axis]);
		int32_t delta = static_cast<int32_t>(value) - gamepad.m_axes[axis];

		if (std::abs(delta) >= 128 || (!value && gamepad.m_axes[axis])) {
			gamepad.m_axes[axis] = value;
			OsQueuePut(OS_INPUT_GAMEPAD_AXIS, gamepad.m_device, axis, value, time);
		}

		if (axis == GAMEPAD_AXIS_LEFT_TRIGGER || axis == GAMEPAD_AXIS_RIGHT_TRIGGER) {
			uint32_t button = axis == GAMEPAD_AXIS_LEFT_TRIGGER ? GAMEPAD_BUTTON_LEFT_TRIGGER : GAMEPAD_BUTTON_RIGHT_TRIGGER;
			bool pressed = gamepad.m_buttons[button] ? value > 8000 : value > 12000;

			if (pressed != gamepad.m_buttons[button]) {
				gamepad.m_buttons[button] = pressed;
				OsQueuePut(pressed ? OS_INPUT_GAMEPAD_BUTTON_DOWN : OS_INPUT_GAMEPAD_BUTTON_UP, gamepad.m_device, button, pressed, time);
			}
		}
	}
}

}

int32_t OsGamepadInitialize() {
	if (s_initialized) {
		return 1;
	}

	if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
		return 0;
	}

	s_initialized = true;
	return 1;
}

void OsGamepadDestroy() {
	if (!s_initialized) {
		return;
	}

	for (OsGamepad& gamepad : s_gamepads) {
		SDL_CloseGamepad(gamepad.m_gamepad);
	}

	s_gamepads.clear();
	s_selectedDevice = 0xFFFFFFFF;
	SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
	s_initialized = false;
}

void OsGamepadPoll(int32_t active) {
	if (!s_initialized) {
		return;
	}

	SDL_UpdateGamepads();

	int32_t count = 0;
	SDL_JoystickID* devices = SDL_GetGamepads(&count);
	uint32_t time = OsGetAsyncTimeMs();

	if (devices) {
		for (int32_t i = 0; i < count; i++) {
			AddGamepad(devices[i], time);
		}
	}

	RemoveDisconnectedGamepads(devices, count, time);
	SDL_free(devices);

	for (OsGamepad& gamepad : s_gamepads) {
		if (active) {
			PollGamepad(gamepad, time);
		} else {
			ReleaseGamepadState(gamepad, time);
		}
	}
}

uint32_t OsGamepadCount() {
	return static_cast<uint32_t>(s_gamepads.size());
}

uint32_t OsGamepadGetDevice(uint32_t index) {
	return index < s_gamepads.size() ? s_gamepads[index].m_device : 0xFFFFFFFF;
}

uint32_t OsGamepadGetSelectedDevice() {
	return s_selectedDevice;
}

uint32_t OsGamepadGetSelectedIndex() {
	for (uint32_t i = 0; i < s_gamepads.size(); i++) {
		if (s_gamepads[i].m_device == s_selectedDevice) {
			return i;
		}
	}

	return 0xFFFFFFFF;
}

uint32_t OsGamepadGetType(uint32_t index) {
	return index < s_gamepads.size() ? s_gamepads[index].m_type : GAMEPAD_TYPE_UNKNOWN;
}

int32_t OsGamepadSelect(uint32_t index) {
	if (index >= s_gamepads.size()) {
		return 0;
	}

	uint32_t time = OsGetAsyncTimeMs();

	if (OsGamepad* selected = FindGamepad(s_selectedDevice)) {
		ReleaseGamepadState(*selected, time);
	}

	s_selectedDevice = s_gamepads[index].m_device;
	ReleaseGamepadState(s_gamepads[index], time);
	return 1;
}

char* OsGamepadGetName(uint32_t device, char* buffer, uint32_t size) {
	OsGamepad* gamepad = FindGamepad(device);

	if (!gamepad || !buffer || !size) {
		return nullptr;
	}

	const char* name = SDL_GetGamepadName(gamepad->m_gamepad);

	if (!name) {
		name = "Unknown Gamepad";
	}

	std::strncpy(buffer, name, size - 1);
	buffer[size - 1] = '\0';
	return buffer;
}
