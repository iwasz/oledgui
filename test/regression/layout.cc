/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "cellPhone.h"
#include "regression.h"

namespace {
using namespace og;
using namespace std::string_view_literals;

enum class Win { menu, layout, window };
ISuite<Win> *mySuite{};

auto menu = window<0, 0, 18, 7> (vbox (button ("[back]"sv, [] { mainMenu (); }),       //
                                       group ([] (Win s) { mySuite->current () = s; }, //
                                              item (Win::layout, "Layout (2x2)"sv),    //
                                              item (Win::window, "Window"sv)           //
                                              )));

auto back = button ("[back]"sv, [] { mySuite->current () = Win::menu; });

/****************************************************************************/

// clang-format off
auto layout = window<0, 0, 18, 7> (vbox (                          
        hbox (hbox<9> (label ("cell1"sv)), hbox<9> (vbox (label ("cell2"sv), 
                                                          label ("cell2"sv),
                                                          hspace <1>
        ))),

        hbox (hbox<9> (label ("cell3"sv)), hbox<9> (vbox (label ("cell4"sv), 
                                                          label ("cell4"sv),
                                                          hspace <1>))),

        std::ref (back)
));
// clang-format on

/****************************************************************************/

auto background = window<0, 0, 18, 7> (vbox (text<18, 7> (
        "0xdeadbabe0xdeadbee0xdeadbabe0xdeadbee0xdeadbabe0xdeadbee0xdeadbabe0xdeadbee0xdeadbabe0xdeadbee0xdeadbabe0xdeadbee0xdeadbabe0xdeadbee"sv)));
auto win = window<5, 1, 8, 4, true> (vbox (label ("Window"sv), std::ref (back)));

/****************************************************************************/

auto s = suite (element (Win::menu, std::ref (menu)),                        //
                element (Win::layout, std::ref (layout)),                    // Desktop only
                element (Win::window, std::ref (background), std::ref (win)) // Desktop covered by the menu

);

} // namespace

og::detail::augment::IWindow &layout ()
{
        mySuite = &s;
        return s;
}