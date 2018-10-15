#pragma once
#include "trace.h"
#include <typeinfo>
#include <string.h>

namespace checked_ptr_deleter {

template<class T>
struct Default_Deleter
{
    void operator()(T* ptr) { delete ptr; }
};

template<class T>
struct Release_Deleter
{
    void operator()(T* ptr) { ptr->release(); }
};

template<class T>
struct DoNothing_Deleter
{
    void operator()(T* ptr) {}
};

}

template<class T, class Deleter>
class checked_ptr
{
protected:
    checked_ptr(T* ptr, const char* name) :
        _ptr(ptr), _name(name)
    {
        if (_name == nullptr) {
            _name = "noname";
        }

        if (strncmp(_name, "class ", 6) == 0) {
            _name += 6;
        }

        if (strncmp(_name, "struct ", 7) == 0) {
            _name += 7;
        }
    }

    ~checked_ptr()
    {
        destroy();
    }

    void reset(T* ptr)
    {
        if (_ptr != ptr) {
            destroy();
            _ptr = ptr;
        }
    }

    void destroy()
    {
        if (_ptr != nullptr) {
            Deleter deleter;
            deleter(_ptr);
            _ptr = nullptr;
        }
    }

    T* release()
    {
        T* ptr = _ptr;
        _ptr = nullptr;
        return ptr;
    }

    T* get() const
    {
        return _ptr;
    }

    bool empty() const
    {
        return _ptr == nullptr;
    }
public:
    T* operator->() const
    {
        if (_ptr == nullptr) {
            logger_error_va("%s is nullptr when call member function", _name);
            abort();
        }

        return _ptr;
    }

    operator T*() const
    {
        return _ptr;
    }
protected:
    T* _ptr;
    const char* _name;
private:
    checked_ptr(const checked_ptr& ptr);
    checked_ptr& operator=(const checked_ptr& ptr);
};

template<class T, class D>
bool operator==(const checked_ptr<T, D>& left, std::nullptr_t)
{
    return left.empty();
}

template<class T, class D>
bool operator==(std::nullptr_t, const checked_ptr<T, D>&& right)
{
    return right.empty();
}

template<class T, class D>
bool operator!=(const checked_ptr<T, D>&& left, std::nullptr_t)
{
    return !left.empty();
}

template<class T, class D>
bool operator!=(std::nullptr_t, const checked_ptr<T, D>&& right)
{
    return !right.empty();
}

template<class T>
class checked_release_ptr : public checked_ptr<T, checked_ptr_deleter::Release_Deleter<T>>
{
public:
    checked_release_ptr(T* ptr, const char* name = typeid(T).name()) :
        checked_ptr(ptr, name) {}
};

template<class T>
class checked_delete_ptr : public checked_ptr<T, checked_ptr_deleter::Default_Deleter<T>>
{
public:
    checked_delete_ptr(T* ptr, const char* name = typeid(T).name()) :
        checked_ptr(ptr, name) {}
};

template<class T>
class checked_useonly_ptr : public checked_ptr<T, checked_ptr_deleter::DoNothing_Deleter<T>>
{
public:
    checked_useonly_ptr(T* ptr, const char* name = typeid(T).name()) :
        checked_ptr(ptr, name) {}
};

