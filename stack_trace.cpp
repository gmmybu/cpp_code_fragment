#include <windows.h>
#include <algorithm>
#include <type_traits>
#include <stdexcept>
#include <stdio.h>

typedef void (*stack_trace_visit_callback)(const char* file, const char* func);

namespace {

template<class T, int MAX_SIZE>
class object_pool
{
    struct node
    {
        char  data[sizeof(T) - 4]; // ignore small object
        node* next;
    };
public:
    object_pool()
    {
        static_assert(std::is_pod_v<T>, "std::is_pod_v<T>");

        _pool[MAX_SIZE - 1].next = nullptr;
        for (int i = 0; i < MAX_SIZE - 1; i++) {
            _pool[i].next = &_pool[i + 1];
        }
        _head = &_pool[0];
    }

    T* alloc()
    {
        if (_head == nullptr)
            throw std::bad_alloc();

        node* q = _head;
        _head = _head->next;
        return (T*)q;
    }

    void free(T* p)
    {
        node* n = (node*)p;
        n->next = _head;
        _head = n;
    }
private:
    node  _pool[MAX_SIZE];
    node* _head = nullptr;
private:
    object_pool(const object_pool&) = delete;
    object_pool& operator=(const object_pool&) = delete;
};

class mutex
{
public:
    mutex()
    {
        InitializeCriticalSectionAndSpinCount(&_mutex, 100);
    }

    ~mutex()
    {
        DeleteCriticalSection(&_mutex);
    }

    void lock()
    {
        EnterCriticalSection(&_mutex);
    }

    void unlock()
    {
        LeaveCriticalSection(&_mutex);
    }
private:
    CRITICAL_SECTION _mutex;
private:
    mutex(const mutex&) = delete;
    mutex& operator=(const mutex&) = delete;
};

class mutex_guard
{
public:
    mutex_guard(mutex& mutex) : _mutex(mutex)
    {
        mutex.lock();
    }

    ~mutex_guard()
    {
        _mutex.unlock();
    }
private:
    mutex& _mutex;
private:
    mutex_guard(const mutex_guard&) = delete;
    mutex_guard& operator=(const mutex_guard&) = delete;
};

template<class T>
void linked_list_insert(T*& head, T* what)
{
    what->next = head;
    head = what;
}

template<class T, class Pred>
T* linked_list_query(T* header, Pred pred)
{
    for (T* curr = header; curr != nullptr; curr = curr->next) {
        if (pred(curr)) return curr;
    }

    return nullptr;
}

template<class T, class Pred>
T* linked_list_remove(T*& head, Pred pred)
{
    T** pp = &head;
    while (*pp != nullptr && !pred(*pp)) { pp = &(*pp)->next; }

    T* rr = *pp;
    if (rr != nullptr) {
        *pp = rr->next;
        rr->next = nullptr;
    }

    return rr;
}

template<class T, class Func>
void linked_list_foreach(T* head, Func func)
{
    for (T* curr = head; curr != nullptr;) {
        auto temp = curr;
        curr = curr->next;
        func(temp);
    }
}

class stack_trace_manager
{
    struct stack_frame
    {
        const char* file;
        const char* func;
        stack_frame* next;
    };

    struct stack_trace
    {
        DWORD thread_id;
        stack_frame* stack_frames;
        stack_trace* next;
    };
public:
    stack_trace_manager()
    {
        _worker_thread = CreateThread(NULL, 0, thread_proc, this, 0, NULL);
        if (_worker_thread == NULL)
            throw std::runtime_error("stack_trace, create thread");
    }

    ~stack_trace_manager()
    {
        _running = false;
        if (_worker_thread != NULL) {
            WaitForSingleObject(_worker_thread, INFINITE);
            CloseHandle(_worker_thread);
        }

        for (int i = 0; i < _thread_count; i++) {
            CloseHandle(_threads[i]);
        }
    }

    void enter(const char* file, const char* func)
    {
        mutex_guard guard{_mutex};
        auto trace = ensure_stack_trace();
        if (trace != nullptr) {
            auto sf = _stack_frame_pool.alloc();
            sf->file = file;
            sf->func = func;
            linked_list_insert(trace->stack_frames, sf);
        }
    }
    
    void leave(const char* file, const char* func)
    {
        mutex_guard guard{_mutex};
        auto trace = get_stack_trace(GetCurrentThreadId());
        if (trace != nullptr) {
            auto sf = trace->stack_frames;
            if (sf != nullptr &&
                strcmp(file, sf->file) == 0 &&
                strcmp(func, sf->func) == 0)
            {
                trace->stack_frames = sf->next;
                _stack_frame_pool.free(sf);
            }
        }
    }

    void visit(stack_trace_visit_callback callback)
    {
        mutex_guard guard{_mutex};
        auto trace = get_stack_trace(GetCurrentThreadId());
        if (trace != nullptr) {
            linked_list_foreach(trace->stack_frames,
                [=](stack_frame* sf) { callback(sf->file, sf->func); }
            );
        }
    }
private:
    stack_trace* ensure_stack_trace()
    {
        auto thread_id = GetCurrentThreadId();
        auto trace = get_stack_trace(thread_id);
        if (trace == nullptr) {
            HANDLE process = GetCurrentProcess();
            HANDLE thread = NULL;
            BOOL success = DuplicateHandle(process, GetCurrentThread(),
                process, &thread, 0, FALSE, DUPLICATE_SAME_ACCESS);
            if (success) {
                start_monitor(thread);

                trace = _stack_trace_pool.alloc();
                trace->thread_id = thread_id;
                trace->stack_frames = nullptr;
                linked_list_insert(_head, trace);
            }
        }

        return trace;
    }

    stack_trace* get_stack_trace(DWORD thread_id)
    {
        return linked_list_query(_head,
            [=](stack_trace* st) { return st->thread_id == thread_id; }
        );
    }

    void del_stack_trace(DWORD thread_id)
    {
        mutex_guard guard{_mutex};
        auto trace = linked_list_remove(_head,
            [=](stack_trace* st) { return st->thread_id == thread_id; }
        );

        if (trace != nullptr) {
            printf("del thread %d\n", thread_id);

            linked_list_foreach(trace->stack_frames,
                [=](stack_frame* frame) { _stack_frame_pool.free(frame); }
            );

            _stack_trace_pool.free(trace);
        }
    }
private:
    mutex _mutex;
    object_pool<stack_trace, 1024> _stack_trace_pool;
    object_pool<stack_frame, 64*1024> _stack_frame_pool;

    stack_trace* _head = nullptr;
private:
    void start_monitor(HANDLE thread)
    {
        mutex_guard guard{_worker_mutex};
        if (_thread_count < 1024) {
            _threads[_thread_count++] = thread;
        } else {
            throw std::bad_alloc();
        }
    }

    void worker_loop()
    {
        int offset = 0;
        while (_running) {
            int count = 0;
            _worker_mutex.lock();
            if (_thread_count > 0) {
                count = std::min<>(8, _thread_count - offset);
            }
            _worker_mutex.unlock();

            int pos = -1;
            if (count > 0) {
                DWORD result = WaitForMultipleObjects(count,
                    _threads + offset, FALSE, 20);
                if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + count) {
                    pos = offset + result - WAIT_OBJECT_0;
                }
            } else {
                Sleep(20);
            }

            HANDLE thread = NULL;
            if (pos != -1) {
                thread = _threads[pos];
            }

            _worker_mutex.lock();
            if (pos != -1) {
                _threads[pos] = _threads[--_thread_count];
            }

            if (count < 8) {
                offset = 0;
            } else {
                offset += 8;
            }
            _worker_mutex.unlock();

            if (thread != NULL) {
                del_stack_trace(GetThreadId(thread));
                CloseHandle(thread);
            }
        }
    }

    mutex _worker_mutex;
    bool _running = true;
    HANDLE _worker_thread = 0;

    int _thread_count = 0;
    HANDLE _threads[1024];

    static DWORD WINAPI thread_proc(LPVOID param)
    {
        auto thiz = (stack_trace_manager*)param;
        thiz->worker_loop();
        return 0;
    }
public:
    static void* operator new(size_t) noexcept
    {
        static char buffer[sizeof(stack_trace_manager)];
        return buffer;
    }

    static void operator delete(void*) noexcept
    {
    }
};

stack_trace_manager* _manager = nullptr;

};

extern "C" {

__declspec(dllexport)
void stack_trace_start()
{
    try
    {
        if (_manager == nullptr) {
            _manager = new stack_trace_manager;
        }
    }
    catch (...)
    {
    }
}

__declspec(dllexport)
void stack_trace_close()
{
    delete _manager;
    _manager = nullptr;
}

__declspec(dllexport)
void stack_trace_enter(const char* file, const char* func)
{
    try
    {
        if (_manager != nullptr) {
            _manager->enter(file, func);
        }
    }
    catch (...)
    {
    }
}

__declspec(dllexport)
void stack_track_leave(const char* file, const char* func)
{
    try
    {
        if (_manager != nullptr) {
            _manager->leave(file, func);
        }
    }
    catch(...)
    {
    }
}

__declspec(dllexport)
void stack_trace_visit(stack_trace_visit_callback callback)
{
    try
    {
        if (_manager != nullptr) {
            _manager->visit(callback);
        }
    }
    catch (...)
    {
    }
}

}
