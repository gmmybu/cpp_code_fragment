#pragma once

typedef void (*stack_trace_visit_callback)(const char* file, const char* func);

extern "C" {

__declspec(dllimport)
void stack_trace_start();

__declspec(dllimport)
void stack_trace_close();


__declspec(dllimport)
void stack_trace_enter(const char* file, const char* func);

__declspec(dllimport)
void stack_track_leave(const char* file, const char* func);

__declspec(dllimport)
void stack_trace_visit(stack_trace_visit_callback callback);

}

class __auto_stack_trace
{
public:
    __auto_stack_trace(const char* file, const char* func) :
        _file(file), _func(func)
    {
        stack_trace_enter(_file, _func);
    }

    ~__auto_stack_trace()
    {
        stack_track_leave(_file, _func);
    }

    const char* _file;
    const char* _func;
};

#define auto_stack_trace() \
    __auto_stack_trace ast{__FILE__, __FUNCTION__}
