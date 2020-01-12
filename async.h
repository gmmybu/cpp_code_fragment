#pragma once
#include <functional>
#include <stdexcept>
#include <memory>

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

struct _Place_Holder
{
    template<class... T>
    void operator()(T&&...) const
    {
    }
};

// donot throw exception
template<class Ok, class Err = _Place_Holder, class Stop = _Place_Holder>
struct _Action
{
    _Action(Ok ok, Err err={}, Stop stop={}) :
        _ok(std::move(ok)),
        _err(std::move(err)),
        _stop(std::move(stop))
    {
    }

    template<class Func>
    _Action<Ok, Func, Stop> on_error(Func h) &&
    {
        return { std::move(_ok), std::move(h), std::move(_stop) };
    }

    template<class Func>
    _Action<Ok, Err, Func> on_stop(Func h) &&
    {
        return { std::move(_ok), std::move(_err), std::move(h) };
    }

    Ok   _ok;
    Err  _err;
    Stop _stop;
};

template<class Ok>
_Action<Ok> on_success(Ok ok)
{
    return _Action<Ok>{std::move(ok)};
}

/////////////////////////////////////////////////////////////////////

template<class T>
struct _Handler_Base
{
    virtual void handle_success(T t) const = 0;
    virtual void handle_error(std::runtime_error&) const = 0;
    virtual void handle_stop(aaa::resume_mode, std::string) const = 0;
};

template<>
struct _Handler_Base<void>
{
    virtual void handle_success() const = 0;
    virtual void handle_error(std::runtime_error&) const = 0;
    virtual void handle_stop(aaa::resume_mode, std::string) const = 0;
};

template<class T, class Ok, class Err, class Stop>
class _Handler : public _Handler_Base<T>
{
    static_assert(std::is_invocable_v<Ok, T>, "bad type, Ok");
    static_assert(std::is_invocable_v<Err, std::runtime_error&>, "bad type, Err");
    static_assert(std::is_invocable_v<Stop, aaa::resume_mode, std::string>, "bad type, Stop");
public:
    _Handler(_Action<Ok, Err, Stop> action) :
        _action(std::move(action))
    {
    }

    void handle_success(T t) const
    {
        if constexpr(std::is_reference_v<T>) {
            _action._ok(t);
        } else {
            _action._ok(std::move(t));
        }
    }

    void handle_error(std::runtime_error& err) const
    {
        _action._err(err);
    }

    void handle_stop(resume_mode mode, std::string message) const
    {
        _action._stop(mode, std::move(message));
    }
private:
    _Action<Ok, Err, Stop> _action;
};

template<class Ok, class Err, class Stop>
class _Handler<void, Ok, Err, Stop> : public _Handler_Base<void>
{
    static_assert(std::is_invocable_v<Ok>, "bad type, Ok");
    static_assert(std::is_invocable_v<Err, std::runtime_error&>, "bad type, Err");
    static_assert(std::is_invocable_v<Stop, aaa::resume_mode, std::string>, "bad type, Stop");
public:
    _Handler(_Action<Ok, Err, Stop> h) :
        _action(std::move(h))
    {
    }

    void handle_success() const
    {
        _action._ok();
    }

    void handle_error(std::runtime_error& err) const
    {
        _action._err(err);
    }

    void handle_stop(resume_mode mode, std::string message) const
    {
        _action._stop(mode, std::move(message));
    }
private:
    _Action<Ok, Err, Stop> _action;
};

template<class T>
class handler
{
public:
    template<class Ok, class Err, class Stop>
    handler(_Action<Ok, Err, Stop> action)
    {
        _ptr = std::make_shared<_Handler<T, Ok, Err, Stop>>(std::move(action));
    }

    void handle_success(T t) const
    {
        if constexpr(std::is_reference_v<T>) {
            _ptr->handle_success(t);
        } else {
            _ptr->handle_success(std::move(t));
        }
    }

    void handle_error(std::runtime_error& err) const
    {
        _ptr->handle_error(err);
    }
        
    void handle_stop(aaa::resume_mode mode, std::string message) const
    {
        _ptr->handle_stop(mode, std::move(message));
    }
private:
    std::shared_ptr<_Handler_Base<T>> _ptr;
};

template<>
class handler<void>
{
public:
    template<class Ok, class Err, class Stop>
    handler(_Action<Ok, Err, Stop> h)
    {
        _ptr = std::make_shared<_Handler<void, Ok, Err, Stop>>(std::move(h));
    }

    void handle_success() const
    {
        _ptr->handle_success();
    }

    void handle_error(std::runtime_error& err)
    {
        _ptr->handle_error(err);
    }

    void handle_stop(aaa::resume_mode mode, std::string message)
    {
        _ptr->handle_stop(mode, std::move(message));
    }
private:
    std::shared_ptr<_Handler_Base<void>> _ptr;
};

}
