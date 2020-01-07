#pragma once
#include <functional>
#include <stdexcept>

namespace aaa {

enum class resume_mode
{
    normal,
    cancel,
    timeout,
    stop,
};

inline std::string _to_string(resume_mode mode, const std::string& detail)
{
    if (mode == resume_mode::cancel)
        return u8"请求取消，" + detail;
    if (mode == resume_mode::timeout)
        return u8"请求超时，" + detail;
    if (mode == resume_mode::stop)
        return u8"请求终止，" + detail;

    throw std::logic_error{u8"参数错误mode"};
}

class resume_error : public std::runtime_error
{
public:
    resume_error(resume_mode mode, const std::string& detail = "") :
        std::runtime_error{_to_string(mode, detail)}, _mode{mode}
    {
    }

    resume_mode get_mode() const
    {
        return _mode;
    }
private:
    resume_mode _mode;
};

using continuation =
    std::function<void(resume_mode, const char*)>;

/////////////////////////////////////////////////////////////////////

template<class Handler>
void _on_error(std::exception_ptr ex, const Handler& h)
{
    try
    {
        if (ex != nullptr) {
            std::rethrow_exception(ex);
        }
    }
    catch(resume_error& e1)
    {
        if (e1.get_mode() != resume_mode::stop) {
            h(e1);
        }
    }
    catch(std::runtime_error& e2)
    {
        h(e2);
    }
}

template<class Handler>
void _on_stop(std::exception_ptr ex, const Handler& h)
{
    try
    {
        if (ex != nullptr) {
            std::rethrow_exception(ex);
        }
    }
    catch(resume_error& e1)
    {
        if (e1.get_mode() == resume_mode::stop) {
            h();
        }
    }
    catch(std::runtime_error&)
    {
    }
}

template<class T>
class result
{
public:
    static result with_value(T t)
    {
        return result{std::move(t), nullptr};
    }

    static result with_exception(std::exception_ptr err)
    {
        if (err == nullptr)
            throw std::logic_error{u8"异常不能为空"};

        return result{T{}, err};
    }

    template<class Ok>
    result& on_success(const Ok& ok)
    {
        if (_err == nullptr) {
            ok(_data);
        }

        return *this;
    }

    template<class Handler>
    result& on_error(const Handler& h)
    {
        _on_error(_err, h);
        return *this;
    }

    template<class Handler>
    result& on_stop(const Handler& h)
    {
        _on_stop(_err, h);
        return *this;
    }
private:
    result(T t, std::exception_ptr err) :
        _data(std::move(t)), _err(err) {}

    T _data;
    std::exception_ptr _err;
};

template<class T>
class result<T&>
{
public:
    static result with_value(T& t)
    {
        return result{&t, nullptr};
    }

    static result with_exception(std::exception_ptr err)
    {
        if (err == nullptr)
            throw std::logic_error{u8"异常不能为空"};

        return result{nullptr, err};
    }

    template<class Ok>
    result& on_success(const Ok& ok)
    {
        if (_err == nullptr) {
            ok(*_data);
        }

        return *this;
    }

    template<class Handler>
    result& on_error(const Handler& h)
    {
        _on_error(_err, h);
        return *this;
    }

    template<class Handler>
    result& on_stop(const Handler& h)
    {
        _on_stop(_err, h);
        return *this;
    }
private:
    result(T* t, std::exception_ptr err) :
        _data(t), _err(err) {}

    T* _data;
    std::exception_ptr _err;
};

template<>
class result<void>
{
public:
    static result with_value()
    {
        return result{nullptr};
    }

    static result with_exception(std::exception_ptr err)
    {
        if (err == nullptr)
            throw std::logic_error{u8"异常不能为空"};

        return result{err};
    }

    template<class Ok>
    result& on_success(const Ok& ok)
    {
        if (_err == nullptr) {
            ok();
        }

        return *this;
    }

    template<class Handler>
    result& on_error(const Handler& h)
    {
        _on_error(_err, h);
        return *this;
    }

    template<class Handler>
    result& on_stop(const Handler& h)
    {
        _on_stop(_err, h);
        return *this;
    }
private:
    result(std::exception_ptr err) :
        _err(err) {}

    std::exception_ptr _err;
};

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

template<class T, class Ok, class Err = _place_holder, class Stop = _place_holder>
class handler
{
public:
    handler(Ok ok, Err err = {}, Stop stop = {}) :
        _ok(std::move(ok)), _err(std::move(err)), _stop(std::move(stop))
    {
    }

    void operator()(result<T> result)
    {
        result.on_success(_ok)
              .on_error(_err)
              .on_stop(_stop);
    }

    template<class Handler>
    handler<T, Ok, Handler, Stop> on_error(Handler h) &&
    {
        return handler<T, Ok, Handler, Stop>{
            std::move(_ok), std::move(h), std::move(_stop)
        };
    }

    template<class Handler>
    handler<T, Ok, Err, Handler> on_stop(Handler h) &&
    {
        return handler<T, Ok, Err, Handler>{
            std::move(_ok), std::move(_err), std::move(h)
        };
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
