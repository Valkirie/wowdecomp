#ifndef ENGINE_SOURCE_OS_GAMEPAD_H
#define ENGINE_SOURCE_OS_GAMEPAD_H

#include <cstdint>

void OsGamepadDestroy();
char* OsGamepadGetName(uint32_t device, char* buffer, uint32_t size);
int32_t OsGamepadInitialize();
void OsGamepadPoll(int32_t active);
uint32_t OsGamepadCount();
uint32_t OsGamepadGetDevice(uint32_t index);
uint32_t OsGamepadGetSelectedDevice();
uint32_t OsGamepadGetSelectedIndex();
uint32_t OsGamepadGetType(uint32_t index);
int32_t OsGamepadSelect(uint32_t index);

#endif
