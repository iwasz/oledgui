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
        callbacks,
        dataReferencesCheck,
        dataReferencesCheckCtad,
        dataReferencesCombo,
        dataReferencesComboEnum,
        dataReferencesRadio,
};

ISuite<Windows> *mySuite{};
std::string chkBxLabel = "-";
std::string rdoBxLabel = "-";
std::string cboLabel = "-";
std::string buttonLabel = "-";
int buttonCnt{};

auto menu = window<0, 0, 18, 7> (vbox (button ("[back]"sv, [] { mainMenu (); }), //
                                       button ("Callbacks"sv, [] { mySuite->current () = Windows::callbacks; }),
                                       button ("Check API"sv, [] { mySuite->current () = Windows::dataReferencesCheck; }),
                                       button ("Check API CTAD"sv, [] { mySuite->current () = Windows::dataReferencesCheckCtad; }),
                                       button ("Combo API"sv, [] { mySuite->current () = Windows::dataReferencesCombo; }),
                                       button ("Combo enum"sv, [] { mySuite->current () = Windows::dataReferencesComboEnum; }),
                                       button ("Radio API"sv, [] { mySuite->current () = Windows::dataReferencesRadio; })));

auto backButton = button ("[back]"sv, [] { mySuite->current () = Windows::menu; });

/**
 * All available inputs with callbacks.
 */
auto callbacks = window<0, 0, 18, 7> (
        vbox (std::ref (backButton),
              hbox (check ([] (bool active) { chkBxLabel = (active) ? ("true") : ("false"); }, " chkbx "sv), label (std::ref (chkBxLabel))),

              hbox (group ([] (auto selected) { rdoBxLabel = std::to_string (selected); }, //
                           radio (0, " R "sv),                                             //
                           radio (1, " G "sv),                                             //
                           radio (2, " B "sv)),                                            //
                    label (std::ref (rdoBxLabel))),                                        //

              hbox (combo ([] (auto selected) { cboLabel = std::to_string (selected); }, //
                           option (6, "red"sv),                                          //
                           option (7, "green"sv),                                        //
                           option (8, "blue"sv)),                                        //
                    hspace<1>,                                                           //
                    label (std::ref (cboLabel))),

              hbox (button ("[OK]"sv, [] { buttonLabel = std::to_string (++buttonCnt); }), hspace<1>, label (std::ref (buttonLabel)))

                      ));

/*--------------------------------------------------------------------------*/

bool bbb{true};

auto dataReferencesCheck = window<0, 0, 18, 7> (vbox (std::ref (backButton),                   //
                                                      check (true, " PR value "sv),            //
                                                      check (std::ref (bbb), " std::ref 1"sv), //
                                                      check ([] (bool) {}, std::ref (bbb), " std::ref 2"sv)));

/*--------------------------------------------------------------------------*/

auto dataReferencesCheckCtad = window<0, 0, 18, 7> (vbox (std::ref (backButton),                   //
                                                          check (true, " PR value "sv),            //
                                                          check (std::ref (bbb), " std::ref 1"sv), //
                                                          Check ([] (bool) {}, std::ref (bbb), " std::ref 2"sv)));

/*--------------------------------------------------------------------------*/

int cid{777};

auto dataReferencesCombo
        = window<0, 0, 18, 7> (vbox (std::ref (backButton), //
                                     combo (888, option (666, "red"sv), option (777, "green"sv), option (888, "blue"sv)),
                                     combo (std::ref (cid), option (666, "red"sv), option (777, "green"sv), option (888, "blue"sv)),
                                     combo ([] (int) {}, std::ref (cid), option (666, "RED"sv), option (777, "GREEN"sv), option (888, "BLUE"sv)))

        );

/*--------------------------------------------------------------------------*/

enum class Color { red, green, blue };
Color clr{Color::blue};

auto dataReferencesComboEnum = window<0, 0, 18, 7> (hbox (std::ref (backButton),
                                                          combo ([] (Color clr) {}, std::ref (clr), option (Color::red, "red"sv),
                                                                 option (Color::green, "green"sv), option (Color::blue, "blue"sv))));

/*--------------------------------------------------------------------------*/

int gid{2};

auto dataReferencesRadio
        = window<0, 0, 18, 7> (vbox (std::ref (backButton), //
                                     hbox (group ([] (int) {}, 1, radio (0, " c "sv), radio (1, " m "sv), radio (2, " y "sv))),
                                     hbox (group (std::ref (gid), radio (0, " r "sv), radio (1, " g "sv), radio (2, " b "sv))),
                                     hbox (group ([] (int) {}, std::ref (gid), radio (0, " R "sv), radio (1, " G "sv), radio (2, " B "sv))))

        );

/*--------------------------------------------------------------------------*/

auto s = suite<Windows> (element (Windows::menu, std::ref (menu)), //
                         element (Windows::callbacks, std::ref (callbacks)),
                         element (Windows::dataReferencesCheck, std::ref (dataReferencesCheck)),
                         element (Windows::dataReferencesCombo, std::ref (dataReferencesCombo)),
                         element (Windows::dataReferencesCheckCtad, std::ref (dataReferencesCheckCtad)),
                         element (Windows::dataReferencesComboEnum, std::ref (dataReferencesComboEnum)),
                         element (Windows::dataReferencesRadio, std::ref (dataReferencesRadio)));

} // namespace

og::detail::augment::IWindow &inputApi ()
{
        mySuite = &s;
        return s;
}