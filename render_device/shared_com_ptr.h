#pragma once

/// @robust_rank: S
/// @review_date: 2017-10-11

/// make sure ptr is nullptr before raw assign

template<class T>
class shared_com_ptr
{
public:
    shared_com_ptr(T* p = nullptr) : _ptr(p)
    {
    }

    shared_com_ptr(const shared_com_ptr<T>& rhs) : _ptr(nullptr)
    {
        assign(rhs.ptr());
    }

    template<class U>
    explicit shared_com_ptr(U* p) : _ptr(nullptr)
    {
        assign(p);
    }

    template<class U>
    shared_com_ptr(const shared_com_ptr<U>& rhs) : _ptr(nullptr)
    {
        assign(rhs.ptr());
    }

    ~shared_com_ptr()
    {
        reset();
    }

    void reset()
    {
        if (_ptr != nullptr) {
            _ptr->Release();
            _ptr = nullptr;
        }
    }

    operator T*&()
    {
        return _ptr;
    }

    T* operator->() const
    {
        return _ptr;
    }

    template<class U>
    bool assign(U* u)
    {
        reset();

        if (u == nullptr)
            return true;

        if (SUCCEEDED(u->QueryInterface(__uuidof(T), (void**)&_ptr)))
            return true;

        return false;
    }

    bool assign(T* t)
    {
        if (t != nullptr) {
            t->AddRef();
        }

        reset();

        _ptr = t;
        return true;
    }

    template<class U>
    shared_com_ptr& operator=(U* u)
    {
        assign(u);
        return *this;
    }

    template<class U>
    shared_com_ptr& operator=(const shared_com_ptr<U>& rhs)
    {
        return operator=(rhs.ptr());
    }

    shared_com_ptr& operator=(const shared_com_ptr<T>& rhs)
    {
        assign(rhs.ptr());
        return *this;
    }

    shared_com_ptr& operator=(T* p)
    {
        assign(p);
        return *this;
    }

    bool empty() const
    {
        return _ptr == nullptr;
    }

    T* ptr() const
    {
        return _ptr;
    }

    T** operator&()
    {
        return &_ptr;
    }
private:
    T* _ptr;
};

template<class T>
bool operator==(const shared_com_ptr<T>& left, std::nullptr_t)
{
    return left.empty();
}

template<class T>
bool operator==(std::nullptr_t, const shared_com_ptr<T>& right)
{
    return right.empty();
}

template<class T>
bool operator!=(const shared_com_ptr<T>& left, std::nullptr_t)
{
    return !left.empty();
}

template<class T>
bool operator!=(std::nullptr_t, const shared_com_ptr<T>& right)
{
    return !right.empty();
}
