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

static_assert (c::widget<Line<0, void>>);
static_assert (c::widget<Check<decltype ([] {}), bool, std::string_view, void>>);
static_assert (c::widget<Radio<std::string_view, int, void>>);
static_assert (c::widget<Label<std::string_view>>);
static_assert (c::widget<Button<std::string_view, decltype ([] {})>>);
// These does not have the operator () and the height field

using MyLayout = Layout<detail::VBoxDecoration, decltype (std::tuple{check (true, ""sv)})>;
static_assert (!c::widget<MyLayout>);
static_assert (!c::widget<Group<decltype ([] {}), int, decltype (std::make_tuple (radio (1, ""sv)))>>);
static_assert (!c::widget<Window<0, 0, 0, 0, void, Label<std::string_view>>>);

static_assert (is_layout<MyLayout>::value);
static_assert (!is_layout<decltype (check (true, ""sv))>::value);
static_assert (!c::layout<Label<std::string_view>>);
static_assert (c::layout<MyLayout>);

static_assert (!c::group<int>);
static_assert (!c::group<Label<std::string_view>>);
static_assert (!c::group<MyLayout>);
static_assert (c::group<Group<decltype ([] {}), int, decltype (std::make_tuple (radio (1, ""sv)))>>);

static_assert (c::window<Window<0, 0, 0, 0, void, Label<std::string_view>>>);
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
static_assert (detail::augment::window_wrapper<detail::augment::Window<og::Window<0, 0, 0, 0, void, int>, int>>);
static_assert (!detail::augment::window_wrapper<detail::augment::Group<int, std::tuple<int>, 0, 0, 0, 0, DefaultDecor<int>>>);

static_assert (!detail::augment::group_wrapper<detail::augment::Widget<Label<std::string_view>, 0, 0, 0, 0>>);
static_assert (!detail::augment::group_wrapper<detail::augment::Layout<MyLayout, std::tuple<int>, 0, 0>>);
static_assert (!detail::augment::group_wrapper<detail::augment::Window<int, int>>);
static_assert (detail::augment::group_wrapper<detail::augment::Group<int, std::tuple<int>, 0, 0, 0, 0, DefaultDecor<int>>>);

TEST_CASE ("Pow function", "[detail]")
{

        CHECK (og::detail::pow (10, 0) == 1);
        CHECK (og::detail::pow (10, 1) == 10);
        CHECK (og::detail::pow (10, 2) == 100);
        CHECK (og::detail::pow (10, 3) == 1000);
        CHECK (og::detail::pow (10, 4) == 10000);

        static_assert (og::detail::pow (10, 0) == 1);
        static_assert (og::detail::pow (10, 1) == 10);
        static_assert (og::detail::pow (10, 2) == 100);
        static_assert (og::detail::pow (10, 3) == 1000);
        static_assert (og::detail::pow (10, 4) == 10000);
}

TEST_CASE ("Numeric to string conversion", "[detail]")
{
        using namespace std::string_view_literals;

        std::array<char, 32> buf;

        SECTION ("Integers")
        {
                CHECK (detail::itoa (0U, buf) == 1);
                CHECK (std::string_view{buf.begin (), 1} == "0"sv);

                CHECK (detail::itoa (42U, buf) == 2);
                CHECK (std::string_view{buf.begin (), 2} == "42"sv);
        }

        SECTION ("Integers padding")
        {
                CHECK (detail::itoa (0U, buf, 3) == 3);
                CHECK (std::string_view{buf.begin (), 3} == "000"sv);

                CHECK (detail::itoa (10U, buf, 3) == 3);
                CHECK (std::string_view{buf.begin (), 3} == "010"sv);

                CHECK (detail::itoa (10U, buf, 5) == 5);
                CHECK (std::string_view{buf.begin (), 5} == "00010"sv);
        }

        SECTION ("Signed integers")
        {
                CHECK (detail::itoa (0, buf) == 1);
                CHECK (std::string_view{buf.begin (), 1} == "0"sv);

                CHECK (detail::itoa (-1, buf) == 2);
                CHECK (std::string_view{buf.begin (), 2} == "-1"sv);

                CHECK (detail::itoa (-42, buf) == 3);
                CHECK (std::string_view{buf.begin (), 3} == "-42"sv);

                CHECK (detail::itoa (-30000, buf) == 6);
                CHECK (std::string_view{buf.begin (), 6} == "-30000"sv);
        }

        SECTION ("Signed integers padding")
        {
                CHECK (detail::itoa (0, buf, 3) == 3);
                CHECK (std::string_view{buf.begin (), 3} == "000"sv);

                CHECK (detail::itoa (-1, buf, 3) == 4);
                CHECK (std::string_view{buf.begin (), 4} == "-001"sv);

                CHECK (detail::itoa (-42, buf, 4) == 5);
                CHECK (std::string_view{buf.begin (), 5} == "-0042"sv);

                CHECK (detail::itoa (-30000, buf, 6) == 7);
                CHECK (std::string_view{buf.begin (), 7} == "-030000"sv);
        }

        SECTION ("Floats")
        {
                CHECK (detail::ftoa (42.1F, buf, 1) == 4);
                CHECK (std::string_view{buf.begin (), 4} == "42.1"sv);

                CHECK (detail::ftoa (0.0F, buf, 1) == 3);
                CHECK (std::string_view{buf.begin (), 3} == "0.0"sv);

                CHECK (detail::ftoa (1.0F, buf, 3) == 5);
                CHECK (std::string_view{buf.begin (), 5} == "1.000"sv);

                CHECK (detail::ftoa (1.0F, buf, 0) == 1);
                CHECK (std::string_view{buf.begin (), 1} == "1"sv);

                CHECK (detail::ftoa (42.4242, buf, 4) == 7);
                CHECK (std::string_view{buf.begin (), 7} == "42.4242"sv);
        }

        SECTION ("Rounded floats")
        {
                CHECK (detail::ftoa (3.9999F, buf, 1) == 3);
                CHECK (std::string_view{buf.begin (), 3} == "4.0"sv);

                CHECK (detail::ftoa (3.9999F, buf, 2) == 4);
                CHECK (std::string_view{buf.begin (), 4} == "4.00"sv);

                CHECK (detail::ftoa (3.9999F, buf, 3) == 5);
                CHECK (std::string_view{buf.begin (), 5} == "4.000"sv);
        }

        // auto r = radio (0, std::string_view{""});
        // EmptyDisplay d{};
        // Context const c{};
        // r.operator()<int> (d, c);
}

static_assert (og::detail::IntStrLen<uint8_t>::value == 4);   // 255'\0'
static_assert (og::detail::IntStrLen<int8_t>::value == 5);    // -128'\0'
static_assert (og::detail::IntStrLen<uint16_t>::value == 6);  // 65536'\0'
static_assert (og::detail::IntStrLen<int16_t>::value == 7);   // -32768'\0'
static_assert (og::detail::IntStrLen<uint32_t>::value == 11); // 4294967296'\0'
static_assert (og::detail::IntStrLen<int32_t>::value == 12);  // -2147483648'\0'
