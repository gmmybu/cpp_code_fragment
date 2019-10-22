#pragma once

// do not call stack_trace_* in callback
typedef void (*stack_trace_visit_callback)(void* ctx, const char* file, const char* func);

extern "C" {

__declspec(dllimport)
void stack_trace_start();

__declspec(dllimport)
void stack_trace_close();

__declspec(dllimport)
void stack_trace_enter(const char* file, const char* func);

__declspec(dllimport)
void stack_track_leave();

__declspec(dllimport)
void stack_trace_visit(void* ctx, stack_trace_visit_callback callback);

}

class __auto_stack_trace
{
public:
    __auto_stack_trace(const char* file, const char* func)
    {
        stack_trace_enter(file, func);
    }

    ~__auto_stack_trace()
    {
        stack_track_leave();
    }
};

#define auto_stack_trace() \
    __auto_stack_trace ast{__FILE__, __FUNCTION__}
