#include "stack_trace.h"
#include <stdio.h>
#include <thread>
#include <windows.h>

void dump_stack(void* ctx, const char* file, const char* func)
{
    printf("    file = %s, func = %s\n", file, func);
}

void func3()
{
    auto_stack_trace();

    auto funcx = [] {
        auto_stack_trace();

        printf("dumping stack\n");
        stack_trace_visit(nullptr, dump_stack);
    };

    funcx();
}

void func2()
{
    auto_stack_trace();
    func3();
}

void func1()
{
    auto_stack_trace();
    func2();
    func3();
}

int main()
{
    stack_trace_start();
    {
        std::thread thd([] {
            func1();
        });
        thd.join();
    }

    ::Sleep(2000);
   

    stack_trace_close();
    getchar();
    return 0;
}
