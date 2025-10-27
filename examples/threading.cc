#include "threads.hpp"
#include <iostream>

int callback1(void* data) {
    int* counter = static_cast<int*>(data);
    for (int i = 0; i < 1000000; i++) {
        (*counter)++;
    }
    return 42;
}

int callback2(void* data) {
    throw std::runtime_error("");
    return 0;
}

int main() {
    /* callback1 */
    int counter = 0;
    Thread t1(callback1, &counter);
    t1.start();
    t1.join();

    std::cout << "Counter: " << counter << std::endl;
    std::cout << "Exit code: " << t1.exit_code() << std::endl;
    std::cout << ((t1.exit_code() == 42 && counter == 1000000) ? "SUCCESS" : "FAILED") << std::endl;

    Thread t2(callback2, &counter);
    t2.start();
    t2.join();

    try { 
        t2.exit_code();
        std::cout << "Exception throw failed!" << std::endl;
    } 
    catch ( ... ) {
        std::cout << "Successfully threw and passed on exception!" << std::endl;
    }
    return 0;
}