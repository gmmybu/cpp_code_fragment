#pragma once
#include "trace.h"
#include <typeinfo>

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

template<class T, bool check_as_parameter, class Deleter>
class checked_ptr
{
protected:
    checked_ptr(T* ptr, const char* name) :
        _ptr(ptr), _name(name) {}

    ~checked_ptr()
    {
        destroy();
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
        if (check_as_parameter && _ptr == nullptr) {
            logger_error_va("%s is nullptr when used as a parameter", _name);
            abort();
        }

        return _ptr;
    }

    T* get()
    {
        return _ptr;
    }

    bool empty()
    {
        return _ptr == nullptr;
    }
protected:
    void destroy()
    {
        if (_ptr != nullptr) {
            Deleter deleter;
            deleter(_ptr);
        }
    }
protected:
    T* _ptr;
    const char* _name;
};

template<class T, bool B, class D>
bool operator==(const checked_ptr<T, B, D>& left, std::nullptr_t)
{
    return left.empty();
}

template<class T, bool B, class D>
bool operator==(std::nullptr_t, const checked_ptr<T, B, D>& right)
{
    return right.empty();
}

template<class T, bool B, class D>
bool operator!=(const checked_ptr<T, B, D>& left, std::nullptr_t)
{
    return !left.empty();
}

template<class T, bool B, class D>
bool operator!=(std::nullptr_t, const checked_ptr<T, B, D>& right)
{
    return !right.empty();
}

template<class T, bool check_as_parameter = true>
class checked_release_ptr : public checked_ptr<T, check_as_parameter, checked_ptr_deleter::Release_Deleter<T>>
{
public:
    checked_release_ptr(T* ptr, const char* name = typeid(T).name() + 6) :
        checked_ptr(ptr, name) {}

    void operator=(T* ptr)
    {
        destroy();
        _ptr = ptr;
    }
private:
    checked_release_ptr(const checked_release_ptr& ptr);
    checked_release_ptr& operator=(const checked_release_ptr& ptr);
};

template<class T, bool check_as_parameter = true>
class checked_delete_ptr : public checked_ptr<T, check_as_parameter, checked_ptr_deleter::Default_Deleter<T>>
{
public:
    checked_delete_ptr(T* ptr, const char* name = typeid(T).name() + 6) :
        checked_ptr(ptr, name) {}

    void operator=(T* ptr)
    {
        destroy();
        _ptr = ptr;
    }
private:
    checked_delete_ptr(const checked_delete_ptr& ptr);
    checked_delete_ptr& operator=(const checked_delete_ptr& ptr);
};

template<class T, bool check_as_parameter = true>
class checked_useonly_ptr : public checked_ptr<T, check_as_parameter, checked_ptr_deleter::DoNothing_Deleter<T>>
{
public:
    checked_useonly_ptr(T* ptr, const char* name = typeid(T).name()) :
        checked_ptr(ptr, name) {}

    void operator=(T* ptr)
    {
        _ptr = ptr;
    }
};
