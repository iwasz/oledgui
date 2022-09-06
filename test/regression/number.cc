/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "number.h"
#include "regression.h"

namespace {
using namespace og;
using namespace std::string_view_literals;

enum class Windows {
        menu,
        integer,
};

ISuite<Windows> *mySuite{};
std::string chkBxLabel = "-";
std::string rdoBxLabel = "-";
std::string cboLabel = "-";
std::string buttonLabel = "-";
int buttonCnt{};

auto menu = window<0, 0, 18, 7> (vbox (button ("[back]"sv, [] { mainMenu (); }),                          //
                                       group ([] (Windows s) { mySuite->current () = s; }, Windows::menu, //
                                              item (Windows::integer, "Simple integer"sv)                 //
                                              )));

auto backButton = button ("[back]"sv, [] { mySuite->current () = Windows::menu; });

/**
 * All available inputs with callbacks.
 */
auto integer = window<0, 0, 18, 7> (vbox (std::ref (backButton),                                                        //
                                          hbox (label ("8bit, 0-9(1): "sv), og::number ([] (auto val) {}, uint8_t (0))) //
                                          ));

/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/

auto s = suite<Windows> (element (Windows::menu, std::ref (menu)), //
                         element (Windows::integer, std::ref (integer)));

} // namespace

og::detail::augment::IWindow &number ()
{
        mySuite = &s;
        return s;
}