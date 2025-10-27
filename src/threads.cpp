#include "threads.hpp"

#include <atomic>
#include <mutex>

#ifdef _WIN32
extern "C" {
    #include <windows.h>
}

using thread_t = HANDLE;

#else
    #error "Not implemented other than for Windows!"
#endif

// ======= Thread Context ====== 
// This class is owned by shared pointer, but as it will act as a
// to/from Thread and actual thread, atomic variables will be used to
// synchronize the critical sections
struct ThreadContext {
    // These are the values that must be set before creating the thread
    callback_t callback;
    void*      data;

    // These will only be written to from inside the thread
    std::atomic<bool> started   = false;
    std::atomic<bool> completed = false;

    // These are only read after completed is completed
    std::exception_ptr exc       = nullptr;
    int                exit_code = 0;

    ThreadContext(callback_t c, void* d): callback(c), data(d) {}

    /// @todo Add implementation for cleanup of any data
};

// ====== Thread Implementation ======
// This class is owned by the Thread class, and will detach any running
// thread on destruction - for desired join behavior, it must
// be called explicitly
struct Thread::Impl {
    std::shared_ptr<ThreadContext> context;
    thread_t                       thread;
    bool                           _suspended = false;
    bool                           _joined    = false;

    // Is true even if completed
    bool started() {
        return context->started;
    }
    
    bool completed() {
        return context->completed;
    }

    bool running() {
        return started() && !completed() && !_suspended;
    }

    bool suspended() {
        return started() && !completed() && _suspended;
    }

    bool joined() {
        return _joined;
    }

    #ifdef _WIN32
    // Note - create and win_thread should only be called inside this,
    //        others should use these by constructing instances of this
    //        class
    private:
        static unsigned __stdcall win_thread(void* ctx) {
            // Implements shared_ptr's copy constructor on Impl.context
            std::shared_ptr<ThreadContext> context = *static_cast<std::shared_ptr<ThreadContext>*>(ctx);
            
            context->started = true;
            
            try {
                context->exit_code = context->callback(context->data);
            } catch ( ... ) {
                context->exc = std::current_exception();
                context->exit_code = -1;
            }

            context->completed = true;
            return context->exit_code;
        }

        void create(callback_t callback, void* data) {
            context = std::make_shared<ThreadContext>(callback, data);
            thread = (thread_t)_beginthreadex(
                nullptr,          // security
                0,                // 
                win_thread,       // callback/method
                &context,         // ctx
                CREATE_SUSPENDED, // flags
                nullptr           // thread address
            );
            if ( thread == NULL )
                throw ThreadRuntimeError("NULL thread created!");
        }
    
    public:
        void set_priority(Priority priority) {
            if ( running() )
                throw ThreadUserError("Cannot set priority on running thread!");
            int nPriority;
            switch ( priority ) {
                case LOWEST:
                    nPriority = THREAD_PRIORITY_LOWEST;
                    break;

                case LOW:
                    nPriority = THREAD_PRIORITY_BELOW_NORMAL;
                    break;
                
                case NORMAL:
                    nPriority = THREAD_PRIORITY_NORMAL;
                    break;

                case HIGH:
                    nPriority = THREAD_PRIORITY_ABOVE_NORMAL;
                    break;
                    
                case HIGHEST:
                    nPriority = THREAD_PRIORITY_HIGHEST;
                    break;
            };
            if ( !SetThreadPriority(thread, nPriority) )
                throw ThreadRuntimeError("Failed to set priority...");
        }

        void start() {
            if ( context->started )
                throw ThreadUserError("Cannot start thread more than once!");
            if ( ResumeThread(thread) == -1 )
                throw ThreadRuntimeError("Failed to start thread!");
            // Wait until thread started, in case user wants to detach,
            // which could cause an issue wherein context's memory
            // is re-allocated
            while ( !started() )
                ; 
            /// @todo - Improve the waiting for thread to start loop
        }
    
        void suspend() {
            if ( !running() )
                if ( !started() )
                    throw ThreadUserError("Cannot suspend an unstarted thread!");
                else if ( completed() )
                    throw ThreadExited("Thread already completed!");
                else
                    throw ThreadUserError("Thread already suspended!");
            if ( SuspendThread(thread) == -1 )
                throw ThreadRuntimeError("Failed to suspend thread!");
            _suspended = true;
        }

        void resume() {
            if ( !suspended() )
                if ( !started() )
                    throw ThreadUserError("Cannot resume an unstarted thread!");
                else if ( completed() )
                    throw ThreadExited("Thread already completed!");
                else
                    throw ThreadUserError("Thread not suspended!");
            if ( SuspendThread(thread) == -1 )
                throw ThreadRuntimeError("Failed to resume thread!");
            _suspended = false;
        }

    private:
        // Note - this should never be called outside this class
        void detach() {
            if ( thread != NULL )
                CloseHandle(thread);
        }

    public:
        void terminate(int exit_code) {
            if ( completed() || !started() )
                if ( completed() )
                    throw ThreadExited("Thread already completed!");
                else
                    throw ThreadUserError("Cannot terminate an unstarted thread!");
            if ( !TerminateThread(thread, exit_code) )
                throw ThreadRuntimeError("Failed to terminate thread!");
        }

        bool try_join(size_t ms) {
            if ( joined() )
                throw ThreadUserError("Cannot join more than once!");
            else if ( !started() )
                throw ThreadUserError("Cannot join until a thread has started!");
            else if ( suspended() )
                resume();

            switch ( WaitForSingleObject(thread, ms) ) {
                case WAIT_OBJECT_0:
                    _joined = true;
                    return true; // Success
                
                case WAIT_TIMEOUT:
                    _joined = false;
                    return false; // Timeout

                default:
                    throw ThreadRuntimeError("Failed to join!");
            }
        }

        void join() {
            try_join(INFINITE);
        }

        int exit_code() {
            if ( !joined() )
                throw ThreadUserError("Cannot retrieve exit code until the thread has joined!");
            
            if ( context->exc )
                std::rethrow_exception(context->exc);
            
            return context->exit_code;
        }
    #endif // _WIN32

    Impl(callback_t callback, void* user_data) {
        create(callback, user_data);
    }

    ~Impl() {
        detach();
    }
};

// ====== Thread Class Methods ======
Thread::Thread() = default;

Thread::Thread(callback_t callback, void* data) {
    create(callback, data);
}

Thread::~Thread() {
    if ( pimpl )
        try {
            pimpl->join();
        }
        catch ( ... ) { ; }
}

Thread::Thread(Thread&& o) {
    pimpl = std::move(o.pimpl);
}

Thread& Thread::operator=(Thread&& o) {
    /// @todo Make pimpl have a "safe join" for these types of operations
    if ( pimpl )
        pimpl->join();
    pimpl = std::move(o.pimpl);
    return *this;
}

void Thread::create(callback_t callback, void* data) {
    if ( pimpl )
        throw ThreadUserError("Cannot create on top of existing thread!");
    pimpl = std::make_unique<Impl>(callback, data);
}

void Thread::set_priority(Priority priority) {
    if ( !pimpl )
        throw ThreadUserError("Cannot set priority without a thread!");
    pimpl->set_priority(priority);
}

void Thread::start() {
    if ( !pimpl )
        throw ThreadUserError("Cannot start without a thread!");
    pimpl->start();
}

void Thread::suspend() {
    if ( !pimpl )
        throw ThreadUserError("Cannot suspend without a thread!");
    pimpl->suspend();
}

void Thread::resume() {
    if ( !pimpl )
        throw ThreadUserError("Cannot resume without a thread!");
    pimpl->resume();
}

bool Thread::started() const {
    if ( !pimpl )
        throw ThreadUserError("No thread!");
    return pimpl->started();
}

bool Thread::running() const {
    if ( !pimpl )
        throw ThreadUserError("No thread!");
    return pimpl->running();
}

bool Thread::suspended() const {
    if ( !pimpl )
        throw ThreadUserError("No thread!");
    return pimpl->suspended();
}

bool Thread::completed() const {
    if ( !pimpl )
        throw ThreadUserError("No thread!");
    return pimpl->completed();
}

void Thread::detach() {
    pimpl = nullptr;
}

void Thread::terminate(int exit_code) {
    if ( !pimpl )
        throw ThreadUserError("Cannot terminate without a thread!");
    pimpl->terminate(exit_code);
    pimpl = nullptr;
}

void Thread::join() {
    if ( !pimpl )
        throw ThreadUserError("Cannot join without a thread!");
    pimpl->join();
}

bool Thread::try_join(size_t ms) {
    if ( !pimpl )
        throw ThreadUserError("Cannot join without a thread!");
    return pimpl->try_join(ms);
}

int Thread::exit_code() {
    if ( !pimpl )
        throw ThreadUserError("Cannot get exit code without a thread!");
    return pimpl->exit_code();
}