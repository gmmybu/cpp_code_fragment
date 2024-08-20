#pragma once
#include <mutex>
#include <condition_variable>
#include <vector>
#include <thread>
#include <stdexcept>
#include <atomic>

class lifecycle final {
public:
    lifecycle() {
    }

    ~lifecycle() {
        if (_use_count > 0) {
            // lifecycle `lock` and `unlock` are not paired
            // dtor should not throw exception, so just abort
            std::abort();
        }
    }

    void release() {
        _released = true;

        auto pos = find_thread_state();
        if (pos != thread_states.end()) {
            // remove current thread use count
            if (pos->dec_use_count) {
                pos->dec_use_count = false;
                _use_count--;
            }

            if (_use_count == 0) {
                _cond.notify_all();
            }
        }

        std::unique_lock<std::mutex> lock{ _mutex };
        _cond.wait(lock, [this] {
            return _use_count == 0;
        });
    }

    bool lock(bool& already_locked) {
        if (_released) {
            return false;
        }

        auto pos = find_thread_state();
        if (pos != thread_states.end()) {
            already_locked = true;
            return true;
        }

        thread_state state = {this};
        thread_states.push_back(state);

        _use_count++;

        if (_released) {
            unlock(false);
            return false;
        }

        already_locked = false;
        return true;
    }

    void unlock(bool already_locked) {
        if (already_locked) {
            return;
        }

        auto pos = find_thread_state();
        if (pos == thread_states.end()) {
            throw std::logic_error{"lifecycle `unlock` isn't paired with `lock` in the same thread"};
        }

        if (pos->dec_use_count) {
            _use_count--;
        }

        thread_states.erase(pos);

        if (_released && _use_count == 0) {
            _cond.notify_all();
        }
    }

private:
    std::mutex _mutex;
    std::condition_variable _cond;

    std::atomic<int32_t> _use_count = 0;
    std::atomic_bool _released = false;

    struct thread_state {
        lifecycle* lc;
        bool dec_use_count = true;
    };

    static thread_local std::vector<thread_state> thread_states;

private:
    std::vector<thread_state>::iterator find_thread_state() {
        for (auto iter = thread_states.begin(); iter != thread_states.end(); iter++) {
            if (iter->lc == this) {
                return iter;
            }
        }
        return thread_states.end();
    }
};

template<class T>
class object_lifecycle {
public:
    object_lifecycle(T* obj) : _obj(obj) {
        if (_obj == nullptr) {
            throw std::logic_error{ "object_lifecycle obj is nullptr" };
        }
    }

    void release() {
        _lc.release();
    }

    T* lock(bool& already_locked) {
        return _lc.lock(already_locked) ? _obj : nullptr;
    }

    void unlock(bool already_locked) {
        return _lc.unlock(already_locked);
    }

private:
    lifecycle _lc;
    T* _obj;
};

template<class T>
auto make_lifecycle(T* t) {
    return std::make_shared<object_lifecycle<T>>(t);
}

template<class T>
using object_lifecycle_ptr = std::shared_ptr<object_lifecycle<T>>;

// usage:
//     if (auto obj = use_object(olc); obj) {
//         // use obj
//     }
template<class T>
class object_wrapper {
public:
    explicit object_wrapper(object_lifecycle_ptr<T>& lc) : _lc(lc) {
        _obj = _lc->lock(_already_locked);
    }

    ~object_wrapper() {
        if (_obj) {
            _lc->unlock(_already_locked);
        }
    }

    operator bool() const {
        return _obj != nullptr;
    }

    T* operator->() const {
        return _obj;
    }

    object_wrapper(const object_wrapper&) = delete;
    object_wrapper& operator=(const object_wrapper&) = delete;

private:
    object_lifecycle_ptr<T>& _lc;
    bool _already_locked;

    T* _obj;
};

template<class T>
auto use_object(object_lifecycle_ptr<T>& lc) {
    return object_wrapper<T>{lc};
}
