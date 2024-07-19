#include <coroutine>
#include <functional>
#include <iostream>
#include <mutex>
#include <future>

namespace co {
namespace internal {

template<class T>
struct _task_awaiter_state_base {
    std::mutex _mutex;

    bool _has_val = false;

    std::coroutine_handle<> _handle;

    std::exception_ptr _exc;

    bool is_ready() {
        std::lock_guard<std::mutex> lock{ _mutex };
        return _has_val || _exc;
    }

    bool set_handle(std::coroutine_handle<> handle) {
        std::lock_guard<std::mutex> lock{ _mutex };
        _handle = handle;
        return !_has_val && !_exc;
    }

    void set_exception(std::exception_ptr exc) {
        std::lock_guard<std::mutex> lock{ _mutex };
        _exc = exc;

        if (_handle) {
            _handle.resume();
        }
    }
};

template<class T>
struct task_awaiter_state : public _task_awaiter_state_base<T> {
    T _val;

    void set_value(T val) {
        std::lock_guard<std::mutex> lock{ this->_mutex };
        this->_val = std::move(val);
        this->_has_val = true;

        if (this->_handle) {
            this->_handle.resume();
        }
    }

    T get_value() {
        if (this->_exc) {
            std::rethrow_exception(this->_exc);
        }
        return std::move(this->_val);
    }
};

template<class T>
struct task_awaiter_state<T&> : public _task_awaiter_state_base<T&> {
    T* _val;

    void set_value(T& val) {
        std::lock_guard<std::mutex> lock{ this->_mutex };
        this->_val = &val;
        this->_has_val = true;

        if (this->_handle) {
            this->_handle.resume();
        }
    }

    T& get_value() {
        if (this->_exc) {
            std::rethrow_exception(this->_exc);
        }
        return *this->_val;
    }
};

template<>
struct task_awaiter_state<void> : public _task_awaiter_state_base<void> {
    void set_value() {
        std::lock_guard<std::mutex> lock{ this->_mutex };
        this->_has_val = true;

        if (this->_handle) {
            this->_handle.resume();
        }
    }

    void get_value() {
        if (this->_exc) {
            std::rethrow_exception(this->_exc);
        }
    }
};

template<class T>
struct _task_awaiter_base {
    _task_awaiter_base() {
        _state = std::make_shared<task_awaiter_state<T>>();
    }

    auto get_state() {
        return _state;
    }

    bool await_ready() {
        return _state->is_ready();
    }

    auto await_resume() {
        return _state->get_value();
    }

    bool await_suspend(std::coroutine_handle<> handle) {
        return _state->set_handle(handle);
    }

    std::shared_ptr<task_awaiter_state<T>> _state;
};

template<class T>
struct task_awaiter : public _task_awaiter_base<T> {
    template<class... Args>
    void operator()(Args&&... args) {
        return this->_state->set_value(std::make_tuple(args...));
    }
};

template<>
struct task_awaiter<void> : public _task_awaiter_base<void> {
    void operator()() {
        return this->_state->set_value();
    }
};

template<class T>
struct _task_promise_base {
    std::suspend_always initial_suspend() {
        return {};
    }

    std::suspend_never final_suspend() noexcept {
        return {};
    }

    void unhandled_exception() {
        if (_awaiter) {
            _awaiter->set_exception(std::current_exception());
        }
        else if (_promise) {
            _promise->set_exception(std::current_exception());
        }
    }

    void use_awaiter(std::shared_ptr<task_awaiter_state<T>> p) {
        _awaiter = p;
    }

    void use_promise() {
        _promise = std::make_shared<std::promise<T>>();
    }

    std::shared_ptr<task_awaiter_state<T>> _awaiter;

    std::shared_ptr<std::promise<T>> _promise;
};

template<class T>
struct _task_promise : public _task_promise_base<T> {
    void return_value(T value) {
        if (this->_awaiter) {
            this->_awaiter->set_value(std::move(value));
        }
        else if (this->_promise) {
            this->_promise->set_value(std::move(value));
        }
    }
};

template<>
struct _task_promise<void> : public _task_promise_base<void> {
    void return_void() {
        if (this->_awaiter) {
            this->_awaiter->set_value();
        }
        else if (this->_promise) {
            this->_promise->set_value();
        }
    }
};

} // namespace internal

template<class T>
struct task {
    struct promise_type;

    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type : public internal::_task_promise<T> {
        promise_type() {
            std::cout << "task promise created ==> " << this << std::endl;
        }

        ~promise_type() {
            std::cout << "task promise destroyed <== " << this << std::endl;
        }

        task get_return_object() {
            return task(handle_type::from_promise(*this));
        }
    };

    task(handle_type h) : _handle(h) {
    }

    ~task() {
    }

private:
    template<class Func, class... Args>
    friend auto call_coro(Func&& func, Args&&... args);

    template<class Func, class... Args>
    friend auto run_coro(Func&& func, Args&&... args);

    auto& promise() {
        return _handle.promise();
    }

    void resume() {
        return _handle.resume();
    }

    auto future() {
        return _handle.promise()._promise->get_future();
    }

    handle_type _handle;
};

template<class Type>
struct _async_task_runner {
    template<class Func, class... Args>
    auto operator()(Func&& func, Args&&... args) const {
        Type waiter{};
        std::bind(std::forward<Func>(func), std::forward<Args>(args)..., waiter)();
        return waiter;
    }
};

template<class... Args>
struct _get_async_awaiter_type {
    using tuple_type = decltype(std::make_tuple(std::declval<Args>()...));
    using type = internal::task_awaiter<tuple_type>;
};

template<class... Args>
constexpr _async_task_runner<typename _get_async_awaiter_type<Args...>::type> run_async;

template<class Func>
struct _get_task_param_type;

template<class Ret>
struct _get_task_param_type<task<Ret>> {
    using type = internal::task_awaiter<Ret>;
};

template<class Func, class... Args>
auto call_coro(Func&& func, Args&&... args) {
    using TaskType = decltype(
        std::bind(std::forward<Func>(func), std::forward<Args>(args)...)()
    );

    using Awaiter = _get_task_param_type<TaskType>::type;

    Awaiter waiter{};
    auto t = std::bind(std::forward<Func>(func), std::forward<Args>(args)...)();
    t.promise().use_awaiter(waiter.get_state());
    t.resume();

    return waiter;
}

template<class Func, class... Args>
auto run_coro(Func&& func, Args&&... args) {
    auto t = std::bind(std::forward<Func>(func), std::forward<Args>(args)...)();
    t.promise().use_promise();

    auto f = t.future();
    t.resume();

    return f;
}

}
