#ifdef _WIN32

#include "init.hpp"

#include <mmdeviceapi.h>

std::mutex Win32Co::m;
size_t Win32Co::count = 0;

Win32Co::Win32Co() {
    std::lock_guard<std::mutex> lock(m);
    if ( count == 0 )
        CoInitialize(NULL);
    count ++;
}

Win32Co::~Win32Co() {
    std::lock_guard<std::mutex> lock(m);
    count --;
    if ( count == 0 )
        CoUninitialize();
}

#endif