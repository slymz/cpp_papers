#ifndef LIBCPP__RANGE_CONCAT_HPP
#define LIBCPP__RANGE_CONCAT_HPP

#include <concepts>
#include <functional>
#include <ranges>
#include <tuple>
#include <variant>

#include "utils.hpp"

namespace std::ranges {

namespace xo { // exposition only things (and persevering face)

// these concepts are needed if we make the implementation same as range-v3.
// for random access, range-v3 actually isn't constant time.
// For it + n, if n is bigger than the current view, it recursively
// goes to the end and checks the next view, which is O(N), where N
// is number of concated view.
// we can possibly use ranges::size if they are sized_range
template <bool Const, class... Views>
concept all_random_access = // exposition only
    (random_access_range<__maybe_const<Const, Views>>&&...);

template <bool Const, class... Views>
concept all_bidirectional = // exposition only
    (bidirectional_range<__maybe_const<Const, Views>>&&...);

template <bool Const, class... Views>
concept all_forward = // exposition only
    (forward_range<__maybe_const<Const, Views>>&&...);

template <bool Const, typename... Views>
constexpr auto iterator_concept_test() {
    // [TODO] if there is only one View and it has contiguous iterators, we should be one too. or
    // perhaps, we
    //        should simply views::concat(r) as views::all(r)?
    if constexpr (all_random_access<Const, Views...>) {
        return random_access_iterator_tag{};
    } else if constexpr (all_bidirectional<Const, Views...>) {
        return bidirectional_iterator_tag{};
    } else if constexpr (all_forward<Const, Views...>) {
        return forward_iterator_tag{};
    } else {
        return input_iterator_tag{};
    }
}

template <typename... T>
using back = tuple_element_t<sizeof...(T) - 1, tuple<T...>>;

} // namespace xo

template <input_range... Views>
requires(view<Views>&&...) && (sizeof...(Views) > 0) class concat_view
    : public view_interface<concat_view<Views...>> {

    tuple<Views...> views_; // exposition only

    template <bool Const>
    class iterator {
        // use of exposition only trait `maybe-const` defined in
        // http://eel.is/c++draft/ranges#syn
        using ParentView = __maybe_const<Const, concat_view>;
        using BaseIt = variant<iterator_t<__maybe_const<Const, Views>>...>;


        // [TODO] range-v3 has pointed out that rvalue_reference is a problem
        using common_ref = common_reference_t<range_reference_t<__maybe_const<Const, Views>>...>;

        ParentView* parent_ = nullptr;
        BaseIt it_ = BaseIt();

        friend class iterator<!Const>;
        friend class concat_view;

        template <std::size_t N>
        void satisfy() {
            if constexpr (N != (sizeof...(Views) - 1)) {
                if (get<N>(it_) == ranges::end(get<N>(parent_->views_))) {
                    it_.template emplace<N + 1>(ranges::begin(get<N + 1>(parent_->views_)));
                    satisfy<N + 1>();
                }
            }
        }

      public:
        using difference_type = common_type_t<range_difference_t<__maybe_const<Const, Views>>...>;
        using value_type = common_type_t<range_value_t<__maybe_const<Const, Views>>...>;
        using iterator_concept = decltype(xo::iterator_concept_test<Const, Views...>());

        /* [TODO]
         * this one is tricky.
         * it depends on the iterate_category of every base and also depends on if
         * the common_reference_t<...> is actually a reference
         */
        // using iterator_category = ; // not always present.

        iterator() requires(default_initializable<iterator_t<__maybe_const<Const, Views>>>&&...) =
            default;

        template <class... Args>
        explicit iterator(ParentView* parent,
                          Args&&... args) requires constructible_from<BaseIt, Args&&...>
            : parent_{parent}, it_{static_cast<Args&&>(args)...} {}

        constexpr iterator(iterator<!Const> i) requires Const &&
            (convertible_to<iterator_t<Views>, iterator_t<__maybe_const<Const, Views>>>&&...)
            // [TODO] noexcept specs?
            : parent_{i.parent_}
            , it_{std::move(i.it_)} {}

        constexpr common_ref operator*() const {
            return visit([](auto&& it) -> common_ref { return *it; }, it_);
        }

        constexpr iterator& operator++() {
            // TODO: implement this function
            //  range-v3 variant has visit_i where the visitor has I at compile time
            return *this;
        }

        constexpr void operator++(int) { ++*this; }
        constexpr iterator operator++(int) requires xo::all_forward<Const, Views...> {
            auto tmp = *this;
            ++*this;
            return tmp;
        }
    };


    template <bool Const>
    class sentinel {
        friend class iterator<Const>;
        friend class sentinel<!Const>;

        using LastSentinel = sentinel_t<__maybe_const<Const, xo::back<Views...>>>;
        LastSentinel last_ = LastSentinel();

      public:
        sentinel() requires(default_initializable<LastSentinel>) = default;

        constexpr explicit sentinel(LastSentinel s)
            : last_{std::move(s)} {}

        // what is this for?
        constexpr sentinel(sentinel<!Const> s) requires Const &&
            (convertible_to<sentinel_t<xo::back<Views...>>, LastSentinel>)
            : last_{std::move(s.last_)} {}
    };

  public:
    concat_view() requires(default_initializable<Views>&&...) = default;
    constexpr explicit concat_view(Views... views)
        : views_{static_cast<Views&&>(views)...} {}

    // used exposition only concepts simple-view defined here:
    // http://eel.is/c++draft/ranges#range.utility.helpers (we can reuse in the spec)
    constexpr auto begin() requires(!(__simple_view<Views> && ...)) {
        iterator<false> it{this, in_place_index<0u>, ranges::begin(get<0>(views_))};
        it.template satisfy<0>();
        return it;
        // O(1) as sizeof...(Views) known at compile time
    }

    constexpr auto begin() const requires(range<const Views>&&...) {
        iterator<true> it{this, in_place_index<0u>, ranges::begin(get<0>(views_))};
        it.template satisfy<0>();
        return it;
    }

    constexpr auto end() requires(!(__simple_view<Views> && ...)) {
        if constexpr (common_range<xo::back<Views...>>) {
            constexpr auto N = sizeof...(Views);
            return iterator<false>{this, in_place_index<N - 1>, ranges::end(get<N - 1>(views_))};
        } else {
            return sentinel<false>{ranges::end(get<sizeof...(Views) - 1>(views_))};
        }
    }

    constexpr auto end() const requires(range<const Views>&&...) {
        if constexpr (common_range<xo::back<const Views...>>) {
            constexpr auto N = sizeof...(Views);
            return iterator<true>{this, in_place_index<N - 1>, ranges::end(get<N - 1>(views_))};
        } else {
            return sentinel<true>{ranges::end(get<sizeof...(Views) - 1>(views_))};
        }
    }

    constexpr auto size() requires(sized_range<Views>&&...) {
        return apply(
            [](auto... sizes) {
                using CT = make_unsigned_t<common_type_t<decltype(sizes)...>>;
                return (CT{0} + ... + CT{sizes});
            },
            concat_detail::tuple_transform(ranges::size, views_));
    }

    constexpr auto size() const requires(sized_range<const Views>&&...) {
        return apply(
            [](auto... sizes) {
                using CT = make_unsigned_t<common_type_t<decltype(sizes)...>>;
                return (CT{0} + ... + CT{sizes});
            },
            concat_detail::tuple_transform(ranges::size, views_));
    }
};

template <class... R>
concat_view(R&&...) -> concat_view<views::all_t<R>...>;

} // namespace std::ranges


#endif