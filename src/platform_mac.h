#pragma once

#ifdef __APPLE__
// Forward declare the SFML window handle type
#include <cstdint>

void setupMacWindow(void* windowHandle);
#endif