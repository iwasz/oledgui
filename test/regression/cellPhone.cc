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

int volumeLevel{1};

auto volume = hbox<2> (combo<CanFocus::no> (std::ref (volumeLevel), option (0, "🕨"sv), option (1, "🕩"sv), option (2, "🕪"sv)));

auto cellP = window<0, 0, 18, 7> (
        vbox (hbox (label ("12:34"sv), hspace<10>, std::ref (volume), button ("…"sv, [] { mainMenu (); })),  // top status bar
              hbox (combo (std::ref (volumeLevel), option (0, "0"sv), option (1, "1"sv), option (2, "2"sv))) // volume option (settable)

              ));

// "📶🔇🔈🔉🔊🕨🕩🕪"

} // namespace

og::detail::augment::IWindow &cellPhone () { return cellP; }