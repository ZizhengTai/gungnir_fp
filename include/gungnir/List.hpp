/*
 * Copyright 2016 Zizheng Tai
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GUNGNIR_LIST_HPP
#define GUNGNIR_LIST_HPP

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "gungnir/detail/util.hpp"

namespace gungnir {

using namespace detail;

/**
 * @brief An immutable linked list.
 *
 * @author Zizheng Tai
 * @since 1.0
 * @tparam A the type of the elements; must be a non-reference type
 */
template<typename A>
class List final {
public:
    /**
     * @brief Constructs an empty list.
     */
    List() noexcept : size_(0), node_(Node::create()) {}

    /**
     * @brief Constructs a list with the given element.
     *
     * @param x the only element of this list
     */
    explicit List(A x) noexcept
        : size_(1)
        , node_(Node::create(
                    std::make_shared<const A>(std::move(x)),
                    Node::create()))
    {}

    /**
     * @brief Constructs a list with the given head and tail.
     *
     * @param head the first element of this list
     * @param tail all elements of this list except the first one
     */
    List(A head, List tail) noexcept
        : size_(tail.size_ + 1)
        , node_(Node::create(
                    std::make_shared<const A>(std::move(head)),
                    std::move(tail.node_)))
    {}

    /**
     * @brief Constructs a list with the given elements.
     *
     * @tparam Args the types of the given elements
     * @param head the first element of this list
     * @param tail all elements of this list except the first one
     */
    template<
        typename... Args,
        typename = typename std::enable_if<
            AllTrue<std::is_convertible<Args, A>::value...>::value
        >::type
    >
    List(A head, Args&&... tail) noexcept
        : List(std::move(head), List(std::forward<Args>(tail)...))
    {}

    /**
     * @brief Constructs a list with the contents of the range [`first`, `last`).
     *
     * @tparam InputIt the type of the iterators
     * @param first the iterator pointing to the start of the range
     * @param last the iterator pointing to the end of the range
     */
    template<
        typename InputIt,
        typename = typename std::enable_if<std::is_convertible<
            typename std::iterator_traits<InputIt>::value_type, A
        >::value>::type
    >
    List(InputIt first, InputIt last) noexcept
        : List([&first, &last] {
            std::vector<Ptr<A>> buf;
            buf.reserve(std::distance(first, last));
            for (; first != last; ++first) {
                buf.emplace_back(std::make_shared<A>(*first));
            }

            return toList(
                    buf.size(),
                    std::make_move_iterator(buf.rbegin()),
                    std::make_move_iterator(buf.rend()));
        }())
    {}

    /** @brief Default copy constructor. */
    List(const List&) = default;

    /** @brief Default move constructor. */
    List(List&&) = default;

    /** @brief Default copy assignment operator. */
    List& operator=(const List&) = default;

    /** @brief Default move assignment operator. */
    List& operator=(List&&) = default;

    /**
     * @brief Returns `true` if this list contains no elements, `false` otherwise.
     *
     * @return `true` if this list contains no elements, `false` otherwise
     */
    bool isEmpty() const
    {
        return size_ == 0;
    }

    /**
     * @brief Returns the number of elements of this list.
     *
     * @return the number of elements of this list
     */
    std::size_t size() const
    {
        return size_;
    }

    /**
     * @brief Returns the first element of this list.
     *
     * @return the first element of this list
     * @throws std::out_of_range if this list is empty
     */
    const A& head() const
    {
        if (isEmpty()) {
            throw std::out_of_range("head of empty list");
        }
        return *(node_->head);
    }

    /**
     * @brief Returns all elements of this list except the first one.
     *
     * @return all elements of this list except the first one
     * @throws std::out_of_range if this list is empty
     */
    List tail() const
    {
        if (isEmpty()) {
            throw std::out_of_range("tail of empty list");
        }
        return List(size() - 1, node_->tail);
    }

    /**
     * @brief Returns a pair consisting of the head and tail of this list.
     *
     * @return a pair consisting of the head and tail of this list
     * @throws std::out_of_range if this list is empty
     */
    std::pair<std::reference_wrapper<const A>, List> uncons() const
    {
        if (isEmpty()) {
            throw std::out_of_range("uncons on empty list");
        }
        return std::make_pair(std::cref(head()), tail());
    }

    /**
     * @brief Returns the last element of this list.
     *
     * @return the last element of this list
     * @throws std::out_of_range if this list is empty
     */
    const A& last() const
    {
        if (isEmpty()) {
            throw std::out_of_range("last of empty list");
        }
        return (*this)[size() - 1];
    }

    /**
     * @brief Returns all elements of this list except the last one.
     *
     * @return all elements of this list except the last one
     * @throws std::out_of_range if this list is empty
     */
    List init() const
    {
        if (isEmpty()) {
            throw std::out_of_range("init of empty list");
        }
        return take(size() - 1);
    }

    /**
     * Applies a function to each element of this list.
     *
     * @param f the function to apply, for its side-effect,
     *          to each element of this list
     */
    template<typename Fn>
    void foreach(Fn f) const
    {
        foreachImpl([&f](const Ptr<A>& x) { f(*x); });
    }

    /**
     * @brief Returns a new list resulting from applying a function to
     *        each element of this list.
     *
     * @tparam Fn the type of the function to apply to each element of this list
     * @tparam B the element type of returned list
     * @param f the function to apply to each element of this list
     * @return a new list resulting from applying the given function `f` to
     *         each element of this list
     */
    template<typename Fn, typename B = Decay<Ret<Fn, A>>>
    List<B> map(Fn f) const
    {
        const auto ff = std::bind(std::move(f), std::placeholders::_1);

        std::vector<Ptr<B>> buf;
        buf.reserve(size());
        foreachImpl([&buf, &ff](const Ptr<A>& x) {
            buf.emplace_back(std::make_shared<const B>(ff(*x)));
        });

        return List<B>::toList(
                size(),
                std::make_move_iterator(buf.rbegin()),
                std::make_move_iterator(buf.rend()));
    }

    /**
     * @brief Returns all elements of this list that satisfy a predicate.
     *
     * The order of the elements is preserved.
     *
     * @tparam Fn type of the predicate
     * @param p the predicate used to test elements
     * @return a new list consisting of all elements of this list that satisfy
     *         the given predicate `p`
     */
    template<typename Fn>
    List filter(Fn p) const
    {
        std::vector<Ptr<A>> buf;
        foreachImpl([&p, &buf](const Ptr<A>& x) {
            if (p(*x)) {
                buf.emplace_back(x);
            }
        });

        return toList(
                buf.size(),
                std::make_move_iterator(buf.rbegin()),
                std::make_move_iterator(buf.rend()));
    }

    /**
     * @brief Returns all elements of this list that violate a predicate.
     *
     * The order of the elements is preserved.
     *
     * @tparam Fn type of the predicate
     * @param p the predicate used to test elements
     * @return a new list consisting of all elements of this list that violate
     *         the given predicate `p`
     */
    template<typename Fn>
    List filterNot(Fn p) const
    {
        return filter([&p](const A& x) { return !p(x); });
    }

    /**
     * @brief Returns a new list with elements of this list in reversed order.
     *
     * @return a new list with elements of this list in reversed order
     */
    List reverse() const
    {
        auto hd = Node::create();
        foreachImpl([&hd](const Ptr<A>& x) {
            hd = Node::create(x, std::move(hd));
        });
        return List(size(), std::move(hd));
    }

    /**
     * @brief Returns the first `n` elements of this list.
     *
     * @param n the number of elements to take
     * @return a list consisting of the first `n` elements of this list,
     *         or the whole list if `n > size()`
     */
    List take(std::size_t n) const
    {
        n = std::min(n, size());

        std::vector<Ptr<A>> buf;
        for (auto nd = node_.get(); n > 0; nd = nd->tail.get(), --n) {
            buf.emplace_back(nd->head);
        }

        return toList(
                buf.size(),
                std::make_move_iterator(buf.rbegin()),
                std::make_move_iterator(buf.rend()));
    }

    /**
     * @brief Returns the last `n` elements of this list.
     *
     * @param n the number of elements to take
     * @return a list consisting of the last `n` elements of this list,
     *         or the whole list if `n > size()`
     */
    List takeRight(std::size_t n) const
    {
        return drop(size() - std::min(n, size()));
    }

    /**
     * @brief Returns the longest prefix of this list whose elements satisfy
     *        the given predicate.
     *
     * @tparam Fn the type of the predicate
     * @param p the predicate
     * @return the longest prefix of this list whose elements satisfy
     *         the given predicate
     */
    template<typename Fn>
    List takeWhile(Fn p) const
    {
        std::vector<Ptr<A>> buf;
        for (auto n = node_.get();
                n->head && p(*(n->head));
                n = n->tail.get()) {
            buf.emplace_back(n->head);
        }

        return toList(
                buf.size(),
                std::make_move_iterator(buf.rbegin()),
                std::make_move_iterator(buf.rend()));
    }

    /**
     * @brief Returns all elements of this list except the first `n` ones.
     *
     * @param n the number of elements to drop
     * @return a list consisting of all elements of this list except
     *         the first `n` ones, or an empty list if `n > size()`
     */
    List drop(std::size_t n) const
    {
        if (n >= size()) {
            return List();
        }
        auto pn = &node_;
        for (std::size_t i = 0; i < n; pn = &(*pn)->tail, ++i) {}
        return List(size() - n, *pn);
    }

    /**
     * @brief Returns all elements of this list except the last `n` ones.
     *
     * @param n the number of elements to drop
     * @return a list consisting of all elements of this list except
     *         the last `n` ones, or an empty list if `n > size()`
     */
    List dropRight(std::size_t n) const
    {
        return take(size() - std::min(n, size()));
    }

    /**
     * @brief Returns the longest suffix of this list whose first element
     *        does not satisfy the given predicate.
     *
     * @tparam Fn the type of the predicate
     * @param p the predicate
     * @return the longest suffix of this list whose first element does not
     *         satisfy the given predicate
     */
    template<typename Fn>
    List dropWhile(Fn p) const
    {
        auto s = size();
        auto pn = &node_;
        for (; (*pn)->head && p(*((*pn)->head)); --s, pn = &(*pn)->tail) {}
        return List(s, *pn);
    }

    /**
     * @brief Returns a list consisting of all elements of this list starting at
     *        position `from` and extending up until position `until`.
     *
     * An empty list is returned if `from >= until` or `from >= size()`.
     *
     * @param from the index of the starting position (included)
     * @param until the index of the ending position (excluded)
     * @return a list consisting of all elements of this list starting at
     *         position `from` and extending up until position `until`,
     *         or an empty list if `from >= until` or `from >= size()`
     */
    List slice(std::size_t from, std::size_t until) const
    {
        if (from >= until) {
            return List();
        }
        return drop(from).take(until - from);
    }

    /**
     * @brief Returns a list resulting from applying the given function `f`
     *        to each element of this list and concatenating the results.
     *
     * @tparam Fn the type of the function to apply to each element of this list
     * @tparam B the element type of the returned list
     * @return a list resulting from applying the given function `f` to each
     *         element of this list and concatenating the results
     */
    template<typename Fn, typename B = Decay<typename HKT<Ret<Fn, A>>::L>>
    List<B> flatMap(Fn f) const
    {
        std::vector<Ptr<B>> buf;
        foreachImpl([&f, &buf](const Ptr<A>& x) {
            f(*x).foreachImpl([&buf](const Ptr<B>& y) {
                buf.emplace_back(y);
            });
        });

        return List<B>::toList(
                buf.size(),
                std::make_move_iterator(buf.rbegin()),
                std::make_move_iterator(buf.rend()));
    }

    /**
     * @brief Returns a list resulting from concatenating all element lists
     *        of this list.
     *
     * @tparam A1 the same as A, used to make SFINAE work
     * @tparam B the element type of the returned list
     * @return a list resulting from concatenating all element lists of this list.
     */
    template<typename A1 = A, typename B = typename HKT<A1>::L>
    List<B> flatten() const
    {
        std::vector<Ptr<B>> buf;
        foreachImpl([&buf](const Ptr<List<B>>& ys) {
            ys->foreachImpl([&buf](const Ptr<B>& y) {
                buf.emplace_back(y);
            });
        });

        return List<B>::toList(
                buf.size(),
                std::make_move_iterator(buf.rbegin()),
                std::make_move_iterator(buf.rend()));
    }

    /**
     * @brief Returns `true` if at least one element of this list satisfy
     *        the given predicate, `false` otherwise.
     *
     * @tparam Fn the type of the predicate
     * @param p the predicate
     * @return `true` if at least one element of this list satisfy the given
     *         predicate, `false` otherwise
     */
    template<typename Fn>
    bool exists(Fn p) const
    {
        for (auto n = node_.get(); n->head; n = n->tail.get()) {
            if (p(*(n->head))) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Returns `true` if this list is empty or the given predicate
     *        holds for all elements of this list, `false` otherwise.
     *
     * @tparam Fn the type of the predicate
     * @param p the predicate
     * @return `true` if this list is empty or the given predicate holds for
     *         all elements of this list, `false` otherwise
     */
    template<typename Fn>
    bool forall(Fn p) const
    {
        for (auto n = node_.get(); n->head; n = n->tail.get()) {
            if (!p(*(n->head))) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Returns `true` if this list has an element that is equal
     *        (as determined by `==`) to `x`, `false` otherwise.
     *
     * @param x the object to test against
     * @return `true` if this list has an element that is equal
     *         (as determined by `==`) to `x`, `false` otherwise
     */
    bool contains(const A& x) const
    {
        for (auto n = node_.get(); n->head; n = n->tail.get()) {
            if (*(n->head) == x) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Returns the number of elements of this list that are equal
     *        (as determined by `==`) to `x`.
     *
     * @param x the object to test against
     * @return the number of elements of this list that are equal
     *         (as determined by `==`) to `x`
     */
    std::size_t count(const A& x) const
    {
        std::size_t num = 0;
        foreachImpl([&x, &num](const Ptr<A>& y) {
            if (*y == x) {
                ++num;
            }
        });
        return num;
    }

    /**
     * @brief Returns the number of elements of this list that satisfy
     *        the given predicate.
     *
     * @tparam Fn the type of the predicate
     * @param p the predicate
     * @return the number of elements of this list that satisfy
     *         the given predicate
     */
    template<typename Fn>
    std::size_t count(Fn p) const
    {
        std::size_t num = 0;
        foreachImpl([&p, &num](const Ptr<A>& y) {
            if (p(*y)) {
                ++num;
            }
        });
        return num;
    }

    /**
     * @brief Returns a list whose head is constructed in-place from `args`,
     *        and tail is this list.
     *
     * @tparam Args the types of the arguments passed to the constructor of `A`
     * @param args the arguments passed to the constructor of `A`
     * @return a list whose head is constructed in-place from `args`,
     *         and tail is this list
     */
    template<typename... Args>
    List prepend(Args&&... args) const
    {
        return List(
                size() + 1,
                Node::create(
                    std::make_shared<A>(std::forward<Args>(args)...),
                    node_));
    }

    /**
     * @brief Returns a list resulting from concatenating this list and `that`.
     *
     * @param that the list whose elements follow those of this list
     *             in the returned list
     * @return a list resulting from concatenating this list and `that`
     */
    List concat(const List& that) const
    {
        if (isEmpty()) {
            return that;
        } else if (that.isEmpty()) {
            return *this;
        }

        std::vector<Ptr<A>> buf;
        buf.reserve(size());
        foreachImpl([&buf](const Ptr<A>& x) {
            buf.emplace_back(x);
        });

        return toList(
                size() + that.size(),
                std::make_move_iterator(buf.rbegin()),
                std::make_move_iterator(buf.rend()),
                that.node_);
    }

    /**
     * @brief Returns a copy of this list with one single replaced element.
     *
     * @tparam Args the types of the argument passed to the constructor of `A`
     * @param index the position of the replacement
     * @param args the argument passed to the constructor of `A`
     * @return a copy of this list with the element at position `index`
     *         replaced by a new element constructed in-place from `args`
     * @throws std::out_of_range if `index >= size()`
     */
    template<typename... Args>
    List updated(std::size_t index, Args&&... args) const
    {
        if (index >= size()) {
            throw std::out_of_range("index out of range");
        }

        std::vector<Ptr<A>> buf;
        buf.reserve(index);
        auto n = node_.get();
        for (; index > 0; n = n->tail.get(), --index) {
            buf.emplace_back(n->head);
        }

        return toList(
                size(),
                std::make_move_iterator(buf.rbegin()),
                std::make_move_iterator(buf.rend()),
                Node::create(
                    std::make_shared<A>(std::forward<Args>(args)...),
                    n->tail));
    }

    /**
     * @brief Folds the elements of this list using the given associative
     *        binary operator.
     *
     * The order in which operations are performed on elements is unspecified
     * and may be nondeterministic.
     *
     * @tparam A1 the result type of the binary operator, a supertype of `A`
     * @tparam Fn the type of the associative binary operator
     * @param z a neutral element for the fold operation; may be added to
     *          the result an arbitrary number of times, and must not change
     *          the result (e.g., an empty list for list concatenation,
     *          0 for addition, or 1 for multiplication)
     * @param op a binary operator that must be associative
     * @return the result of applying the fold operator `op` between all
     *         elements of this list and `z`, or `z` if this list is empty
     */
    template<
        typename A1,
        typename Fn,
        typename = typename std::enable_if<
            std::is_same<A1, A>::value || std::is_base_of<A1, A>::value
        >::type
    >
    A1 fold(A1 z, Fn op) const
    {
        return foldLeft(std::move(z), std::move(op));
    }

    /**
     * @brief Applies a binary operator to a start value and all elements of
     *        this list, going left to right.
     *
     * @tparam B the result type of the binary operator
     * @tparam Fn the type of the binary operator
     * @param z the start value
     * @param op the binary operator
     * @return the result of inserting `op` between consecutive elements of
     *         this list, going left to right with the start value `z`
     *         on the left, or `z` if this list is empty
     */
    template<typename B, typename Fn>
    B foldLeft(B z, Fn op) const
    {
        foreachImpl([&z, &op](const Ptr<A>& x) {
            z = op(std::move(z), *x);
        });
        return z;
    }

    /**
     * @brief Applies a binary operator to a start value and all elements of
     *        this list, going right to left.
     *
     * @tparam B the result type of the binary operator
     * @tparam Fn the type of the binary operator
     * @param z the start value
     * @param op the binary operator
     * @return the result of inserting `op` between consecutive elements of
     *         this list, going right to left with the start value `z`
     *         on the right, or `z` if this list is empty
     */
    template<typename B, typename Fn>
    B foldRight(B z, Fn op) const
    {
        std::vector<const A*> buf;
        buf.reserve(size());
        foreachImpl([&buf](const Ptr<A>& x) {
            buf.emplace_back(&*x);
        });

        for (auto it = buf.crbegin(); it != buf.crend(); ++it) {
            z = op(**it, std::move(z));
        }
        return z;
    }

    /**
     * @brief Returns the sum of all elements of this list,
     *        or 0 if this list is empty.
     *
     * @return the sum of all elements of this list, or 0 if this list is empty
     */
    A sum() const
    {
        A acc = 0;
        foreachImpl([&acc](const Ptr<A>& x) {
            acc += *x;
        });
        return acc;
    }

    /**
     * @brief Returns the product of all elements of this list,
     *        or 1 if this list is empty.
     *
     * @return the product of all elements of this list,
     *         or 1 if this list is empty
     */
    A product() const
    {
        A acc = 1;
        foreachImpl([&acc](const Ptr<A>& x) {
            acc *= *x;
        });
        return acc;
    }

    /**
     * @brief Returns a list consisting of elements of this list sorted in
     *        ascending order, as determined by the `<` operator.
     *
     * If `stable` is `true`, the sort will be stable. That is, equal elements
     * appear in the same order in the sorted list as in the original.
     *
     * @param stable whether to use stable sort
     * @return a list consisting of elements of this list sorted in
     *         ascending order, as determined by the `<` operator
     */
    List sorted(bool stable = false) const
    {
        return sorted([](const A& x, const A& y) { return x < y; }, stable);
    }

    /**
     * @brief Returns a list consisting of elements of this list sorted in
     *        ascending order, as determined by the given comparator.
     *
     * If `stable` is `true`, the sort will be stable. That is, equal elements
     * appear in the same order in the sorted list as in the original.
     *
     * @tparam Fn the type of the comparator
     * @param lt a comparator that returns `true` if its first argument
     *           is *less* than (i.e., is ordered *before*) the second
     * @param stable whether to use stable sort
     * @return a list consisting of elements of this list sorted in
     *         ascending order, as determined by the given comparator
     */
    template<typename Fn>
    List sorted(Fn lt, bool stable = false) const
    {
        std::vector<Ptr<A>> buf;
        buf.reserve(size());
        foreachImpl([&buf](const Ptr<A>& x) {
            buf.emplace_back(x);
        });

        auto comp = [&lt](const Ptr<A>& x, const Ptr<A>& y) {
            return lt(*x, *y);
        };
        if (stable) {
            std::stable_sort(buf.begin(), buf.end(), comp);
        } else {
            std::sort(buf.begin(), buf.end(), comp);
        }

        return toList(
                size(),
                std::make_move_iterator(buf.rbegin()),
                std::make_move_iterator(buf.rend()));
    }

    /**
     * @brief Returns a list resulting from wrapping the elements of this list
     *        in `std::reference_wrapper<const A>`s.
     *
     * @return a list resulting from wrapping the elements of this list
     *         in `std::reference_wrapper<const A>`s
     */
    List<std::reference_wrapper<const A>> cref() const
    {
        return map([](const A& x) { return std::cref(x); });
    }

    /**
     * @brief Returns a list formed from this list and `that` by combining
     *        corresponding elements in pairs.
     *
     * If one of the two lists is longer than the other, its remaining elements
     * are ignored.
     *
     * The list elements are copied into the pairs. To avoid copying, transform
     * the lists with `cref()` and zip the resulting lists instead.
     *
     * @tparam B the element type of `that`
     * @param that the list providing the second element of each result pair
     * @return a list formed from this list and `that` by combining
     *         corresponding elements in pairs
     */
    template<typename B>
    List<std::pair<A, B>> zip(const List<B>& that) const
    {
        using AB = std::pair<A, B>;

        std::size_t s = std::min(size(), that.size());
        std::vector<Ptr<AB>> buf;
        buf.reserve(s);

        auto n1 = node_.get();
        auto n2 = that.node_.get();
        for (; s > 0; n1 = n1->tail.get(), n2 = n2->tail.get(), --s) {
            buf.emplace_back(std::make_shared<AB>(*(n1->head), *(n2->head)));
        }

        return List<AB>::toList(
                buf.size(),
                std::make_move_iterator(buf.rbegin()),
                std::make_move_iterator(buf.rend()));
    }

    /**
     * @brief Returns a prefix scan over this list with the given start value
     *        and associative binary operator.
     *
     * @tparam A1 the result type of the binary operator, a supertype of `A`
     * @tparam Fn the type of the associative binary operator
     * @param z a neutral element for the prefix scan; may be added to
     *          the result an arbitrary number of times, and must not change
     *          the result (e.g., an empty list for list concatenation,
     *          0 for addition, or 1 for multiplication)
     * @param op a binary operator that must be associative
     * @return a prefix scan over this list with the given start value and
     *         associative binary operator
     */
    template<
        typename A1,
        typename Fn,
        typename = typename std::enable_if<
            std::is_same<A1, A>::value || std::is_base_of<A1, A>::value
        >::type
    >
    List<A1> scan(A1 z, Fn op) const
    {
        return scanLeft(std::move(z), std::move(op));
    }

    /**
     * @brief Returns a list consisting of the intermediate results of
     *        a left fold over this list with the given start value and
     *        binary operator.
     *
     * @tparam B the result type of the binary operator
     * @tparam Fn the type of the binary operator
     * @param z the start value
     * @param op the binary operator
     * @return a list consisting of the intermediate results of a left fold
     *         over this list with the given start value and binary operator
     */
    template<typename B, typename Fn>
    List<B> scanLeft(B z, Fn op) const
    {
        std::vector<Ptr<B>> acc;
        acc.reserve(size() + 1);
        acc.emplace_back(std::make_shared<B>(std::move(z)));
        foreachImpl([&acc, &op] (const Ptr<A>& x) {
            acc.emplace_back(std::make_shared<B>(op(*acc.back(), *x)));
        });

        return List<B>::toList(
                size() + 1,
                std::make_move_iterator(acc.rbegin()),
                std::make_move_iterator(acc.rend()));
    }

    /**
     * @brief Returns a list consisting of the intermediate results of
     *        a right fold over this list with the given start value and
     *        binary operator.
     *
     * @tparam B the result type of the binary operator
     * @tparam Fn the type of the binary operator
     * @param z the start value
     * @param op the binary operator
     * @return a list consisting of the intermediate results of a right fold
     *         over this list with the given start value and binary operator
     */
    template<typename B, typename Fn>
    List<B> scanRight(B z, Fn op) const
    {
        std::vector<const A*> buf;
        buf.reserve(size());
        foreachImpl([&buf](const Ptr<A>& x) {
            buf.emplace_back(x.get());
        });

        using BN = typename List<B>::Node;
        auto hd = BN::create(std::make_shared<B>(std::move(z)), BN::create());
        for (auto it = buf.crbegin(); it != buf.crend(); ++it) {
            auto ptr = std::make_shared<B>(op(**it, *hd->head));
            hd = BN::create(std::move(ptr), std::move(hd));
        }
        return List<B>(size() + 1, std::move(hd));
    }

    /**
     * @brief Applies the given associative binary operator to all elements of
     *        this list.
     *
     * The order in which operations are performed on elements is unspecified
     * and may be nondeterministic.
     *
     * @tparam A1 the result type of the binary operator, a supertype of `A`
     * @tparam Fn the type of the associative binary operator
     * @param op a binary operator that must be associative
     * @return the result of inserting `op` between all elements of this list
     * @throws std::out_of_range if this list is empty
     */
    template<
        typename Fn,
        typename A1 = Decay<Ret<Fn, A, A>>,
        typename = typename std::enable_if<
            std::is_same<A1, A>::value || std::is_base_of<A1, A>::value
        >::type
    >
    A1 reduce(Fn op) const
    {
        if (isEmpty()) {
            throw std::out_of_range("reduce on empty list");
        }
        return reduceLeft(std::move(op));
    }

    /**
     * @brief Applies a binary operator to all elements of this list,
     *        going left to right.
     *
     * @tparam Fn the type of the binary operator
     * @tparam A1 the result type of the binary operator, a supertype of `A`
     * @param op the binary operator
     * @return the result of inserting `op` between consecutive elements of
     *         this list, going left to right
     * @throws std::out_of_range if this list is empty
     */
    template<
        typename Fn,
        typename A1 = Decay<Ret<Fn, A, A>>,
        typename = typename std::enable_if<
            std::is_same<A1, A>::value || std::is_base_of<A1, A>::value
        >::type
    >
    A1 reduceLeft(Fn op) const
    {
        if (isEmpty()) {
            throw std::out_of_range("reduceLeft on empty list");
        }
        return tail().foldLeft(A1(head()), std::move(op));
    }

    /**
     * @brief Applies a binary operator to all elements of this list,
     *        going right to left.
     *
     * @tparam Fn the type of the binary operator
     * @tparam A1 the result type of the binary operator, a supertype of `A`
     * @param op the binary operator
     * @return the result of inserting `op` between consecutive elements of
     *         this list, going right to left
     * @throws std::out_of_range if this list is empty
     */
    template<
        typename Fn,
        typename A1 = Decay<Ret<Fn, A, A>>,
        typename = typename std::enable_if<
            std::is_same<A1, A>::value || std::is_base_of<A1, A>::value
        >::type
    >
    A1 reduceRight(Fn op) const
    {
        if (isEmpty()) {
            throw std::out_of_range("reduceRight on empty list");
        }

        std::vector<const A*> buf;
        buf.reserve(size());
        foreachImpl([&buf] (const Ptr<A>& x) {
            buf.emplace_back(x.get());
        });

        A1 acc = *buf.back();
        for (auto it = ++buf.crbegin(); it != buf.crend(); ++it) {
            acc = op(**it, std::move(acc));
        }
        return acc;
    }

    /**
     * @brief Returns the element at the specified position of this list.
     *
     * @param index index of the element to return
     * @return the element at the specified position of this list
     * @throws std::out_of_range if `index` is out of range (`index >= size()`)
     */
    const A& operator[](std::size_t index) const
    {
        if (index >= size()) {
            throw std::out_of_range("index out of range");
        }
        auto n = node_.get();
        for (; index > 0; n = n->tail.get(), --index) {}
        return *(n->head);
    }

    /**
     * @brief Compares this list with the given list for equality.
     *
     * @param that the list to be compared for equality with this list
     * @return `true` if `that` contains the same elements as this list
     *         in the same order, `false` otherwise
     */
    bool operator==(const List<A>& that) const
    {
        if (this == &that) {
            return true;
        }
        if (size() != that.size()) {
            return false;
        }
        for (auto n1 = node_.get(), n2 = that.node_.get();
                n1->head;
                n1 = n1->tail.get(), n2 = n2->tail.get()) {
            if (*(n1->head) != *(n2->head)) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Compares this list with the given list for inequality.
     *
     * @param that the list to be compared for inequality with this list
     * @return `true` if `that` does not contains the same elements as this list
     *         in the same order, `false` otherwise
     */
    bool operator!=(const List<A>& that) const
    {
        return !(*this == that);
    }

    class StdIterator;

    /**
     * @brief Returns an iterator to the first element of this list.
     *
     * If this list is empty, the returned iterator will be equal to `end()`.
     *
     * @return an iterator to the first element of this list.
     */
    StdIterator begin() const
    {
        return StdIterator(node_.get());
    }

    /**
     * @brief Returns an iterator to the element following the last element
     *        of this list.
     *
     * This element acts as a placeholder; attempting to access it results in
     * undefined behavior.
     *
     * @return an iterator to the element following the last element
     *         of this list
     */
    StdIterator end() const
    {
        static const auto nil = Node::create();
        return StdIterator(nil.get());
    }

private:
    template<typename>
    friend class List;

    template<typename T>
    using Ptr = std::shared_ptr<const T>;

    class Node;

    List(std::size_t size, Ptr<Node> node) noexcept
        : size_(size)
        , node_(std::move(node))
    {}

    template<typename Fn>
    void foreachImpl(Fn f) const
    {
        for (auto n = node_.get(); n->head; n = n->tail.get()) {
            f(n->head);
        }
    }

    template<typename ReverseIt>
    static List toList(
            std::size_t size,
            ReverseIt rbegin,
            ReverseIt rend,
            Ptr<Node> head = Node::create())
    {
        for (; rbegin != rend; ++rbegin) {
            head = Node::create(*rbegin, std::move(head));
        }
        return List(size, std::move(head));
    }

    std::size_t size_;
    Ptr<Node> node_;
};

/**
 * @brief A `ForwardIterator` for a `List`.
 *
 * @author Zizheng Tai
 * @since 1.0
 * @tparam A the element type of the list
 */
template<typename A>
class List<A>::StdIterator final
    : public std::iterator<std::forward_iterator_tag,
                           A,
                           std::ptrdiff_t,
                           const A*,
                           const A&> {
public:
    /** @brief Default copy constructor. */
    StdIterator(const StdIterator&) = default;

    /** @brief Default move constructor. */
    StdIterator(StdIterator&) = default;

    /** @brief Default copy assignment operator. */
    StdIterator& operator=(const StdIterator&) = default;

    /** @brief Default move assignment operator. */
    StdIterator& operator=(StdIterator&&) = default;

    /**
     * @brief Tests whether two iterators point to the same element.
     *
     * @return `true` if this iterator and `that` point to the same element,
     *         `false` otherwise
     */
    bool operator==(const StdIterator& that) const
    {
        return node_->head == that.node_->head;
    }

    /**
     * @brief Tests whether two iterators point to different element.
     *
     * @return `true` if this iterator and `that` point to different elements,
     *         `false` otherwise
     */
    bool operator!=(const StdIterator& that) const
    {
        return node_->head != that.node_->head;
    }

    /**
     * @brief Increments this iterator and returns a reference to it.
     *
     * @return a reference to this iterator
     */
    StdIterator& operator++()
    {
        node_ = node_->tail.get();
        return *this;
    }

    /**
     * @brief Increments this iterator and returns a copy of the original iterator.
     *
     * @return a copy of the original iterator
     */
    StdIterator operator++(int)
    {
        StdIterator it = *this;
        node_ = node_->tail.get();
        return it;
    }

    /**
     * @brief Returns a reference to the element this iterator points to.
     *
     * @return a reference to the element this iterator points to
     */
    const A& operator*() const
    {
        return *node_->head;
    }

    /**
     * @brief Returns a pointer to the element this iterator points to.
     *
     * @return a pointer to the element this iterator points to
     */
    const A* operator->() const
    {
        return node_->head.get();
    }

private:
    friend class List;

    explicit StdIterator(const Node* node) noexcept : node_(node) {}

    const Node* node_;
};

/// @cond GUNGNIR_PRIVATE
template<typename A>
class List<A>::Node final {
    class Priv final {};

public:
    static Ptr<Node> create()
    {
        return std::make_shared<const Node>(Priv());
    }

    static Ptr<Node> create(Ptr<A> head, Ptr<Node> tail)
    {
        return std::make_shared<const Node>(
                Priv(), std::move(head), std::move(tail));
    }

    Node(Priv) noexcept {}

    Node(Priv, Ptr<A> head, Ptr<Node> tail) noexcept
        : head(std::move(head))
        , tail(std::move(tail))
    {}

    Node(const Node&) = delete;
    Node(Node&&) = delete;

    Node& operator=(const Node&) = delete;
    Node& operator=(Node&&) = delete;

    const Ptr<A> head;
    const Ptr<Node> tail;
};
/// @endcond

}  // namespace gungnir

#endif  // GUNGNIR_LIST_HPP
