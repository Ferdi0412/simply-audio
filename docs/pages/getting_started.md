@page getting_started Getting Started

# Requirements
@todo verify these
- C++17 or later
- CMake 3.12 or later
- Windows (Linux & macOS support is planned after initial Windows release)

# Example

## Thread
The audio will be handled internally using callbacks and dedicated audio threads with *real-time* priority. As such Thread is provided to simplify working with threads.

\code{.cpp}
#include <iostream>
#include <simply_audio.hpp>

int callback(void*) {
    /* Add logic... */

    if ( condition )
        throw std::runtime_error("");
    
    return 0;
}

int main() {
    Thread thread(callback, nullptr);
    
    // Must be explicitly started
    thread.start();

    // .try_join blocks for 500ms to join
    while ( !thread.try_join(500) ) {
        std::cout << "Still trying to join..." << std::endl;
    }

    // If in callback std::runtime_error is thrown,
    // .exit_code will throw std::runtime_error
    // Otherwise if 0 is returned by callback,
    // .exit_code will return 0
    std::cout << "Thread returned with code "
              << std::to_string(thread.exit_code())
              << std::endl;

    return 0;
}
\endcode