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

/////////////////////////////////////////////////////////////////////

struct _place_holder
{
    template<class T>
    void operator()(T&&) const
    {
    }

    void operator()() const
    {
    }
};

// do not throw exception
template<class T, class Ok, class Err = _place_holder, class Stop = _place_holder>
class handler
{
public:
    handler(Ok ok, Err err={}, Stop stop={}) :
        _ok(std::move(ok)),
        _err(std::move(err)),
        _stop(std::move(stop))
    {
    }

    void handle_success(T t) const
    {
        if constexpr(std::is_reference_v<T>) {
            _ok(t);
        } else {
            _ok(std::move(t));
        }
    }

    void handle_error(std::runtime_error& err) const
    {
        _err(err);
    }

    void handle_stop(resume_mode mode, std::string message) const
    {
        _stop(mode, std::move(message));
    }

    template<class Handler>
    handler<T, Ok, Handler, Stop> on_error(Handler h) &&
    {
        return {std::move(_ok), std::move(h), std::move(_stop)};
    }

    template<class Handler>
    handler<T, Ok, Err, Handler> on_stop(Handler h) &&
    {
        return {std::move(_ok), std::move(_err), std::move(h)};
    }
private:
    Ok   _ok;
    Err  _err;
    Stop _stop;
};

template<class Ok, class Err, class Stop>
class handler<void, Ok, Err, Stop>
{
public:
    handler(Ok ok, Err err={}, Stop stop={}) :
        _ok(std::move(ok)),
        _err(std::move(err)),
        _stop(std::move(stop))
    {
    }

    void handle_success() const
    {
        _ok();
    }

    void handle_error(std::runtime_error& err) const
    {
        _err(err);
    }

    void handle_stop(resume_mode mode, std::string message) const
    {
        _stop(mode, std::move(message));
    }

    template<class Handler>
    handler<void, Ok, Handler, Stop> on_error(Handler h) &&
    {
        return {std::move(_ok), std::move(h), std::move(_stop)};
    }

    template<class Handler>
    handler<void, Ok, Err, Handler> on_stop(Handler h) &&
    {
        return {std::move(_ok), std::move(_err), std::move(h)};
    }
private:
    Ok   _ok;
    Err  _err;
    Stop _stop;
};

template<class T, class Ok>
handler<T, Ok> on_success(Ok ok)
{
    return handler<T, Ok>{std::move(ok)};
}

}
