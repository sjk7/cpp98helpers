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
#endif
#include <assert.h>

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
    explicit atomic_int(){};
    atomic_int(T newval) { setval(newval); }
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

template <typename CRTP> class thread {

    static DWORD WINAPI thread_proc(_In_ LPVOID p) {
        thread<CRTP>* t = (thread<CRTP>*)p;
        return t->threadloop();
    }

    DWORD threadloop() {

        HANDLE events[2]{m_event_wake, m_event_notify_quit};

        SetEvent(m_event);

        while (true) {
            state_set(states::STATE_ASLEEP);
            DWORD wait = ::WaitForMultipleObjects(2, events, FALSE, INFINITE);

            if (wait == WAIT_OBJECT_0) {
                SetEvent(m_event_woke);
                state_set(states::STATE_AWAKE);
                CRTP* crtp = (CRTP*)this;
                if (crtp->on_thread() == 0) {
                    continue;
                } else {
                    state_set(states::STATE_ABORTED);
                }
            } else {
                state_set(states::STATE_QUITTING);
                break;
            }
        };
        SetEvent(m_event_quit);
        state_set(states(m_state | states::STATE_QUIT));
        return 0;
    }

    int create() {
        if (!have_quit()) {
            assert(
                "thread: call to create() when we have a thread already" == 0);
            return -EEXIST;
        }
    }

    public:
    NO_COPY_CLASS(thread);
    thread(const char* id = 0)
        : m_event(CreateEvent(0, 0, 0, 0))
        , m_event_quit(CreateEvent(0, 0, 0, 0))
        , m_event_wake(CreateEvent(0, 0, 0, 0))
        , m_event_woke(CreateEvent(0, 0, 0, 0))
        , m_event_notify_quit(CreateEvent(0, 0, 0, 0))
        , m_thread(CreateThread(
              0, 0, &thread_proc, this, CREATE_SUSPENDED, &m_thread_id))
        , m_state(states::STATE_NONE)
        , m_id(id != 0 ? id : "")

    {
        state_set(states::STATE_ABORTED);
        assert(m_state == states::STATE_ABORTED);
    }

    inline bool have_quit() {
        if ((state() & states::STATE_QUIT) == STATE_QUIT) {
            return true;
        }
        return false;
    }

    inline DWORD start() {
        assert(
            have_quit() && "thread has been quit, and you want to start it??");
        DWORD ret = ::ResumeThread(m_thread);

        if (ret == 1) {
            ::WaitForSingleObject(m_event, MY_INFINITY);
        }
        return ret;
    }

    protected:
    enum states {
        STATE_NONE,
        STATE_AWAKE = 1,
        STATE_ASLEEP = 2,
        STATE_QUITTING = 4,
        STATE_ABORTED = 8,
        STATE_QUIT = 16
    };

    private:
    HANDLE m_event;
    HANDLE m_event_wake;
    HANDLE m_event_woke;
    HANDLE m_event_notify_quit;
    HANDLE m_event_quit;
    HANDLE m_thread;
    DWORD m_thread_id;
    volatile LONG m_state;
    std::string m_id;

    void state_set(states newstate) {
        concurrent::safe_write_value(m_state, static_cast<LONG>(newstate));
        assert(state() == newstate);
    }

    inline states state() const {
        concurrent::safe_read_value(m_state);
        return static_cast<states>(m_state);
    }
};
} // namespace concurrent
