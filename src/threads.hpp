/**
 * @file threads.hpp
 * @brief Provides @b Thread class
 */
#ifndef SIMPLY_THREAD_HPP_
#define SIMPLY_THREAD_HPP_

#include <string>
#include <exception>
#include <memory>

/// @typedef callback_t
/// @brief The method that is called by the thread
/// @param data Passed by pointer to the thread
/// @return 0 indicates success
typedef int (*callback_t)(void* data);

/**
 * @class ThreadException
 * @brief This is the base class of all exceptions thrown by @b Thread
 */
class ThreadException: public std::exception {
    protected:
        std::string msg;
        explicit ThreadException(const std::string& msg): msg(msg) {}
    
    public:
        /// @brief Get a message describing the error
        /// @return NULL-terminated c-string
        const char* what() const noexcept override { return msg.c_str(); }
};

/**
 * @class ThreadUserError
 * @brief This means some @b Thread operations were used in incorrect order/combination
 */
class ThreadUserError: public ThreadException {
    public:
        explicit ThreadUserError(const std::string& msg): ThreadException("ThreadUserError: " + msg) {}
};

/**
 * @class ThreadRuntimeError
 * @brief This means that the library or your system failed to handle a valid operation
 */
class ThreadRuntimeError: ThreadException {
    public:
        explicit ThreadRuntimeError(const std::string& msg): ThreadException("ThreadRuntimeError: " + msg) {}
};

/**
 * @class ThreadExited
 * @brief This means that your valid request can't be handled as the thread already completed
 */
class ThreadExited: ThreadException {
    public:
        explicit ThreadExited(const std::string& msg): ThreadException("ThreadExited: " + msg) {}
};

/**
 * @class Thread
 * @brief Start and handle threads in a simple, (soon to be) cross-platform class
 * 
 * This was built despite there being the `std::thread` as I need control
 * over the thread's priority due to audio processing's real-time constraint.
 * 
 * @todo Consider how to add the following
 * - Suspend / resume
 * - Sleep / switch / yield
 * - Affinity (which CPUs to use)
 * - Thread ID
 * - Process priority/name/ID/etc.
 * - Thread status (kernel/user time, etc.)
 * - Other thread operations
 * 
 * @todo See std::rethrow_exception and std::exception_ptr ON class???
 * 
 * @todo Fill out all operations supported by @b std::thread and @b std::jthread
 */
class Thread {
    protected:
        struct Impl;
        std::unique_ptr<Impl> pimpl;

    public:
        /**
         * @enum Priority
         * @brief To abstract away OS-specific thread priority levels
         * @note In Windows this is used in tandem with a "Process Priority"
         * @note My readings indicate macOS handles REAL_TIME differently
         */
        enum Priority {
            /// Lowest priority
            LOWEST,   
            /// Low/below normal
            LOW,      
            /// Normal
            NORMAL,   
            /// High/above normal
            HIGH,     
            /// Highest before real-time
            HIGHEST,  
            /// Real-time / critical priority
            REAL_TIME 
        };

        /// @brief Construct an empty instance
        Thread();

        /// @brief Creates, but does not run, thread
        Thread(callback_t callback, void* data=nullptr);
        
        /// @brief Destructor will join any non-detached/non-terminated threads
        ~Thread();

        // Finishing off the rule of 3
        Thread(const Thread&) = delete;
        Thread& operator=(const Thread&) = delete;

        // Finishing off the rule of 5
        /// @brief Move constructor to move around unique threads
        Thread(Thread&& o);

        /// @brief Move operator to move around unique threads
        Thread& operator=(Thread&& o);

        /// @brief Create, but don't start thread with callback
        /// This will join any non-detached/non-terminated threads
        void create(callback_t callback, void* data=nullptr);

        /// @brief Set the thread priority
        void set_priority(Priority priority);

        /// @brief Start the created thread
        void start();

        /// @brief Suspend execution of a thread
        void suspend();

        /// @brief Resume execution of a suspended thread
        void resume();

        /// @brief Check if this has a thread that has been started
        bool started() const;

        /// @brief Check if this thread is currently running
        bool running() const;

        /// @brief Check if this thread is currently suspended
        bool suspended() const;

        /// @brief Check if this thread is completed
        bool completed() const;

        /// @brief Detach the thread so this class doesn't join on destruction
        void detach();

        /// @brief Terminate the thread forcefully
        /// @warning This might cause memory leaks depending on OS-specific implementation
        void terminate(int exit_code=0);

        /// @brief Join the running/started thread, blocking infinitely
        /// @returns `true` if successfully joined (should always be case)
        void join();

        /// @brief Join the running/started thread, with finite blocking
        /// If the thread is suspended, it will be resumed (and if try_join
        /// returns `false`, it will not be re-suspended afterwards)
        /// @param ms Milliseconds to block for
        /// @returns `true` if successfully joined
        bool try_join(size_t ms);

        /// @brief Get the exit/return code of the callback after joining
        /// @returns Value returned by the callback method IFF already joined
        /// @throws ThreadRuntimeError if thread crashed/threw exception
        /// @throws ThreadUserError if no thread/thread not joined
        /// @throws Any exceptions not caught by @b callback
        int exit_code();
};

#endif // SIMPLY_THREAD_HPP_