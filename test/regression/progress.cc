/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "progress.h"
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

auto menu = window<0, 0, 18, 7> (vbox (button ([] { mainMenu (); }, "[back]"sv),                          //
                                       group ([] (Windows s) { mySuite->current () = s; }, Windows::menu, //
                                              item (Windows::integer, "Simple progress"sv)                //
                                              )));

auto backButton = button ([] { mySuite->current () = Windows::menu; }, "[back]"sv);

uint8_t sharedVal{};

/**
 * All available inputs with callbacks.
 */
auto integer = window<0, 0, 18, 7> (vbox (std::ref (backButton),                                                              //
                                          hbox (label ("value: "sv), og::number<0, 16> (std::ref (sharedVal))),               //
                                          hbox (label ("["sv), og::progress<16, 0, 16> (std::ref (sharedVal)), label ("]"sv)) //
                                          ));

/*--------------------------------------------------------------------------*/

auto s = suite<Windows> (element (Windows::menu, std::ref (menu)), //
                         element (Windows::integer, std::ref (integer)));

} // namespace

og::detail::augment::IWindow &progress ()
{
        mySuite = &s;
        return s;
}