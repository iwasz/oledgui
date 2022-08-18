/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "inputApi.h"
#include "regression.h"

namespace {
using namespace og;
using namespace std::string_view_literals;

enum class Windows {
        menu,
        textReferences,
        // dataReferencesCheck,
        // dataReferencesCombo,
        // dataReferencesComboEnum,
        // dataReferencesRadio,
};

ISuite<Windows> *mySuite{};

/*--------------------------------------------------------------------------*/

auto menu = window<0, 0, 18, 7> (vbox (button ("[back]"sv, [] { mainMenu(); }), //
button ("Text widget"sv, [] {
        mySuite->current () = Windows::textReferences; })/*,
                                       button ("Check API"sv, [] { mySuite->current () = Windows::dataReferencesCheck; }),
                                       button ("Combo API"sv, [] { mySuite->current () = Windows::dataReferencesCombo; }),
                                       button ("Combo enum"sv, [] { mySuite->current () = Windows::dataReferencesComboEnum; }),
                                       button ("Radio API"sv, [] { mySuite->current () = Windows::dataReferencesRadio; })*/) );

auto backButton = button ("[back]"sv, [] { mySuite->current () = Windows::menu; });

/*--------------------------------------------------------------------------*/

std::string buff{R"(The
class
template
basic_string_view describes an object that can refer to a constant contiguous sequence of
char-like
objects
with
the first element of the sequence at position zero.)"};
// std::string buff{"aaa"};

auto txt = text<17, 6> (std::ref (buff));
LineOffset startLine{};
auto up = button ("▲"sv, [/* &txt, &startLine */] { startLine = txt.setStartLine (--startLine); });
auto dwn = button ("▼"sv, [/* &txt, &startLine */] { startLine = txt.setStartLine (++startLine); });
auto txtComp = hbox (std::ref (txt), vbox<1> (std::ref (up), vspace<4>, std::ref (dwn)));

auto textReferences = window<0, 0, 18, 7> (vbox (std::ref (backButton), std::ref (txtComp)));
// log (x);

/*--------------------------------------------------------------------------*/

auto s = suite<Windows> (element (Windows::menu, std::ref (menu)), //
                         element (Windows::textReferences, std::ref (textReferences))
                         //  element (Windows::dataReferencesCheck, std::ref (dataReferencesCheck)),
                         //  element (Windows::dataReferencesCombo, std::ref (dataReferencesCombo)),
                         //  element (Windows::dataReferencesComboEnum, std::ref (dataReferencesComboEnum)),
                         //  element (Windows::dataReferencesRadio, std::ref (dataReferencesRadio))
);

} // namespace

og::detail::augment::IWindow &textWidget ()
{
        mySuite = &s;
        return s;
}