#pragma once
#include <mutex>
#include <condition_variable>
#include <vector>
#include <thread>
#include <stdexcept>

class lifecycle final {
public:
    lifecycle() : _owner_threads() {
        _owner_threads.reserve(32);
    }

    ~lifecycle() {
        std::unique_lock<std::mutex> lock{ _mutex };
        if (_use_count > 0) {
            // lifecycle `lock` and `unlock` is not paired
            // dtor should not throw exception, so just abort
            std::abort();
        }
    }

    void release() {
        std::unique_lock<std::mutex> lock{ _mutex };
        _released = true;

        auto pos = find_owner_thread();
        if (pos != _owner_threads.end()) {
            // remove current thread use count
            if (pos->dec_use_count) {
                pos->dec_use_count = false;
                _use_count--;
            }

            if (_use_count == 0) {
                _cond.notify_all();
            }
        }

        _cond.wait(lock, [this] {
            return _use_count == 0;
        });
    }

    bool lock(bool& already_locked) {
        std::unique_lock<std::mutex> lock{ _mutex };
        if (_released) {
            return false;
        }

        auto pos = find_owner_thread();
        if (pos != _owner_threads.end()) {
            already_locked = true;
            return true;
        }

        owner_thread item{};
        _owner_threads.push_back(item);
        _use_count++;

        already_locked = false;
        return true;
    }

    void unlock(bool already_locked) {
        if (already_locked) {
            return;
        }

        std::unique_lock<std::mutex> lock{ _mutex };
        auto pos = find_owner_thread();
        if (pos == _owner_threads.end()) {
            throw std::logic_error{"lifecycle `unlock` isn't paired with `lock` in the same thread"};
        }

        if (pos->dec_use_count) {
            _use_count--;
        }

        _owner_threads.erase(pos);

        if (_released && _use_count == 0) {
            _cond.notify_all();
        }
    }

private:
    std::mutex _mutex;
    std::condition_variable _cond;

    int32_t _use_count = 0;
    bool _released = false;

    struct owner_thread {
        std::thread::id id = std::this_thread::get_id();
        bool dec_use_count = true;
    };

    std::vector<owner_thread> _owner_threads;

private:
    // already locked
    std::vector<owner_thread>::iterator find_owner_thread() {
        auto tid = std::this_thread::get_id();

        return std::find_if(_owner_threads.begin(), _owner_threads.end(),
            [tid](const auto& item) {
                return item.id == tid;
            }
        );
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
    explicit object_wrapper(object_lifecycle_ptr<T> lc) : _lc(lc) {
        _id = std::this_thread::get_id();
        _obj = _lc->lock(_already_locked);
    }

    ~object_wrapper() {
        if (std::this_thread::get_id() != _id) {
            // object_wrapper ctor and dtor are not in the same thread,
            // dtor should not throw exception, so just abort
            std::abort();
        }

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
    std::thread::id _id;

    object_lifecycle_ptr<T> _lc;
    bool _already_locked;

    T* _obj;
};

template<class T>
auto use_object(object_lifecycle_ptr<T> lc) {
    return object_wrapper<T>{lc};
}
