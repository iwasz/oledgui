/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "inputApi.h"
#include "customStyles.h"
#include "regression.h"
#include "utils.h"

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

auto menu = window<0, 0, 18, 7> (vbox (button ([] { mainMenu (); }, "[back]"sv),                               //
                                       group ([] (Windows s) { mySuite->current () = s; }, Windows::callbacks, //
                                              item (Windows::callbacks, "Callbacks"sv),                        //
                                              item (Windows::dataReferencesCheck, "Check API"sv),              //
                                              item (Windows::dataReferencesCheckCtad, "Check API CTAD"sv),     //
                                              item (Windows::dataReferencesCombo, "Combo API"sv),              //
                                              item (Windows::dataReferencesComboEnum, "Combo enum"sv),         //
                                              item (Windows::dataReferencesRadio, "Radio API"sv)               //
                                              )));

auto backButton = button ([] { mySuite->current () = Windows::menu; }, "[back]"sv);

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

              hbox (button ([] { buttonLabel = std::to_string (++buttonCnt); }, "[OK]"sv), hspace<1>, label (std::ref (buttonLabel)))

                      ));

/*--------------------------------------------------------------------------*/

bool bbb{true};
Cfg<bool> boolCfg{bbb};

auto dataReferencesCheck = window<0, 0, 18, 7> (vbox (std::ref (backButton),                                 //
                                                      check (true, " PR value "sv),                          //
                                                      check (std::ref (bbb), " std::ref 1"sv),               //
                                                      check ([] (bool) {}, std::ref (bbb), " std::ref 2"sv), //
                                                      check (std::ref (boolCfg), " custom type"sv)           //
                                                      ));

/*--------------------------------------------------------------------------*/

auto dataReferencesCheckCtad = window<0, 0, 18, 7> (vbox (std::ref (backButton),                   //
                                                          check (true, " PR value "sv),            //
                                                          check (std::ref (bbb), " std::ref 1"sv), //
                                                          check ([] (bool) {}, std::ref (bbb), " std::ref 2"sv)));

/*--------------------------------------------------------------------------*/

int cid{777};
Cfg<int> intCfg{cid};

auto dataReferencesCombo
        = window<0, 0, 18, 7> (vbox (std::ref (backButton), //
                                     combo (888, option (666, "red"sv), option (777, "green"sv), option (888, "blue"sv)),
                                     combo (std::ref (cid), option (666, "red"sv), option (777, "green"sv), option (888, "blue"sv)),
                                     combo ([] (int) {}, std::ref (cid), option (666, "RED"sv), option (777, "GREEN"sv), option (888, "BLUE"sv)),
                                     combo (std::ref (intCfg), option (666, "red"sv), option (777, "green"sv), option (888, "blue"sv)) //
                                     ));

/*--------------------------------------------------------------------------*/

enum class Color { red, green, blue };
Color clr{Color::blue};
Cfg<Color> colorCfg{clr};

auto dataReferencesComboEnum = window<0, 0, 18, 7> (
        vbox (std::ref (backButton),
              combo ([] (Color clr) {}, std::ref (clr), option (Color::red, "red"sv), option (Color::green, "green"sv),
                     option (Color::blue, "blue"sv)),
              combo (std::ref (colorCfg), option (Color::red, "red"sv), option (Color::green, "green"sv), option (Color::blue, "blue"sv)) //
              ));

/*--------------------------------------------------------------------------*/

int gid{2};
Cfg<int> intCfg2{gid};

auto dataReferencesRadio
        = window<0, 0, 18, 7> (vbox (std::ref (backButton),                                                                                  //
                                     hbox (group ([] (int) {}, 1, radio (0, " c "sv), radio (1, " m "sv), radio (2, " y "sv))),              //
                                     hbox (group (std::ref (gid), radio (0, " r "sv), radio (1, " g "sv), radio (2, " b "sv))),              //
                                     hbox (group ([] (int) {}, std::ref (gid), radio (0, " R "sv), radio (1, " G "sv), radio (2, " B "sv))), //
                                     hbox (group (std::ref (intCfg2), radio (0, " r "sv), radio (1, " g "sv), radio (2, " b "sv)))           //
                                     ));

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