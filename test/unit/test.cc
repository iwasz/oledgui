/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "oledgui/ncurses.h"
#include <string_view>

using namespace og;
using namespace std::string_view_literals;

static_assert (c::widget<Line<0, '-'>>);
static_assert (c::widget<Check<std::string_view>>);
static_assert (c::widget<Radio<std::string_view, int>>);
static_assert (c::widget<Label<std::string_view>>);
static_assert (c::widget<Button<std::string_view, decltype ([] {})>>);
// These does not have the operator () and the height field

using MyLayout = Layout<detail::VBoxDecoration, decltype (std::tuple{check (""sv)})>;
static_assert (!c::widget<MyLayout>);
static_assert (!c::widget<Group<decltype ([] {}), decltype (std::make_tuple (radio (1, ""sv)))>>);
static_assert (!c::widget<Window<0, 0, 0, 0, false, Label<std::string_view>>>);

static_assert (is_layout<MyLayout>::value);
static_assert (!is_layout<decltype (check (""sv))>::value);
static_assert (!c::layout<Label<std::string_view>>);
static_assert (c::layout<MyLayout>);

static_assert (!c::group<int>);
static_assert (!c::group<Label<std::string_view>>);
static_assert (!c::group<MyLayout>);
static_assert (c::group<Group<decltype ([] {}), decltype (std::make_tuple (radio (1, ""sv)))>>);

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
static_assert (detail::augment::window_wrapper<detail::augment::Window<int, int>>);
static_assert (!detail::augment::window_wrapper<detail::augment::Group<int, std::tuple<int>, 0, 0, 0, 0, DefaultDecor<int>>>);

static_assert (!detail::augment::group_wrapper<detail::augment::Widget<Label<std::string_view>, 0, 0, 0, 0>>);
static_assert (!detail::augment::group_wrapper<detail::augment::Layout<MyLayout, std::tuple<int>, 0, 0>>);
static_assert (!detail::augment::group_wrapper<detail::augment::Window<int, int>>);
static_assert (detail::augment::group_wrapper<detail::augment::Group<int, std::tuple<int>, 0, 0, 0, 0, DefaultDecor<int>>>);

int main () {}