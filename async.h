#pragma once
#include <functional>
#include <stdexcept>

namespace aaa {

enum class resume_mode
{
    normal,
    cancel,
    timeout,
    close,
};

using continuation =
    std::function<void(resume_mode, std::string)>;

struct _place_holder
{
    template<class... T>
    void operator()(T&&...) const
    {
    }

    void operator()() const
    {
    }
};

// donot throw exception
template<class Ok, class Err=_place_holder, class Stop=_place_holder>
struct handler
{
    static_assert(std::is_invocable_v<Err, std::runtime_error&>, "bad type, Err");
    static_assert(std::is_invocable_v<Stop, aaa::resume_mode, std::string>, "bad type, Stop");

    handler(Ok ok, Err err = {}, Stop stop = {}) :
        _ok(std::move(ok)),
        _err(std::move(err)),
        _stop(std::move(stop))
    {
    }

    template<class T>
    void handle_success(T&& t)
    {
        static_assert(std::is_invocable_v<Ok, T>, "bad type, Ok");
        if constexpr (std::is_reference_v<T>) {
            _ok(t);
        } else {
            _ok(std::move(t));
        }
    }

    void handle_success()
    {
        static_assert(std::is_invocable_v<Ok>, "bad type, Ok");
        _ok();
    }

    void handle_error(std::runtime_error& err)
    {
        _err(err);
    }

    void handle_stop(resume_mode mode, std::string message)
    {
        _stop(mode, std::move(message));
    }

    template<class Handler>
    handler<Ok, Handler, Stop> on_error(Handler h) &&
    {
        return {std::move(_ok), std::move(h), std::move(_stop)};
    }

    template<class Handler>
    handler<Ok, Err, Handler> on_stop(Handler h) &&
    {
        return {std::move(_ok), std::move(_err), std::move(h)};
    }

    Ok   _ok;
    Err  _err;
    Stop _stop;
};

template<class Ok>
handler<Ok> on_success(Ok ok)
{
    return handler<Ok>{std::move(ok)};
}

}
