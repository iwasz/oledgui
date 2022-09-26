/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "oledgui/oledgui.h"
#include <array>
#include <catch2/catch.hpp>
#include <string_view>

using namespace og;
using namespace std::string_view_literals;

static_assert (c::widget<Line<0, '-'>>);
static_assert (c::widget<Check<decltype ([] {}), bool, std::string_view>>);
static_assert (c::widget<Radio<std::string_view, int>>);
static_assert (c::widget<Label<std::string_view>>);
static_assert (c::widget<Button<std::string_view, decltype ([] {})>>);
// These does not have the operator () and the height field

using MyLayout = Layout<detail::VBoxDecoration, decltype (std::tuple{check (true, ""sv)})>;
static_assert (!c::widget<MyLayout>);
static_assert (!c::widget<Group<decltype ([] {}), int, decltype (std::make_tuple (radio (1, ""sv)))>>);
static_assert (!c::widget<Window<0, 0, 0, 0, false, Label<std::string_view>>>);

static_assert (is_layout<MyLayout>::value);
static_assert (!is_layout<decltype (check (true, ""sv))>::value);
static_assert (!c::layout<Label<std::string_view>>);
static_assert (c::layout<MyLayout>);

static_assert (!c::group<int>);
static_assert (!c::group<Label<std::string_view>>);
static_assert (!c::group<MyLayout>);
static_assert (c::group<Group<decltype ([] {}), int, decltype (std::make_tuple (radio (1, ""sv)))>>);

static_assert (c::window<Window<0, 0, 0, 0, false, Label<std::string_view>>>);
static_assert (!c::window<Label<std::string_view>>);

static_assert (detail::augment::widget_wrapper<detail::augment::Widget<Label<std::string_view>, 0, 0, 0, 0>>);
static_assert (!detail::augment::widget_wrapper<detail::augment::Layout<MyLayout, std::tuple<int>, 0, 0>>);
static_assert (!detail::augment::widget_wrapper<detail::augment::Window<int, int>>);
static_assert (!detail::augment::widget_wrapper<detail::augment::Group<int, std::tuple<int>, 0, 0, 0, 0, DefaultDecor<int>>>);

static_assert (!detail::augment::layout_wrapper<detail::augment::Widget<Label<std::string_view>, 0, 0, 0, 0>>);
static_assert (detail::augment::layout_wrapper<detail::augment::Layout<MyLayout, std::tuple<int>, 0, 0>>);
static_assert (!detail::augment::layout_wrapper<detail::augment::Window<int, int>>);
static_assert (!detail::augment::layout_wrapper<detail::augment::Group<int, std::tuple<int>, 0, 0, 0, 0, DefaultDecor<int>>>);

static_assert (!detail::augment::window_wrapper<detail::augment::Widget<Label<std::string_view>, 0, 0, 0, 0>>);
static_assert (!detail::augment::window_wrapper<detail::augment::Layout<MyLayout, std::tuple<int>, 0, 0>>);
static_assert (detail::augment::window_wrapper<detail::augment::Window<og::Window<0, 0, 0, 0, false, int>, int>>);
static_assert (!detail::augment::window_wrapper<detail::augment::Group<int, std::tuple<int>, 0, 0, 0, 0, DefaultDecor<int>>>);

static_assert (!detail::augment::group_wrapper<detail::augment::Widget<Label<std::string_view>, 0, 0, 0, 0>>);
static_assert (!detail::augment::group_wrapper<detail::augment::Layout<MyLayout, std::tuple<int>, 0, 0>>);
static_assert (!detail::augment::group_wrapper<detail::augment::Window<int, int>>);
static_assert (detail::augment::group_wrapper<detail::augment::Group<int, std::tuple<int>, 0, 0, 0, 0, DefaultDecor<int>>>);

TEST_CASE ("Numeric to string conversion", "[detail]")
{
        using namespace std::string_view_literals;

        std::array<char, 32> buf;

        SECTION ("Integers")
        {
                REQUIRE (detail::itoa (0U, buf) == 1);
                REQUIRE (std::string_view{buf.begin (), 1} == "0"sv);

                REQUIRE (detail::itoa (42U, buf) == 2);
                REQUIRE (std::string_view{buf.begin (), 2} == "42"sv);
        }

        SECTION ("Integers padding")
        {
                REQUIRE (detail::itoa (0U, buf, 3) == 3);
                REQUIRE (std::string_view{buf.begin (), 3} == "000"sv);

                REQUIRE (detail::itoa (10U, buf, 3) == 3);
                REQUIRE (std::string_view{buf.begin (), 3} == "010"sv);

                REQUIRE (detail::itoa (10U, buf, 5) == 5);
                REQUIRE (std::string_view{buf.begin (), 5} == "00010"sv);
        }

        SECTION ("Signed integers")
        {
                REQUIRE (detail::itoa (0, buf) == 1);
                REQUIRE (std::string_view{buf.begin (), 1} == "0"sv);

                REQUIRE (detail::itoa (-1, buf) == 2);
                REQUIRE (std::string_view{buf.begin (), 2} == "-1"sv);

                REQUIRE (detail::itoa (-42, buf) == 3);
                REQUIRE (std::string_view{buf.begin (), 3} == "-42"sv);

                REQUIRE (detail::itoa (-30000, buf) == 6);
                REQUIRE (std::string_view{buf.begin (), 6} == "-30000"sv);
        }

        SECTION ("Signed integers padding")
        {
                REQUIRE (detail::itoa (0, buf, 3) == 3);
                REQUIRE (std::string_view{buf.begin (), 3} == "000"sv);

                REQUIRE (detail::itoa (-1, buf, 3) == 4);
                REQUIRE (std::string_view{buf.begin (), 4} == "-001"sv);

                REQUIRE (detail::itoa (-42, buf, 4) == 5);
                REQUIRE (std::string_view{buf.begin (), 5} == "-0042"sv);

                REQUIRE (detail::itoa (-30000, buf, 6) == 7);
                REQUIRE (std::string_view{buf.begin (), 7} == "-030000"sv);
        }

        SECTION ("Floats")
        {
                detail::ftoa (42.1F, buf, 1);
                REQUIRE (std::string_view{buf.begin (), 4} == "42.1"sv);

                detail::ftoa (42.4242, buf, 4);
                REQUIRE (std::string_view{buf.begin (), 7} == "42.4242"sv);
        }
}