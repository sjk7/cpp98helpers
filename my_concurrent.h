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

#include <cassert>
#include <cerrno>
#include <cstdio>

#ifndef TRACE
#define TRACE printf
#endif

#include "./my/my.h" 
#if ((_MSC_VER > 1899) && (WINVER > 0x0601))
#define MY_NO_EXCEPT noexcept
#else
#define MY_NO_EXCEPT

#ifdef _MSC_VER
#pragma warning(disable : 4482) // non-standard enum use (qualified)
#pragma warning(disable : 4355) // 'this' used in base member initializer list
#endif
#endif

#ifdef _MSC_VER
#if (_MSC_VER <= 1200)
#define MSVC6
#define constexpr

#ifndef LONG
#define LONG long;
#endif
#endif
#endif

#ifdef __MINGW32_MAJOR_VERSION
#if (__cplusplus <= 199711L)
#define constexpr
#endif
#endif

namespace concurrent {

static inline LONG safe_read_value(volatile LONG& value) MY_NO_EXCEPT {
    return InterlockedExchangeAdd((LONG*)&value, 0);
}
static inline ULONGLONG safe_read_value(
    volatile ULONGLONG& value) MY_NO_EXCEPT {
    return InterlockedExchangeAdd64(
        reinterpret_cast<LONGLONG volatile*>(&value), 0);
}

static inline LONG safe_write_value(
    volatile LONG& target, LONG new_value) MY_NO_EXCEPT {
    return InterlockedExchange((LONG*)&target, new_value);
}

static inline ULONGLONG safe_write_value(
    volatile ULONGLONG& target, ULONGLONG new_value) MY_NO_EXCEPT {
    return InterlockedExchange64(
        reinterpret_cast<LONGLONG volatile*>(&target), new_value);
}

#pragma warning(disable : 4197)
template <typename T> class atomic_int {
    private:
    volatile T m_val;

    public:
    NO_COPY_CLASS(atomic_int);

    explicit atomic_int() : m_val(0){};
    atomic_int(T newval) : m_val(newval) {}
    inline T get() const { return m_val; }
    explicit operator T() const { return m_val; }
    T operator==(const T value) const { return get() == value; }
	    inline void setval(const T newval) {
        safe_write_value((volatile T&)m_val, newval);
        assert(newval == m_val);
    }
    atomic_int& operator=(const T value) {
        setval(value);
        assert(m_val == value);
        return *this;
    }
	/*/
    atomic_int& operator+(const T value) {
        ::InterlockedAdd64((LONGLONG*)m_val, value);
        return *this;
    }
	/*/
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

#ifdef _MSC_VER
#if (_MSC_VER <= 1200)
#define DAMN_VC6
#endif
#endif

struct STATES {

#ifndef DAMN_VC6
    static const LONG STATE_NONE = 0;
    static const LONG STATE_AWAKE = 1;
    static const LONG STATE_ASLEEP = 2;
    static const LONG STATE_QUITTING = 4;
    static const LONG STATE_ABORTED = 8;
    static const LONG STATE_QUIT = 16;
#else
    // errors? In VC6 you need to add m_thread_state_defs.cpp
    static const LONG STATE_NONE;
    static const LONG STATE_AWAKE;
    static const LONG STATE_ASLEEP;
    static const LONG STATE_QUITTING;
    static const LONG STATE_ABORTED;
    static const LONG STATE_QUIT;
#endif

    inline STATES(const LONG v) : m_value(v) {}
    inline STATES(const STATES& rhs) : m_value(rhs) {}
    inline STATES& operator=(const STATES& rhs) {
        m_value = rhs.m_value;
        return *this;
    }

    inline operator LONG() const { return m_value; }

    private:
    LONG m_value;
};

// NOTE: A thread class that does not use virtual dispatch.
// Your onthread() could return the following values:
// 0    :   Thread will continue immediately after checking if the thread has an
//          exit request. If there is one, the thread exits and does not call
//          you again.
//
// >= 1 :   Thread will go back into STATE_ASLEEP mode, but will continue to
// check
//          if there are any exit requests. If there is one, the thread exits
//          and does not call you again.
//
//  < 0 :   Thread will exit as fast as possible,
//          and of course will not call you again until (re)start()ed.
template <typename CRTP> class thread {

    friend CRTP; /// hmmm: how to 98'ify this??
    // ^^ can't find a way -- so we have to make
    // things unnec' public!

    // private constructor guards against someone using the wrong class!
    thread(int id = -1)
        : m_event_notify_quit(0, 1, 0, 0)
        , m_thread_id(0)
        , m_thread(0)
        , m_state(states::STATE_NONE)
        , m_id(id)

    {
        state_set(states::STATE_NONE);
        assert(m_state == states::STATE_NONE);
    }

    public:
    typedef STATES states;

    private:
    static DWORD WINAPI thread_proc(LPVOID p) {
        thread<CRTP>* t = (thread<CRTP>*)p;
        return t->threadloop();
    }

    DWORD threadloop() {

        HANDLE events[2] = {m_event_notify_quit, m_event_wake};

        while (true) {

            bool should_quit = false;
        asleep:
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
                    if (ret == 0) {
                        wait = ::WaitForSingleObject(m_event_notify_quit, 0);
                    } else {
                        if (ret < 0) {
                            state_set(states::STATE_QUITTING);
                            should_quit = true;
                            break;
                        } else {
                            // > 1 returned from thread func: go back to sleep.
                            goto asleep;
                        }
                    }
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
                        state_set(states::STATE_QUITTING);
                        state_set(states::STATE_ABORTED);
                        SetEvent(m_event_aborted);
                    } else {
                        state_set(states::STATE_QUITTING);
                        break;
                    }
                }
            } else {
                SetEvent(
                    m_event_woke); // set this so anyone waiting does not block
                state_set(states::STATE_QUITTING);
                break;
            }
        };

        HANDLE t = m_thread;
        m_thread = 0;
        CloseHandle(t);
        m_thread_id = 0;
        state_set(states(m_state | states::STATE_QUIT));
        SetEvent(m_event_quit);
        return 0;
    }

    void clean_exit() {

        DWORD d1 = timeGetTime();
        if (!m_thread) {
            return;
        }

        SetEvent(m_event_notify_quit);
        if (state() == states::STATE_NONE) {
            DWORD dwstart = start();
            assert(dwstart == 1);
        }

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
        state_set(states::STATE_NONE);
        TRACE("thread, with id: %d took %ld ms to die. (slept=%d)\n",
            this->m_id, (long)d2 - d1, slept);

        ResetEvent(m_event);
        ResetEvent(m_event_notify_quit);
        ResetEvent(m_event_quit);
        ResetEvent(m_event_wake);
        ResetEvent(m_event_woke);
    }

    int create() {

        if (state() != states::STATE_NONE) {

            if (!have_quit()) {
                assert("thread: call to create() when we have a thread already"
                    == 0);
                return -EEXIST;
            }
        }
        assert(m_thread == 0);

        ResetEvent(m_event);
        ResetEvent(m_event_notify_quit);
        ResetEvent(m_event_quit);
        ResetEvent(m_event_wake);
        ResetEvent(m_event_woke);

        m_thread = CreateThread(
            0, 0, &thread_proc, this, CREATE_SUSPENDED, &m_thread_id);

        state_set(states::STATE_NONE);

        return 0;
    }

    public:
    NO_COPY_CLASS(thread);

    // This is thread safe. You can call it from your own thread loop
    // to see if you should return. If you don't want to block, wait_timeout
    // is OK to be set to zero.
    inline bool has_been_notified_to_quit(DWORD wait_timeout = 0) const {

        DWORD wait = ::WaitForSingleObject(m_event_notify_quit, wait_timeout);
        if (wait != WAIT_TIMEOUT) {
            return true;
        }
        if (state() <= states::STATE_NONE || state() > states::STATE_ASLEEP) {
            return true;
        }
        return false;
    }
    inline int id() const { return m_id; }

    inline DWORD thread_id() const { return m_thread_id; }
    inline DWORD set_thread_priority(int new_priority) {
        DWORD ret = 0;
        if (m_thread) {
            ret = ::SetThreadPriority(m_thread, new_priority);
        }
        assert(m_thread
            && "setting thread priority when there is no thread has no "
               "effect!");
        return ret;
    }

    inline int thread_priority() const {
        if (m_thread) {
            return ::GetThreadPriority(m_thread);
        } else {
            return 0;
        }
    }

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
        if ((state() & states::STATE_QUIT) == states::STATE_QUIT) {
            return true;
        }
        return false;
    }

    void stop() { clean_exit(); }
    void quit() { clean_exit(); }

    // returns the result of waiting for the thread to start.
    // Same return value semantics as WaitForSingleObject
    inline DWORD start(DWORD max_wait = (DWORD)-1) {

        if (max_wait == (DWORD)-1) {
            max_wait = MY_INFINITY;
        }
        if (have_quit() || state() == states::STATE_NONE) {
            int c = create();
            if (c) {
                assert("Create() thread failed" == 0);
                return c;
            }
        }
        ResetEvent(m_event_notify_quit);
        DWORD ret = ::ResumeThread(m_thread);

        if (ret == 1) {
            ret = ::WaitForSingleObject(m_event, max_wait);
            // TRACE("This is the only place we set event[0] %i\n", m_id);
            SetEvent(m_event_wake);
            ret = ::WaitForSingleObject(m_event_woke, max_wait);

        } else {
            if (state() == states::STATE_ASLEEP) {
                SetEvent(m_event_wake);
                ret = ::WaitForSingleObject(m_event_woke, max_wait);
            }
        }

        if (max_wait) {
            assert(ret != WAIT_TIMEOUT);
        }

        return ret;
    }

    // Thread-safe -- call it from whereever you want!
    inline states state() const {

        concurrent::safe_read_value(m_state);
        return static_cast<states>(m_state);
    }

    private:
    mutable event m_event;
    mutable event m_event_wake;
    mutable event m_event_woke;
    mutable event m_event_notify_quit;
    mutable event m_event_quit;
    mutable event m_event_aborted;
    DWORD m_thread_id;
    volatile HANDLE m_thread;

    mutable volatile LONG m_state;
    int m_id;

    void state_set(const states newstate) {
        if (newstate == 20) {
            TRACE("ffs");
        }
        concurrent::safe_write_value(m_state, (LONG)newstate);
        if ((newstate & states::STATE_QUITTING) == states::STATE_QUITTING) {
            SetEvent(m_event_notify_quit);
        }
        assert(state() == newstate);
    }
}; // class thread

} // namespace concurrent

#define TEST_MY_CONCURRENT
#ifdef TEST_MY_CONCURRENT
#include <atomic>
namespace test {

static inline void test_atomic() {
    concurrent::atomic_int<LONG> myint = (LONG)0;
	
    assert(myint == 0);
    myint = 77;
	LONG i = myint.get();
    assert(i == 77);
    assert(myint == 77);
	 myint.setval(78);
	//myint = myint + 1; <-- NOT atomic, even on std::atomic, so not included.
	 assert(myint == 78);
	
}

class mythread : public concurrent::thread<mythread> {

    public:
    typedef concurrent::thread<mythread> base_t;
    typedef mythread friend_type;

    mythread(int id = -1) : base_t(id) {}
    ~mythread() {}
    int start() { return base_t::start(); }
    typedef base_t::states state_t;
    int on_thread() {
        Sleep(100);
        return 0;
    }

    private:
};

class mythreadex : public concurrent::thread<mythreadex> {

    typedef concurrent::thread<mythreadex> base_t;

    public:
    typedef mythreadex friend_type;
    typedef int (*thread_fun)(void*);
    mythreadex(thread_fun tc, int id = -1) : base_t(id), m_callback(tc) {}
    ~mythreadex() {}
    int start() { return base_t::start(); }
    typedef base_t::states state_t;

    int on_thread() { return m_callback((void*)this); }

    private:
    thread_fun m_callback;
};

static inline void test_thread(int i = -1) {

    mythread t(i);
    mythread::state_t state = t.state();
    assert(state == mythread::states::STATE_NONE);
    int start_val = t.start();
    assert(start_val == 0);
    state = t.state();
    assert(state >= mythread::state_t::STATE_NONE);
}

static int threadfun(void* ptr) {
    mythreadex* pt = (mythreadex*)ptr;
    TRACE("threadfun for threadex, with id %d\n", pt->id());
    return 0;
}

static inline void test_threadex(int i = -1) {
    mythreadex t(threadfun, i);
    mythreadex::state_t state = t.state();
    assert(state == mythreadex::state_t::STATE_NONE);
    int start_val = t.start();
    assert(start_val == 0);
    state = t.state();
    assert(state >= mythreadex::state_t::STATE_NONE);
}

// would like some static analyzer to spot this obvious race,
// but have not found one yet!
static ULONGLONG racy = 0;
static int racy_threadfun(void* ptr) {
    mythreadex* pt = (mythreadex*)ptr;
    (void)pt;
    ++racy;
    if (racy > 10000) {
        return -1;
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
        Sleep(100);
        TRACE("Racy value is %llu\n", racy);
    }
}

static int stop_start_thread_fun(void* ptr) {
    TRACE("stop_start_thread_fun_sleeps a bit ...\n");
    mythreadex* pt = (mythreadex*)ptr;
    int i = 0;
    while (i++ < 1000) {
        Sleep(1);
        if (pt->has_been_notified_to_quit()) {
            TRACE("Stop_start_thread_fun: time to quit, better return ...\n");
            return 0;
        }
    }
    return 0;
}
static inline void test_thread_stop_start() {

    mythreadex t(stop_start_thread_fun, 0);
    int i = 0;

    while (i++ < 100) {
        DWORD dw1 = timeGetTime();
        t.stop();
        DWORD dw2 = timeGetTime();
        TRACE("thread took %ld ms to stop()\n", (long)(dw2 - dw1));
        dw1 = timeGetTime();
        DWORD dw = t.start();
        dw2 = timeGetTime();
        assert(dw == 0);
        TRACE("Thread took %ld ms to start again\n", (long)(dw2 - dw1));
    }
    Sleep(2000);
}
} // namespace test
#endif
