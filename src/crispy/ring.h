/**
 * This file is part of the Contour terminal project
 *   Copyright (c) 2019-2021 Christian Parpart <christian@parpart.family>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <vector>
#include <algorithm>
#include <iterator>

#include <gsl/span>
#include <gsl/span_ext>

namespace crispy
{

template <typename T, typename Vector> struct RingIterator;
template <typename T, typename Vector> struct RingReverseIterator;

/**
 * Implements an efficient ring buffer over type T
 * and the underlying storage Vector.
 */
template<
    typename T,
    typename Vector = std::vector<T>
>
class basic_ring
{
public:
    using iterator = RingIterator<T, Vector>;
    using const_iterator = RingIterator<T const, Vector>;
    using reverse_iterator = RingReverseIterator<T, Vector>;
    using const_reverse_iterator = RingReverseIterator<T const, Vector>;

    basic_ring() = default;
    basic_ring(basic_ring const&) = default;
    basic_ring& operator=(basic_ring const&) = default;
    basic_ring(basic_ring &&) noexcept = default;
    basic_ring& operator=(basic_ring &&) noexcept = default;
    virtual ~basic_ring() = default;

    explicit basic_ring(Vector storage): _storage(std::move(storage)) {}

    T const& operator[](std::size_t i) const noexcept { return _storage[(_zero + i) % size()]; }
    T& operator[](std::size_t i) noexcept { return _storage[(_zero + i) % size()]; }

    T const& at(std::size_t i) const noexcept { return _storage[(_zero + i) % size()]; }
    T& at(std::size_t i) noexcept { return _storage[(_zero + i) % size()]; }

    Vector& storage() noexcept { return _storage; }
    Vector const& storage() const noexcept { return _storage; }
    std::size_t zero_index() const noexcept { return _zero; }

    void rezero(iterator i);
    void rezero();

    std::size_t size() const noexcept { return _storage.size(); }

    void rotate(int count) noexcept { _zero = (_zero + size() + count) % size(); }
    void rotate_left(std::size_t count) noexcept { _zero = (_zero + size() + count) % size(); }
    void rotate_right(std::size_t count) noexcept { _zero = (_zero + size() - count) % size(); }
    void unrotate() { _zero = 0; }

    iterator begin() noexcept { return iterator{this, _zero}; }
    iterator end() noexcept { return iterator{this, _zero + size()}; }

    const_iterator begin() const noexcept { return const_iterator{this, _zero}; }
    const_iterator end() const noexcept { return const_iterator{this, _zero + size()}; }

    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return begin(); }

    reverse_iterator rbegin() noexcept;
    reverse_iterator rend() noexcept;

    gsl::span<T> span(size_t start, size_t count) noexcept
    {
        auto a = std::next(begin(), start);
        auto b = std::next(a, count);
        return gsl::make_span(a, b);
    }

    gsl::span<T const> span(size_t start, size_t count) const noexcept
    {
        auto a = std::next(begin(), start);
        auto b = std::next(a, count);
        return gsl::make_span(a, b);
    }

protected:
    Vector _storage;
    std::size_t _zero = 0;
};

/**
 * Implements an efficient ring buffer over type T
 * and the underlying dynamic storage type Vector<T, Allocator>.
 */
template<
    typename T,
    template <typename, typename> class Container = std::vector,
    typename Allocator = std::allocator<T>
>
class ring: public basic_ring<T, Container<T, Allocator>>
{
public:
    using basic_ring<T, Container<T, Allocator>>::basic_ring;

    ring(size_t capacity, T value): ring(Container<T, Allocator>(capacity, value)) {}
    explicit ring(size_t capacity): ring(capacity, T{}) {}

    void reserve(size_t capacity) { this->_storage.reserve(capacity); }
    void resize(size_t newSize) { this->_storage.resize(newSize); }
    void push_back(T const& _value) { this->_storage.push_back(_value); }
    void emplace_back(T _value) { this->_storage.emplace_back(std::move(_value)); }
};

/// Fixed-size basic_ring<T> implementation
template <typename T, std::size_t N>
using fixed_size_ring = basic_ring<T, std::array<T, N>>;

// {{{ iterator
template <typename T, typename Vector>
struct RingIterator
{
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = long;
    using pointer = T*;
    using reference = T&;

    basic_ring<T, Vector>* ring{};
    std::size_t current{};

    RingIterator(RingIterator const&) = default;
    RingIterator& operator=(RingIterator const&) = default;

    RingIterator(RingIterator &&) noexcept = default;
    RingIterator& operator=(RingIterator &&) noexcept = default;

    RingIterator& operator++() noexcept { ++current; return *this; }
    RingIterator& operator++(int) noexcept { return ++(*this); }

    RingIterator& operator--() noexcept { --current; return *this; }
    RingIterator& operator--(int) noexcept { return --(*this); }

    RingIterator& operator+=(int n) noexcept { current += n; return *this; }
    RingIterator& operator-=(int n) noexcept { current -= n; return *this; }

    RingIterator operator+(difference_type n) noexcept { return RingIterator{ring, current + n}; }
    RingIterator operator-(difference_type n) noexcept { return RingIterator{ring, current - n}; }

    RingIterator operator+(RingIterator const& rhs) const noexcept { return RingIterator{ring, current + rhs.current}; }
    difference_type operator-(RingIterator const& rhs) const noexcept { return current - rhs.current; }

    friend RingIterator operator+(difference_type n, RingIterator a) { return RingIterator{a.ring, n + a.current}; }
    friend RingIterator operator-(difference_type n, RingIterator a) { return RingIterator{a.ring, n - a.current}; }

    bool operator==(RingIterator const& rhs) const noexcept { return current == rhs.current; }
    bool operator!=(RingIterator const& rhs) const noexcept { return current != rhs.current; }

    T& operator*() noexcept { return (*ring)[current % ring->size()]; }
    T const& operator*() const noexcept { return (*ring)[current % ring->size()]; }
};
// }}}

 // {{{ reverse iterator
template <typename T, typename Vector>
struct RingReverseIterator
{
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = long;
    using pointer = T*;
    using reference = T&;

    basic_ring<T, Vector>* ring;
    std::size_t current;

    RingReverseIterator(RingReverseIterator const&) = default;
    RingReverseIterator& operator=(RingReverseIterator const&) = default;

    RingReverseIterator(RingReverseIterator &&) noexcept = default;
    RingReverseIterator& operator=(RingReverseIterator &&) noexcept = default;

    RingReverseIterator& operator++() noexcept { ++current; return *this; }
    RingReverseIterator& operator++(int) noexcept { return ++(*this); }

    RingReverseIterator& operator--() noexcept { --current; return *this; }
    RingReverseIterator& operator--(int) noexcept { return --(*this); }

    RingReverseIterator& operator+=(int n) noexcept { current += n; return *this; }
    RingReverseIterator& operator-=(int n) noexcept { current -= n; return *this; }

    RingReverseIterator operator+(difference_type n) noexcept { return RingReverseIterator{ring, current + n}; }
    RingReverseIterator operator-(difference_type n) noexcept { return RingReverseIterator{ring, current - n}; }

    RingReverseIterator operator+(RingReverseIterator const& rhs) const noexcept { return RingReverseIterator{ring, current + rhs.current}; }
    difference_type operator-(RingReverseIterator const& rhs) const noexcept { return current - rhs.current; }

    friend RingReverseIterator operator+(difference_type n, RingReverseIterator a) { return RingReverseIterator{a.ring, n + a.current}; }
    friend RingReverseIterator operator-(difference_type n, RingReverseIterator a) { return RingReverseIterator{a.ring, n - a.current}; }

    bool operator==(RingReverseIterator const& rhs) const noexcept { return current == rhs.current; }
    bool operator!=(RingReverseIterator const& rhs) const noexcept { return current != rhs.current; }

    T& operator*() noexcept { return (*ring)[ring->size() - current - 1]; }
    T const& operator*() const noexcept { return (*ring)[ring->size() - current - 1]; }
};
// }}}

// {{{ basic_ring<T> impl
template <typename T, typename Vector>
typename basic_ring<T, Vector>::reverse_iterator basic_ring<T, Vector>::rbegin() noexcept
{
    return reverse_iterator{this, 0};
}

template <typename T, typename Vector>
typename basic_ring<T, Vector>::reverse_iterator basic_ring<T, Vector>::rend() noexcept
{
    return reverse_iterator{this, size()};
}

template <typename T, typename Vector>
void basic_ring<T, Vector>::rezero()
{
    std::rotate(begin(), std::next(begin(), _zero), end()); // shift-left
    _zero = 0;
}

template <typename T, typename Vector>
void basic_ring<T, Vector>::rezero(iterator i)
{
    std::rotate(begin(), std::next(begin(), i.current), end()); // shift-left
    _zero = 0;
}
// }}}

}