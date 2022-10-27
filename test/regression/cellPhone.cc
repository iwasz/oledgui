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

enum class Win { desktop, dropDown };
ISuite<Win> *mySuite{};

int volumeLevel{1};

struct CustomComboStyle {
        static constexpr style::Focus focus = style::Focus::disabled;
};

auto volume = hbox<2> (combo<CustomComboStyle> (std::ref (volumeLevel), option (0, "🕨"sv), option (1, "🕩"sv), option (2, "🕪"sv)));

auto desktop = window<0, 0, 18, 7> (vbox (
        hbox (label ("12:34"sv), hspace<10>, std::ref (volume), button ([] { mySuite->current () = Win::dropDown; }, "…"sv)), // top status bar
        hbox ()

                ));

/*--------------------------------------------------------------------------*/

// auto dropDown = window<10, 1, 8, 6, true> (vbox (hbox (label ("vol:"sv), label ("11"sv))));

auto dropDown = window<10, 1, 8, 6, true> (
        vbox (hbox (label ("vol:"sv), combo (std::ref (volumeLevel), option (0, "0"sv), option (1, "1"sv), option (2, "2"sv))), // volume option
              hbox (check (false, " A"sv), check (false, " B"sv)),                                                              //
              label ("xxx"sv), button ([] { mySuite->current () = Win::desktop; }, "exit"sv),                                   //
              button ([] { mainMenu (); }, "prDn"sv)));

auto cellP = suite (element (Win::desktop, std::ref (desktop)),                      // Desktop only
                    element (Win::dropDown, std::ref (desktop), std::ref (dropDown)) // Desktop covered by the menu

);

} // namespace

og::detail::augment::IWindow &cellPhone ()
{
        mySuite = &cellP;
        return cellP;
}