// Helper classes to implement RAII lifetimes for System APIs
//
// Currently only implemented for Windows
#ifndef UTILS_CPP_
#define UTILS_CPP_

#ifdef _WIN32
// ---------------------------------------------------------------------
// --- WINDOWS IMPLEMENTATION --- --------------------------------------
//   EXAMPLE
// /* === Ensure Windows COM library is initialized for block === */
// Win32Co             cominit;    // Load & init libraries
// 
// WinDeviceEnumerator enumerator;
// CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
//                  enumerator.uuid(), (void**) &enumerator);
// 
// WinDevice device;
// enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &device);
// 
// WinPropertyStore propstore;
// device->OpenPropertyStore(STGM_READ, &propstore);
//
// /* === Read device's name using WinPropVariant === */
// std::cout << WinPropVariant::device_name(propstore) << std::endl;
//
// WinAudioClient client;
// device->Activate(client.uuid(), CLSCTX_ALL, nullptr, (void**) &client);
// 
// WinWaveFormatEx fmt;
// client->GetMixFormat(&fmt);
// std::cout << std::to_string(fmt->nSamplesPerSec) << " Hz" << std::endl;
// std::cout << std::to_string(fmt->nChannels) << " channels" << std::endl;
// std::cout << std::to_string(fmt->wBitsPerSample) << " bits" << std::endl;
#include "utils.hpp"

#include <mutex>
#include <utility>
#include <string>

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

// ---------------------------------------------------------------------
// --- String Conversion --- -------------------------------------------
std::string wide_to_utf8(const wchar_t* wide_str) {
    if ( !wide_str )
        return "";
    
    int size = WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, nullptr, 0, nullptr, nullptr);
    if ( size <= 0 )
        return "";
    
    std::string res(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, &res[0], size, nullptr, nullptr);
    return res;
}

// ---------------------------------------------------------------------
// --- Win32Co --- -----------------------------------------------------
// Win32Co is provided in "utils.hpp"
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

// ---------------------------------------------------------------------
// --- Windows MM Interface Helpers --- --------------------------------
// WinPtr        -> RAII lifetime management for example for IMMDevice
//   &winptr          - pointer to underlying/wrapped pointer
//                       - analagous to std::unique_ptr's .put, but:
//                       - will clear existing instance, as this is only
//                         used by Windows when creating new instances
//   winptr->         - same behaviour as in std::unique_ptr
//   winptr.get()     - same behaviour as in std::unique_ptr
//   winptr.release() - same behaviour as in std::unique_ptr
//   if (winptr)      - same behaviour as in std::unique_ptr
// IUDeleter     -> WinPtr Deleter for Windows classes starting with I
// CoTaskDeleter -> For classes needing CoTaskMemFree

// Will work for any classes implementing Window's `IUnknown`
template <typename T>
struct IUDeleter {
    void operator()(T*& ptr) const {
        if (ptr)
            ptr->Release();
        ptr = nullptr;
    }
};

// This is used for COM structs that use `CoTaskMemFree`, such as `WAVEFORMATEX`
template <typename T>
struct CoTaskDeleter {
    void operator()(T*& ptr) const {
        if (ptr)
            CoTaskMemFree(ptr);
        ptr = nullptr;
    }
};

// Used for any Windows pointer-based classes for RAII cleanup
template <typename T, typename Deleter = IUDeleter<T>>
class WinPtr {
    private:
        T* ptr = nullptr;
        Deleter deleter;

        void clear() {
            deleter(ptr);
        }
    
    public:
        WinPtr() = default;

        ~WinPtr() {
            clear();
        }

        WinPtr(const WinPtr&) = delete;
        WinPtr& operator=(const WinPtr&) = delete;

        WinPtr(WinPtr&& o) {
            std::swap(ptr, o.ptr);
        }

        WinPtr& operator=(WinPtr&& o) {
            if (ptr == o.ptr ) {
                o.ptr = nullptr;
                return *this;
            }
            clear();
            std::swap(ptr, o.ptr);
            return *this;
        }

        void swap(WinPtr& o) {
            std::swap(ptr, o.ptr);
        }

        // This will only ever be used when creating instances
        T** operator&() {
            clear();
            return &ptr;
        }

        T* operator->() {
            return ptr;
        }

        T* get() {
            return ptr;
        }

        T* release() {
            T* temp = nullptr;
            std::swap(ptr, temp);
            return temp;
        }

        explicit operator bool() const {
            return ptr != nullptr;
        }

        static inline const IID& uuid() {
            return __uuidof(T);
        }
};

// ---------------------------------------------------------------------
// --- Windows MM Interface Aliases --- --------------------------------
// For IMMDeviceEnumerator
using WinDeviceEnumerator = WinPtr<IMMDeviceEnumerator,
                                   IUDeleter<IMMDeviceEnumerator>>;
// For IMMDevice
using WinDevice           = WinPtr<IMMDevice,
                                   IUDeleter<IMMDevice>>;
// For IAudioClient
using WinAudioClient      = WinPtr<IAudioClient,
                                   IUDeleter<IAudioClient>>;
// For IAudioCaptureClient
using WinAudioCapture     = WinPtr<IAudioCaptureClient,
                                   IUDeleter<IAudioCaptureClient>>;
// For IAudioRenderClient
using WinAudioRender      = WinPtr<IAudioRenderClient,
                                   IUDeleter<IAudioRenderClient>>;
// For IPropertyStore
using WinPropertyStore    = WinPtr<IPropertyStore,
                                   IUDeleter<IPropertyStore>>;
// For WAVEFORMATEX
using WinWaveFormatEx     = WinPtr<WAVEFORMATEX,
                                   CoTaskDeleter<WAVEFORMATEX>>;

// ---------------------------------------------------------------------
// --- Windows HANDLE Helper --- ---------------------------------------
// WinHandle -> RAII lifetime management for HANDLE in Windows
//   &handle          - pointer to the underlying handle
//                       - analagous to std::unique_ptr's .put, but:
//                       - will clear existing isntance, as this is 
//                         mainly used to create new instances
//   handle.get()     - same behaviour as in std::unique_ptr
//   handle.release() - same behaviour as in std::unique_ptr
//   if (handle)      - same behaviour as in std::unique_ptr
class WinHandle {
    private:
        HANDLE handle = NULL;

        bool truthy() const {
            return handle != NULL && handle != INVALID_HANDLE_VALUE;
        }

        void clear() {
            if ( truthy() )
                CloseHandle(handle);
            handle = NULL;
        }

    public:
        WinHandle() = default;

        WinHandle(HANDLE h): handle(h) { }

        WinHandle& operator=(HANDLE h) {
            clear();
            handle = h;
            return *this;
        }

        ~WinHandle() {
            clear();
        }

        WinHandle(WinHandle&& o) {
            std::swap(handle, o.handle);
        }

        WinHandle& operator=(WinHandle&& o) {
            if (o.handle == handle) {
                o.handle = NULL;
                return *this;
            }
            clear();
            std::swap(handle, o.handle);
            return *this;
        }

        HANDLE* operator&() {
            clear();
            return &handle;
        }

        HANDLE get() {
            return handle;
        }

        HANDLE release() {
            HANDLE temp = NULL;
            std::swap(handle, temp);
            return temp;
        }

        explicit operator bool() const {
            return truthy();
        }
};

// ---------------------------------------------------------------------
// --- Windows PROPVARIANT Helper --- ----------------------------------
// WinPropVariant -> Aims to simplify extracting properties from a
//                   WinPropertyStore instance
// - It's construction is suppressed
// 
//   NOTE
// - This class is not meant to be constructed, treat as if it were
//   a namespace:
// - The rationale for this was that I found working PROPVARIANT
//   very verbose, and this was a fun implementation, lol 
class WinPropVariant {
    private:
        PROPVARIANT var;

        void init() {
            PropVariantInit(&var);
        }

        void clear() {
            PropVariantClear(&var);
        }

        WinPropVariant() { init(); }
        ~WinPropVariant() { clear(); }
    
        PROPVARIANT* operator&() {
            return &var;
        }

    public:
        static std::string device_name(WinPropertyStore& prop) {
            WinPropVariant pv;
            prop->GetValue(PKEY_Device_FriendlyName, &pv);
            return wide_to_utf8(pv.var.pwszVal);
        }
};
#else // _WIN32
    #error "utils.cpp only setup for Windows!"
#endif // _WIN32 || else

#endif // UTILS_CPP_