#pragma once

template<class Func>
class delayed_runner
{
public:
    delayed_runner(const Func& func) : _func(func), _canceled(false) { }

    ~delayed_runner() { if (!_canceled) { _func(); } }

    void cancel() { _canceled = true; }
private:
    const Func& _func;

    bool _canceled;
private:
    delayed_runner(const delayed_runner&);
    delayed_runner& operator=(const delayed_runner&);
};

/// 网上找到的构造带行号的变量

#define dd_link2(x,y) x##y

#define dd_link1(x,y) dd_link2(x,y)

#define dd_make_var(x) dd_link1(x, __LINE__)

#define make_func   dd_make_var(__func)

#define make_runner dd_make_var(__runner)

#define will_delayed_run(var, ...) auto make_func = __VA_ARGS__; delayed_runner<decltype(make_func)> var(make_func)

#define will_delayed_run_for_sure(...) auto make_func = __VA_ARGS__; delayed_runner<decltype(make_func)> make_runner(make_func)
