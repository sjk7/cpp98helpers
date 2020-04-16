// This is an independent project of an individual developer. Dear PVS-Studio,
// please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// http://www.viva64.com
#pragma once
// my_concurrent.h

#ifndef NO_COPY_CLASS
#define NO_COPY_CLASS(TypeName)                                                \
    TypeName(const TypeName&);                                                 \
    void operator=(const TypeName&);
#endif

#ifndef PLS_NO_WINDOWS_H
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmsystem.h>
#endif
#include <assert.h>

#ifndef TRACE
#define TRACE printf
#endif

#if _MSC_VER > 1899
#define MY_NO_EXCEPT noexcept
#else
#define NO_EXCEPT
#endif

namespace concurrent {
// returns the initial value of value
template <typename T>
static inline T safe_read_value(volatile T value) MY_NO_EXCEPT {
#if _MSC_VER > 1899
    static_assert(!std::is_integral_v<T>,
        "safe_read_value 32 or 64 bit integer types only.");
#endif
    assert("Primary template should not be called" == 0);
    return 0;
}
template <>
static inline LONG safe_read_value(volatile LONG value) MY_NO_EXCEPT {
    return InterlockedExchangeAdd(&value, 0);
}
template <>
static inline ULONG safe_read_value(volatile ULONG value) MY_NO_EXCEPT {
    return InterlockedExchangeAdd((volatile LONG*)&value, 0);
}
template <>
static inline ULONGLONG safe_read_value(volatile ULONGLONG value) MY_NO_EXCEPT {
    return InterlockedExchangeAdd64((volatile LONGLONG*)&value, 0);
}
template <>
static inline LONGLONG safe_read_value(volatile LONGLONG value) MY_NO_EXCEPT {
    return InterlockedExchangeAdd64(&value, 0);
}

// returns the initial value of value
static inline LONGLONG safe_read_value(volatile LONGLONG value) MY_NO_EXCEPT {
    return InterlockedExchangeAdd64(&value, 0);
}

template <typename T>
static inline T safe_write_value(volatile T& target, T new_value) MY_NO_EXCEPT {
#if _MSC_VER > 1899
    static_assert(!std::is_integral_v<T>,
        "safe_write_value: Use 32 or 64 bit integer types only.");
#endif
    assert("Primary template should not be called" == 0);
}
// returns the initial value of new_value
template <>
static inline LONG safe_write_value(
    volatile LONG& target, LONG new_value) MY_NO_EXCEPT {
    return InterlockedExchange(&target, new_value);
}
template <>
static inline ULONG safe_write_value(
    volatile ULONG& target, ULONG new_value) MY_NO_EXCEPT {
    return InterlockedExchange((LONG*)&target, new_value);
}

template <>
// returns the initial value of new_value
static inline LONGLONG safe_write_value(
    volatile LONGLONG& target, LONGLONG new_value) MY_NO_EXCEPT {
    return InterlockedExchange64(&target, new_value);
}
template <>
// returns the initial value of new_value
static inline ULONGLONG safe_write_value(
    volatile ULONGLONG& target, ULONGLONG new_value) MY_NO_EXCEPT {
    return InterlockedExchange64((LONGLONG*)&target, new_value);
}

template <typename T> class atomic_int {
    private:
    T m_val;
    inline void setval(T newval) {
        safe_write_value(m_val, newval);
        assert(m_val = newval);
    }

    public:
    NO_COPY_CLASS(atomic_int);
    explicit atomic_int() : m_val(0){};
    atomic_int(T newval) : m_val(newval) {}
    inline T get() const { return m_val; }
    operator T() const { return m_val; }
    atomic_int& operator=(const T value) {
        setval(value);
        return *this;
    }
};

#ifdef _DEBUG
static const DWORD MY_INFINITY = 30000;
#else
static const DWORD MY_INFINITY = INFINITE;
#endif

struct event {
    NO_COPY_CLASS(event);

    explicit event(LPSECURITY_ATTRIBUTES lpEventAttributes = 0,
        BOOL bManualReset = 0, BOOL bInitialState = 0, TCHAR* lpName = 0)
        : m_h(CreateEvent(
            lpEventAttributes, bManualReset, bInitialState, lpName)) {}

    inline operator HANDLE() { return m_h; }
    ~event() {
        CloseHandle(m_h);
        m_h = 0;
    }

    private:
    HANDLE m_h;
};
template <typename CRTP> class thread {

    static DWORD WINAPI thread_proc(_In_ LPVOID p) {
        thread<CRTP>* t = (thread<CRTP>*)p;
        return t->threadloop();
    }

    DWORD threadloop() {

        HANDLE events[2]{m_event_notify_quit,m_event_wake};

        while (true) {

            bool should_quit = false;
            state_set(states::STATE_ASLEEP);
            SetEvent(m_event);
            DWORD wait = ::WaitForMultipleObjects(2, events, FALSE, INFINITE);

            if (wait != WAIT_OBJECT_0) {
                state_set(states::STATE_AWAKE);
                SetEvent(m_event_woke);

                CRTP& crtp = (CRTP&)*this;
                int ret = 0;
                while (ret == 0) {
                    ret = crtp.on_thread();
                    wait = ::WaitForSingleObject(m_event_notify_quit, 0);
                    if (wait != WAIT_TIMEOUT) {
                        state_set(states::STATE_QUITTING);
                        should_quit = true;
                        break;
                    }
                }
                if (ret == 0 && !should_quit) {
                    continue;
                } else {
                    if (ret < 0) {
                        state_set(states::STATE_ABORTED);
                        SetEvent(m_event_aborted);
                    } else {
                        state_set(states::STATE_QUITTING);
                        break;
                    }
                   
                }
            } else {
                state_set(states::STATE_QUITTING);
                break;
            }
        };

        HANDLE t = m_thread;
        m_thread = 0;
        CloseHandle(t);
        m_thread_id = 0;
        SetEvent(m_event_quit);
        state_set(states(m_state | states::STATE_QUIT));
        return 0;
    }

    void clean_exit() {

        DWORD d1 = timeGetTime();
        SetEvent(m_event_notify_quit);
        int slept = 0;

        while (state() == states::STATE_AWAKE) {

            if (slept >= 2000) {
                assert("thread destructor: Your thread function is busy!" == 0);
                break;
            }
            Sleep(1);
            ++slept;
        }
                
        DWORD dwWait = WaitForSingleObject(m_event_quit, MY_INFINITY);
        assert(dwWait != WAIT_TIMEOUT
            && "~thread: timed out waiting for thread to exit.");
        DWORD d2 = timeGetTime();
        TRACE("thread, with id: %ld took %ld ms to die. (slept=%d)\n",
            this->m_id, (long)d2 - d1, slept);
    }

    int create() {
        if (!have_quit()) {
            assert(
                "thread: call to create() when we have a thread already" == 0);
            return -EEXIST;
        }
        assert(m_thread == 0);

        ResetEvent(m_event);
        ResetEvent(m_event_notify_quit);
        ResetEvent(m_event_quit);
        ResetEvent(m_event_wake);
        ResetEvent(m_event_woke);

        m_thread = CreateThread(
            0, 0, &thread_proc, this, CREATE_SUSPENDED, &m_thread_id);
        DWORD wait = WaitForSingleObject(m_event, MY_INFINITY);
        assert(wait != WAIT_TIMEOUT);
        if (wait == WAIT_TIMEOUT) {
            return -ETIMEDOUT;
        }
        return 0;
    }

    public:
    NO_COPY_CLASS(thread);
    thread(int id = -1)
        : m_thread_id(0)
        , m_thread(CreateThread(
              0, 0, &thread_proc, this, CREATE_SUSPENDED, &m_thread_id))
        , m_state(states::STATE_NONE)
        , m_id(id)

    {
        state_set(states::STATE_NONE);
        assert(m_state == states::STATE_NONE);
    }

    // This is thread safe. You can call it from your own thread loop
    // to see if you should return. Or, you can look at state(), which
    // is also thread safe.
    inline bool has_been_notified_to_quit() const {
        DWORD wait = ::WaitForSingleObject(m_event_notify_quit, 0);
        return (wait != WAIT_TIMEOUT);
    }
    inline int id() const { return m_id; }

    ~thread() { 

        if (m_thread && GetCurrentThread() == m_thread) {
            assert(
                "It's a crass mistake to try to destroy the thread object from "
                "the actual thread. It would be deadlock!"
                == 0);
        }


        clean_exit(); 
    }

    inline bool have_quit() {
        if ((state() & states::STATE_QUIT) == STATE_QUIT) {
            return true;
        }
        return false;
    }

    inline DWORD start(DWORD max_wait = (DWORD)-1) {

        if (max_wait == (DWORD)-1) {
            max_wait = MY_INFINITY;
        }
        if (have_quit()) {
            int c = create();
            if (c) {
                assert("Create() thread failed" == 0);
                return c;
            }
        }
        DWORD ret = ::ResumeThread(m_thread);

        if (ret == 1) {
            ::WaitForSingleObject(m_event, max_wait);
            // TRACE("This is the only place we set event[0] %i\n", m_id);
            SetEvent(m_event_wake);
            ::WaitForSingleObject(m_event_woke, max_wait);
        }
        return ret;
    }
    enum states {
        STATE_NONE,
        STATE_AWAKE = 1,
        STATE_ASLEEP = 2,
        STATE_QUITTING = 4,
        STATE_ABORTED = 8,
        STATE_QUIT = 16
    };

    // Thread-safe -- call it from whereever you want!
    inline states state() const {
        concurrent::safe_read_value(m_state);
        return static_cast<states>(m_state);
    }

    private:
    event m_event;
    event m_event_wake;
    event m_event_woke;
    event m_event_notify_quit;
    event m_event_quit;
    event m_event_aborted;
    DWORD m_thread_id;
    HANDLE m_thread;

    volatile LONG m_state;
    int m_id;

    void state_set(states newstate) {
        concurrent::safe_write_value(m_state, static_cast<LONG>(newstate));
        assert(state() == newstate);
    }
};
} // namespace concurrent

#define TEST_MY_CONCURRENT
#ifdef TEST_MY_CONCURRENT
namespace test {

static inline void test_atomic() {
    concurrent::atomic_int<LONG> myint = 0;
    assert(myint == 0);
    myint = 77;
    assert(myint.get() == 77);
    assert(myint == 77);
    myint = myint + 1;
    assert(myint == 78);
}
class mythread : public concurrent::thread<mythread> {

    typedef concurrent::thread<mythread> base_t;

    friend class base_t;

    public:
    mythread(int id = -1) : base_t(id) {}
    ~mythread() {}
    int start() { return base_t::start(); }
    typedef base_t::states state_t;

    private:
    int on_thread() {
        Sleep(100);
        return 0;
    }
};

class mythreadex : public concurrent::thread<mythreadex> {

    typedef concurrent::thread<mythreadex> base_t;

    friend class base_t;

    public:
    typedef int (*thread_fun)(void*);
    mythreadex(thread_fun tc, int id = -1) : base_t(id), m_callback(tc) {}
    ~mythreadex() {}
    int start() { return base_t::start(); }
    typedef base_t::states state_t;

    private:
    int on_thread() { return m_callback((void*)this); }
    thread_fun m_callback;
};

static inline void test_thread(int i = -1) {

    mythread t(i);
    mythread::state_t state = t.state();
    assert(state == mythread::state_t::STATE_NONE);
    int start_val = t.start();
    assert(start_val == 1);
    state = t.state();
    assert(state >= mythread::state_t::STATE_NONE);
}

int threadfun(void* ptr) {
    mythreadex* pt = (mythreadex*)ptr;
    TRACE("threadfun for threadex, with id %d\n", pt->id());
    return 0;
}
static inline void test_threadex(int i = -1) {

    // using ft = decltype(threadfun);

    mythreadex t(threadfun, i);
    mythreadex::state_t state = t.state();
    assert(state == mythreadex::state_t::STATE_NONE);
    int start_val = t.start();
    assert(start_val == 1);
    state = t.state();
    assert(state >= mythreadex::state_t::STATE_NONE);
}

// would like some static analyzer to spot this obvious race,
// but have not found one yet!
static ULONGLONG racy = 0;
int racy_threadfun(void* ptr) {
    mythreadex* pt = (mythreadex*)ptr;
    ++racy;
    if (racy > 10000) {
        return 1;
    }
    return 0;
}
static inline void make_race(int i = -1) {
    mythreadex t(racy_threadfun, i);
    mythreadex t2(racy_threadfun, i + 1000);
    assert(t.id() == i);
    assert(t2.id() == i + 1000);
    
    t.start();
    t2.start();

    while (!t2.have_quit()) {
        Sleep(1000);
        TRACE("Racy value is %llu\n", racy);
    }
}
} // namespace test
#endif
