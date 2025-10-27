#ifndef SIMPLY_INIT_HPP_
#define SIMPLY_INIT_HPP_

#ifdef _WIN32
#include <mutex>

/**
 * @class Win32Co
 * @brief RAII implementation to load/unload the WIN32 Com libraries
 */
class Win32Co {
    public:
        /// @brief If count is 0, run `CoInitialize`
        Win32Co();
        
        /// @brief If count goes to 0, run `CoUninitialize`
        ~Win32Co();

    private:
        static std::mutex m;
        static size_t count;
};
#endif

#endif // SIMPLY_INIT_HPP_