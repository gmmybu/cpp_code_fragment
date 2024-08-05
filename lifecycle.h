#pragma once
#include <mutex>
#include <condition_variable>
#include <vector>
#include <thread>

template<class T>
class object_wrapper;

template<class T>
class lifecycle final {
public:
    lifecycle(T* obj) : _owner_threads(), _obj(obj) {
        if (_obj == nullptr) {
            std::abort(); // logic error
        }

        _owner_threads.reserve(32);
    }

    void release() {
        std::unique_lock<std::mutex> lock{ _mutex };
        _released = true;

        auto pos = find_owner_thread();
        if (pos != _owner_threads.end()) {
            // remove current thread use count
            pos->dec_use_count = false;
            _use_count--;

            if (_use_count == 0) {
                _cond.notify_all();
            }
        }

        _cond.wait(lock, [this] {
            return _use_count == 0;
        });
    }

private:
    T* lock(bool& already_locked) {
        std::unique_lock<std::mutex> lock{ _mutex };
        if (_released) {
            return nullptr;
        }

        auto pos = find_owner_thread();
        if (pos != _owner_threads.end()) {
            already_locked = true;
            return _obj;
        }

        owner_thread item{};
        _owner_threads.push_back(item);
        _use_count++;

        already_locked = false;
        return _obj;
    }

    void unlock(bool already_locked) {
        if (already_locked) {
            return;
        }

        std::unique_lock<std::mutex> lock{ _mutex };
        auto pos = find_owner_thread();
        if (pos == _owner_threads.end()) {
            std::abort(); // logic error
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

    
    template<class T>
    friend class object_wrapper;

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

private:
    T* _obj;
};

// usage:
// 
// if (auto obj = use_object(lc); obj) {
//     // use obj
// }
template<class T>
class object_wrapper {
public:
    explicit object_wrapper(std::shared_ptr<lifecycle<T>> lc) : _lc(lc) {
        _id = std::this_thread::get_id();
        _obj = _lc->lock(_already_locked);
    }

    ~object_wrapper() {
        if (std::this_thread::get_id() != _id) {
            std::abort(); // logic error
        }

        if (_obj) {
            _lc->unlock(_already_locked);
        }
    }

    operator bool() const {
        if (std::this_thread::get_id() != _id) {
            std::abort(); // logic error
        }

        return _obj != nullptr;
    }

    T* operator->() const {
        if (std::this_thread::get_id() != _id) {
            std::abort(); // logic error
        }

        return _obj;
    }

    object_wrapper(const object_wrapper&) = delete;
    object_wrapper& operator=(const object_wrapper&) = delete;

private:
    std::thread::id _id;

    std::shared_ptr<lifecycle<T>> _lc;
    bool _already_locked;

    T* _obj;
};

template<class T>
auto use_object(std::shared_ptr<lifecycle<T>> lc) {
    return object_wrapper<T>{lc};
}
