#pragma once
#include <stdint.h>
#include <memory>
#include <map>

namespace lru_cache_internal {

template<class K, class V>
class lru_cache_interface
{
public:
    virtual ~lru_cache_interface() { }

    virtual bool query(const K& k, V& v) = 0;

    virtual void clear() = 0;
};

template<class T>
class do_nothing_deletor
{
public:
    void operator()(const T&)
    {
    }
};

template<class V>
struct default_deletor
{
    typedef typename std::_If<std::is_pointer<V>::value,
        std::default_delete<V>,
        do_nothing_deletor<V>>::type type;
};

template<class K, class V, class C, class D = default_deletor<V>::type>
class lru_cache_impl : public lru_cache_interface<K, V>
{
    struct lru_cache_node
    {
        K _key;
        V _val;

        lru_cache_node* _prev;
        lru_cache_node* _next;
    };
public:
    lru_cache_impl(uint32_t max_cache_count, C&& creator = C(), D&& deletor = D()) :
        _max_cache_count(max_cache_count),
        _creator(std::move(creator)),
        _deletor(std::move(deletor))
    {
        _head._next = _head._prev = &_tail;
        _tail._next = _tail._prev = &_head;
        _size = 0;
    }

    ~lru_cache_impl()
    {
        clear();
    }
private:
    bool query(const K& k, V& v)
    {
        auto iter = _nodes.find(k);
        if (iter != _nodes.end()) {
            auto node = iter->second;
            if (node->_prev != &_head) {
                detach(node);
                attach(node);
            }

            v = node->_val;
            return true;
        }

        bool success = _creator(k, v);
        if (!success) return false;

        if (_size == _max_cache_count) {
            auto node = _tail._prev;
            detach(node, true);
            delete node;
        }

        lru_cache_node* node = new lru_cache_node;
        node->_key = k;
        node->_val = v;
        attach(node, true);

        return true;
    }

    void clear()
    {
        auto node = _head._next;
        while (node != &_tail) {
            auto next = node->_next;

            detach(node, true);
            delete node;

            node = next;
        }
    }

    lru_cache_node _head;

    lru_cache_node _tail;

    uint32_t _size;

    void attach(lru_cache_node* node, bool for_insert = false)
    {
        auto prev = &_head;
        auto next = _head._next;
        prev->_next = node;
        next->_prev = node;
        node->_prev = prev;
        node->_next = next;

        if (for_insert) {
            _nodes.emplace(node->_key, node);
            _size++;
        }
    }

    void detach(lru_cache_node* node, bool for_remove = false)
    {
        auto prev = node->_prev;
        auto next = node->_next;
        prev->_next = next;
        next->_prev = prev;

        if (for_remove) {
            _deletor(node->_val);
            _nodes.erase(node->_key);
            _size--;
        }
    }

    uint32_t _max_cache_count;

    std::map<K, lru_cache_node*> _nodes;

    C _creator;

    D _deletor;
};

}

template<class K, class V>
class lru_cache
{
public:
    template<class C, class D>
    lru_cache(uint32_t max_cache_count, C&& creator = C(), D&& deletor = D()) :
        _impl(new lru_cache_internal::lru_cache_impl<K, V, std::decay<C>::type, std::decay<D>::type>(max_cache_count, std::forward<C>(creator), std::forward<D>(deletor)))
    {
    }

    template<class C>
    lru_cache(uint32_t max_cache_count, C&& creator = C()) :
        _impl(new lru_cache_internal::lru_cache_impl<K, V, std::decay<C>::type>(max_cache_count, std::forward<C>(creator)))
    {
    }

    ~lru_cache()
    {
        delete _impl;
    }

    bool query(const K& k, V& v)
    {
        return _impl->query(k, v);
    }

    void clear()
    {
        _impl->clear();
    }
private:
    lru_cache_internal::lru_cache_interface<K, V>* _impl;

    lru_cache(const lru_cache&);

    lru_cache& operator=(const lru_cache&);
};
