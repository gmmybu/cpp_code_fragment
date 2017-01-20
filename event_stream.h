#pragma once
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <functional>

template<class T>
class event_stream_base
{
public:
    typedef std::function<T> func_type;
protected:
    struct event_observer
    {
        func_type _handler;

        uint32_t _guid;

        bool _removed;
    };

    struct event_stream_guard
    {
    public:
        event_stream_guard(event_stream_base& owner) :
            _owner(owner)
        {
            if (_owner._is_emitting)
                throw std::exception("call event_stream::emit nested");
            
            _owner._is_emitting = true;
        }

        ~event_stream_guard()
        {
            _owner._is_emitting = false;
        }

        event_stream_base& _owner;
        
        event_stream_guard(const event_stream_guard&);
        event_stream_guard& operator=(const event_stream_guard&);
    };
public:
    event_stream_base() :
        _guid_index(10000),
        _is_emitting(false)
    {
    }

    uint32_t subscribe(func_type&& t)
    {
        if (t == nullptr) return 0;
    
        uint32_t guid = _guid_index++;
        event_observer ob = { std::move(t), guid, false };
        append_observer(ob);
        return guid;
    }

    void unsubscribe(uint32_t guid)
    {
        if (guid != 0) {
            remove_observer(guid);
        }
    }
protected:
    void append_observer(event_observer& ob)
    {
        if (_is_emitting) {
            _incomings.push_back(std::move(ob));
        } else {
            _observers.push_back(std::move(ob));
        }
    }

    void remove_observer(uint32_t guid)
    {
        for (auto& ob : _incomings) {
            if (ob._guid == guid) {
                ob._removed = true;
                break;
            }
        }
        
        erase_observer(_incomings);

        for (auto& ob : _observers) {
            if (ob._guid == guid) {
                ob._removed = true;
                break;
            }
        }

        if (_is_emitting) {
            _has_removed = true;
        } else {
            erase_observer(_observers);
        }
    }

    void erase_observer(std::vector<event_observer>& observers)
    {
        observers.erase(
            std::remove_if(observers.begin(), observers.end(),
                [](const event_observer& ob) { return ob._removed; }
            ),
            observers.end()
        );
    }

    void after_emit()
    {
        if (_has_removed) {
            _has_removed = false;
            erase_observer(_observers);
        }

        if (!_incomings.empty()) {
            _observers.insert(_observers.end(), _incomings.begin(), _incomings.end());
            _incomings.clear();
        }
    }
protected:
    uint32_t _guid_index;

    std::vector<event_observer> _observers;

    std::vector<event_observer> _incomings;

    bool _has_removed;

    bool _is_emitting;
private:
    event_stream_base(const event_stream_base&);
    event_stream_base& operator=(event_stream_base&);
};

template<class T>
class event_stream
{
};

template<>
class event_stream<void()> : public event_stream_base<void()>
{
public:
    void emit()
    {
        event_stream_guard(*this);
        for (auto& ob : _observers) {
            if (!ob._removed) {
                ob._handler();
            }
        }
        after_emit();
    }
};

template<class P1>
class event_stream<void(P1)> : public event_stream_base<void(P1)>
{
public:
    void emit(P1 p1)
    {
        event_stream_guard(*this);
        for (auto& ob : _observers) {
            if (!ob._removed) {
                ob._handler(p1);
            }
        }
        after_emit();
    }
};

template<class P1, class P2>
class event_stream<void(P1, P2)> : public event_stream_base<void(P1, P2)>
{
public:
    void emit(P1 p1, P2 p2)
    {
        event_stream_guard(*this);
        for (auto& ob : _observers) {
            if (!ob._removed) {
                ob._handler(p1, p2);
            }
        }
        after_emit();
    }
};

template<class P1, class P2, class P3>
class event_stream<void(P1, P2, P3)> : public event_stream_base<void(P1, P2, P3)>
{
public:
    void emit(P1 p1, P2 p2, P3 p3)
    {
        event_stream_guard(*this);
        for (auto& ob : _observers) {
            if (!ob._removed) {
                ob._handler(p1, p2, p3);
            }
        }
        after_emit();
    }
};
