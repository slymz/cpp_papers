#include "concat.hpp"

#include <concepts>
#include <type_traits>
#include <iterator>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#define TEST_POINT(x) TEST_CASE(x, "[itermoveswap]")


#include <range/v3/view/zip.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/stride.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/iota.hpp>
#include <functional>



TEST_POINT("iter_move") {
    std::vector v1{10, 11};
    std::vector v2{20, 21};
    std::array a1{-10, -11};
    std::array a2{-20, -21};

    auto zv12 = ranges::views::zip(v1, v2);
    auto za12 = ranges::views::zip(a1, a2);

    auto c = std::views::concat(zv12, za12);

    auto i1 = c.begin();
    auto i2 = i1 + v1.size();

    // auto vi1 = iter_move(i1); // pop-quiz: why is this wrong?
    std::ranges::range_value_t<decltype(c)> vi1 = iter_move(i1);
    *i1 = iter_move(i2);
    *i2 = std::move(vi1);

    CHECK(v1[0] == -10);
    CHECK(v2[0] == -20);
    CHECK(a1[0] == 10);
    CHECK(a2[0] == 20);
}


TEST_POINT("smallsort") {
    std::vector v1{10, 15};
    std::vector v2{3, 4};
    std::array a1{9, 15};
    std::array a2{0, 3};

    std::vector<std::pair<int, int>> expected{
        {9, 0},
        {10, 3},
        {15, 3},
        {15, 4},
    };

    auto concat = std::views::concat(ranges::views::zip(v1, v2), ranges::views::zip(a1, a2));

    std::ranges::sort(concat);

    CHECK_THAT(concat | ranges::to_vector, Catch::Matchers::Equals(expected));

    namespace rv = ranges::views;
    auto v1_expected = expected | rv::take(2) | rv::keys | ranges::to_vector;
    auto v2_expected = expected | rv::take(2) | rv::values | ranges::to_vector;
    auto a1_expected = expected | rv::drop(2) | rv::keys | ranges::to_vector;
    auto a2_expected = expected | rv::drop(2) | rv::values | ranges::to_vector;

    CHECK(v1 == v1_expected);
    CHECK(v2 == v2_expected);
    CHECK((a1 | ranges::to_vector) == a1_expected);
    CHECK((a2 | ranges::to_vector) == a2_expected);
}



TEST_POINT("largesort") {
    constexpr int largeN = 50;
    namespace rv = ranges::views;
    auto neg = rv::transform(std::negate{});
    auto v1 = rv::iota(0) | rv::stride(2) | rv::take(largeN) | ranges::to_vector; // 0,2,4,..
    auto v2 = v1 | neg | ranges::to_vector;                                       // 0,-2,-4,...

    auto vp = rv::zip(rv::iota(1) | rv::stride(2) | rv::take(largeN),
                      rv::iota(1) | rv::stride(2) | neg) |
              ranges::to_vector; // (1,-1), (3,-3), ...

    auto concat = std::views::concat(rv::zip(v1, v2), vp);

    std::ranges::sort(concat);

    auto v1_expected = rv::iota(0) | rv::take(largeN) | ranges::to_vector;       // 0,1,2, ...
    auto v2_expected = rv::iota(0) | neg | rv::take(largeN) | ranges::to_vector; // 0,-1,-2, ...
    auto vp_expected =
        rv::zip(rv::iota(largeN), rv::iota(largeN) | neg) | rv::take(largeN) | ranges::to_vector;
    // (50,-50), (51,-51), ...

    CHECK(v1 == v1_expected);
    CHECK(v2 == v2_expected);
    CHECK(vp == vp_expected);
}