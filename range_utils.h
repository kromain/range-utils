#pragma once

#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility>

template<typename C>
struct reversible_range_iterator {
    using NoRefC = typename std::remove_reference<C>::type;

    reversible_range_iterator(C&& container, bool iterateBackward = true) : m_container(std::forward<C>(container)), m_iterateBackward(iterateBackward) {}

    /**
     * @brief This is a proxy for forward/backward iterators that satisfies the requirements of range-for loops (basically just operators *,++ and !=)
     */
    template<typename ForwardIterator, typename BackwardIterator>
    struct iterator_proxy {
        // Default implementation, where ForwardIterator is a standard iterator with a ::reference typedef
        template<typename _It = ForwardIterator>
        typename _It::reference operator*() { return m_isReverse ? *m_bwdIt : *m_fwdIt; }
        // Partial specialization for cases like QByteArray, where ForwardIterator is a plain pointer and has no ::reference or ::value_type typedefs
        // Fallback to the ::value_type on the container itself in that case
        template<typename _It = ForwardIterator, typename = std::enable_if_t<std::is_pointer<_It>::value>>
        typename NoRefC::value_type operator*() { return m_isReverse ? *m_bwdIt : *m_fwdIt; }

        auto& operator++() { if (m_isReverse) ++m_bwdIt; else ++m_fwdIt; return *this; }

        friend bool operator!=(const iterator_proxy& lhs, const iterator_proxy& rhs) { return lhs.m_isReverse ? lhs.m_bwdIt != rhs.m_bwdIt : lhs.m_fwdIt != rhs.m_fwdIt; }

        ForwardIterator base() { return m_isReverse ? m_bwdIt.base() : m_fwdIt; }

        ForwardIterator m_fwdIt;
        BackwardIterator m_bwdIt;
        bool m_isReverse;
    };

    using cit = typename NoRefC::const_iterator;
    using crit = typename NoRefC::const_reverse_iterator;
    using it = typename NoRefC::iterator;
    using rit = typename NoRefC::reverse_iterator;

    // Default implementation for the const_iterator case
    auto begin() const { return iterator_proxy<cit, crit>{m_container.cbegin(), m_container.crbegin(), m_iterateBackward}; }
    auto end() const { return iterator_proxy<cit, crit>{m_container.cend(), m_container.crend(), m_iterateBackward}; }

    // These non-const overloads only make sense with non-const lvalues, so they must be conditionally compiled
    template<typename _C = C, typename = std::enable_if_t<std::is_lvalue_reference<_C>::value && !std::is_const<NoRefC>::value>>
    auto begin() { return iterator_proxy<it, rit>{m_container.begin(), m_container.rbegin(), m_iterateBackward}; }
    template<typename _C = C, typename = std::enable_if_t<std::is_lvalue_reference<_C>::value && !std::is_const<NoRefC>::value>>
    auto end() { return iterator_proxy<it, rit>{m_container.end(), m_container.rend(), m_iterateBackward}; }

private:
    // This will expand to `[const] C&` for lvalues and `const C` for rvalues (ie. the temporary lifetime gets extended)
    // See https://en.cppreference.com/w/cpp/language/template_argument_deduction#Deduction_from_a_function_call (list item 4)
    // and https://en.cppreference.com/w/cpp/language/reference#Forwarding_references for details about this behavior
    const C m_container;
    bool m_iterateBackward;
};

/**
 * @brief This helper allows iterating backwards over any container that supports begin()/end() and rbegin()/rend() within a range-for loop.
 *
 * The extra boolean parameter allows toggling forward/backward iteration at runtime with a single for-loop body.
 *
 * Usage example:
 *
 * @code{.cpp}
 * const QVector<int> values = {0, 1, 2, 3};
 * bool revert = true;
 * for (int&& value : make_reversible(values, revert)) {
 *     qDebug() << value;
 * }
 * // will print:
 * // 3
 * // 2
 * // 1
 * // 0
 *
 * revert = false;
 * for (int&& value : make_reversible(values, revert)) {
 *     qDebug() << value;
 * }
 * // will print:
 * // 0
 * // 1
 * // 2
 * // 3
 * @endcode
 *
 */
template<typename C>
auto make_reversible(C&& container, bool iterateBackward = true) { return reversible_range_iterator<C>(std::forward<C>(container), iterateBackward); }

/**
 * @brief This overload provides default non-mutating backwards iteration of a non-const container within a range-for loop.
 *
 * This is an overload for the general make_reversible helper that converts non-const lvalue references to const ones,
 * therefore defaulting to non-mutating iteration over the container without the need to use qAsConst().
 *
 * Use make_mutable_reversible instead if you want a mutating iteration for such containers.
 */
template<typename C>
auto make_reversible(C& container, bool iterateBackward = true) { return reversible_range_iterator<const C&>(container, iterateBackward); }

/**
 * @brief This helper provides explicit mutating backwards iteration of a non-const container within a range-for loop.
 *
 * Having a separate helper makes it explicit that the iteration allows the values to be modified, which can result in a deep-copy
 * of the container data and is generally more costly due to the copy-on-write semantics. Use with care and only when necessary.
 *
 * Use make_reversible instead if you only want non-mutating (ie. read-only) iteration for your container.
 *
 * @note Mutating iterations on rvalues isn't supported, since modifying temporaries is generally not intended.
 */
template<typename C>
auto make_mutable_reversible(C& container, bool iterateBackward = true) { return reversible_range_iterator<C&>(container, iterateBackward); }


// c++11 equivalent of std::apply() (c++17 feature, but not supported by the older stdlib on Ubuntu 16.04)
// Nb: We can't use universal references for tuple since templated lvalues are not supported in c++11 (cf. http://www.preney.ca/paul/archives/689)
// Alternatively, we can template std::tuple completely, as in http://stackoverflow.com/a/26912970 (more flexible, but a bit less readable as well)

template<typename Func, typename...Ts, std::size_t...Is>
void for_each_in_tuple_impl(std::tuple<Ts...>& tuple, Func&& f, std::index_sequence<Is...>){
    (void) std::initializer_list<int>{ ((void)f(std::get<Is>(tuple)), 0)... }; // guarantees left to right order execution
}
template<typename Func, typename...Ts>
void for_each_in_tuple(std::tuple<Ts...>& tuple, Func&& func){
    for_each_in_tuple_impl(tuple, std::forward<Func>(func), std::make_index_sequence<sizeof...(Ts)>());
}

template<typename Func, typename...Ts, std::size_t...Is>
auto transform_tuple_impl(const std::tuple<Ts...>& tuple, Func&& f, std::index_sequence<Is...>) -> std::tuple<decltype(f(std::declval<Ts>()))...> {
    return std::make_tuple(f(std::get<Is>(tuple))...);
}
template<typename Func, typename...Ts>
auto transform_tuple(const std::tuple<Ts...>& tuple, Func&& f) -> std::tuple<decltype(f(std::declval<Ts>()))...> {
    return transform_tuple_impl(tuple, std::forward<Func>(f), std::make_index_sequence<sizeof...(Ts)>());
}

template <typename...Containers>
struct synchronized_range_iterator {
    synchronized_range_iterator(const Containers&... containers) : m_containers(containers...) {}

    /**
     * @brief This is a wrapper for forward/backward iterators that satisfies the requirements of range-for loops (basically just operators *,++ and !=)
     */
    struct const_iterator {
        typename std::tuple<typename Containers::value_type...> operator*() const { return transform_tuple(m_iterators, [](const auto& it) { return *it; }); }
        const_iterator& operator++() { for_each_in_tuple(m_iterators, [](auto& it) { return ++it; }); return *this; }

        // Implement any-of for tuple equality, instead of the default all-of implemented by std::tuple
        // This allows stopping when any iterator has reached end(), to support collections with different sizes
        template<std::size_t Cur, std::size_t Max, typename It>
        struct iterator_tuple_compare {
            static constexpr bool compare(const It& lhs, const It& rhs) {
                return std::get<Cur>(lhs) == std::get<Cur>(rhs) || iterator_tuple_compare<Cur + 1, Max, It>::compare(lhs, rhs);
            }
        };
        template<std::size_t Max, typename It>
        struct iterator_tuple_compare<Max, Max, It> {
            static constexpr bool compare(const It&, const It&) { return false; }
        };

        friend bool operator!=(const const_iterator& lhs, const const_iterator& rhs) { return !iterator_tuple_compare<0, std::tuple_size<decltype(m_iterators)>::value, decltype(m_iterators)>::compare(lhs.m_iterators, rhs.m_iterators); }

        std::tuple<typename Containers::const_iterator...> m_iterators;
    };

    const_iterator begin() const { return {transform_tuple(m_containers, [](const auto& it) { return it.begin(); }) }; }
    const_iterator end() const { return {transform_tuple(m_containers, [](const auto& it) { return it.end(); }) }; }

private:
    std::tuple<Containers...> m_containers;
};

/**
 * @brief This helper allows iterating over any number of containers that support begin()/end() at once within a range-for loop.
 *
 * The range iterator returned by this helper returns a std::tuple with the current values for each container,
 * which allows extracting the values as structured bindings with c++17, and it works for any number of containers at the same time.
 *
 * If the containers do not have the same element count (ie. don't take the same number of iterations to go from begin() to end()),
 * then iteration stops when any of the iterators reaches end().
 *
 * Usage example:
 *
 * @code{.cpp}
 * const QVector<int> values = {0, 1, 2, 3, 4, 5};
 * const QStringList labels = {"0", "1", "2", "3"};
 *
 * for(auto&& [value, label] : make_synchronized(values, labels)) {
 *     qDebug() << value << "->" label;
 * }
 * // will print:
 * // 0 -> "0"
 * // 1 -> "1"
 * // 2 -> "2"
 * // 3 -> "3"
 * @endcode
 *
 */
template <typename...Containers>
struct synchronized_range_iterator<Containers...> make_synchronized(const Containers&... containers) { return synchronized_range_iterator<Containers...>(containers...); }


template<typename C>
struct key_value_range_iterator {
    key_value_range_iterator(C&& container) : m_container(std::forward<C>(container))  {}

    auto begin() const { return m_container.keyValueBegin(); }
    auto end() const { return m_container.keyValueEnd(); }

private:
    // This will expand to `[const] C&` for lvalues and `const C` for rvalues (ie. the temporary lifetime gets extended)
    // See https://en.cppreference.com/w/cpp/language/template_argument_deduction#Deduction_from_a_function_call (list item 4)
    // and https://en.cppreference.com/w/cpp/language/reference#Forwarding_references for details about this behavior
    const C m_container;
};

/**
 * @brief This helper allows iterating over both keys and values of a QMap or QHash container within a range-for loop.
 *
 * The range iterator returned by this helper returns a std::pair<key,value> for each element in the container,
 * which allows extracting both key and value together as structured bindings with c++17.
 *
 * The helper is non-mutating, so both the container and the key-value pairs are handled as const-references.
 * This means wrapping the container with qAsConst is not required when using this helper, even for non-const lvalues.
 *
 * Passing temporary objects is also supported, with the lifetime of the temporary automatically extended
 * to the end of the iteration.
 *
 * Usage example:
 *
 * @code{.cpp}
 * const QMap<int, QString> digitMap = { {1, "one"}, {2, "two"}, {3, "three"} };
 * for (auto [intKey, strVal] : make_keyval(digitMap)) {
 *     qDebug() << intKey << "->" << strVal;
 * }
 * // will print:
 * // 1 -> "one"
 * // 2 -> "two"
 * // 3 -> "three"
 *
 * const auto json = QJsonDocument::fromJson("{ '1': 'one', '2': 'two', '3': 'three' }");
 * for (auto [propKey, propVal] : make_keyval(json.toObject().toVariantMap())) {
 *     qDebug() << propKey << "->" << propVal.toString();
 * }
 * // will print:
 * // "1" -> "one"
 * // "2" -> "two"
 * // "3" -> "three"
 * @endcode
 *
 */
template<typename C>
auto make_keyval(C&& container) { return key_value_range_iterator<C>(std::forward<C>(container)); }

/**
 * @brief This overload provides default non-mutating iteration over both keys and values of a non-const container within a range-for loop.
 *
 * This is an overload for the general make_keyval helper that converts non-const lvalue references to const ones,
 * therefore defaulting to non-mutating iteration over the container without the need to use qAsConst().
 *
 * Use make_mutable_keyval instead if you want a mutating iteration for such containers.
 */
template<typename C>
auto make_keyval(C& container) { return key_value_range_iterator<const C&>(container); }

/**
 * @brief This helper provides explicit mutating iteration over both keys and values of a non-const container within a range-for loop.
 *
 * Having a separate helper makes it explicit that the iteration allows the values to be modified, which can result in a deep-copy
 * of the container data and is generally more costly due to the copy-on-write semantics. Use with care and only when necessary.
 *
 * Use make_keyval instead if you only want non-mutating (ie. read-only) iteration for your container.
 *
 * @note Mutating iterations on rvalues isn't supported, since modifying temporaries is generally not intended.
 */
template<typename C>
auto make_mutable_keyval(C& container) { return key_value_range_iterator<C&>(container); }
