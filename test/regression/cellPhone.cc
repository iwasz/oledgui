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

auto volume = hbox<2> (combo ([] (auto) {}, option (0, "🕨"sv), option (1, "🕩"sv), option (2, "🕪"sv)));

auto cellP = window<0, 0, 18, 7> (
        vbox (hbox (label ("12:34"sv), hspace<10>, std::ref (volume), button ("…"sv, [] { mainMenu (); })) // top status bar

              ));

// "📶🔇🔈🔉🔊🕨🕩🕪"

} // namespace

og::detail::augment::IWindow &cellPhone () { return cellP; }