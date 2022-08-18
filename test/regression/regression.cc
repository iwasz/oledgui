/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

/**
 * All functionalities in one place. Manual regression testing
 */
#include "inputApi.h"
#include "oledgui/debug.h"
#include "oledgui/ncurses.h"
#include "textWidget.h"

using namespace og;
using namespace std::string_view_literals;

// struct MyConfiguration {

//         bool light0{};
//         bool light1{};

//         enum class Color { red, green, blue, alpha };
//         Color color{};
// };

enum class Windows { menu, inputApi, textWidget };
ISuite<Windows> *mySuiteP{};

/****************************************************************************/

int main ()
{

        NcursesDisplay<18, 7> d1;

        /*--------------------------------------------------------------------------*/

        auto layouts = window<0, 0, 18, 7> (vbox (                         //
                hbox (hbox<9> (label ("h1"sv)), hbox<9> (label ("h2"sv))), //
                hbox (hbox<9> (label ("h3"sv)), hbox<9> (label ("h4"sv)))) //
        );

        /*--------------------------------------------------------------------------*/

        // auto allFeatures = window<0, 0, 18, 7> (vbox (
        //         hbox (label ("Hello "sv), check (" 1 "sv),
        //               check (" 2 "sv)), //
        //                                 //       hbox (label ("World "sv), check (" 5 "sv), check (" 6 "sv)), // button ("Text
        //                                 widget"sv,
        //                                 //       [&mySuite] { mySuite->current () = Windows::textReferences; }), // button
        //                                 //       ("Layouts"sv, [&mySuite] { mySuite->current () = Windows::layouts; }), // button
        //                                 ("Callbacks"sv,
        //                                 //       [&mySuite] { mySuite->current () = Windows::callbacks; }), //
        //         line<18>,               //
        //         group ([] (auto const &o) {}, radio (0, " R "sv), radio (1, " G "sv), radio (1, " B "sv), radio (1, " A "sv)), //
        //         line<18>, // hbox (group ([] (auto const &o) {}, radio (0, " R "sv), radio (1, " G "sv), radio (1, " B "sv), radio (1,
        //         " A "sv))), //

        //         line<18>, //
        //         // Combo (Options (option (0, "red"sv), option (1, "green"sv), option (1, "blue"sv)), [] (auto const &o) {}) //
        //         combo ([] (auto const &o) {}, option (0, "red"sv), option (1, "green"sv), option (1, "blue"sv)) //
        //         //       line<18>,                                                                                               //
        //         //       hbox (button ("Aaa"sv, [] {}), hspace<1>, button ("Bbb"sv, [] {}), hspace<1>, button ("Ccc"sv, [] {})), //
        //         //       line<18>,                                                                                               //
        //         //       check (" 1 "sv, [] (bool checked) { /* ... */ }),                                                       //
        //         //       check (" 2 "sv),                                                                                        //
        //         //       check (" 3 "sv),                                                                                        //
        //         //       check (" 4 "sv),                                                                                        //
        //         //       check (" 5 "sv),                                                                                        //
        //         //       check (" 6 "sv),                                                                                        //
        //         //       check (" 7 "sv),                                                                                        //
        //         //       check (" 8 "sv),                                                                                        //
        //         //       check (" 9 "sv),                                                                                        //
        //         //       check (" 10 "sv),                                                                                       //
        //         //       check (" 11 "sv),                                                                                       //
        //         //       check (" 12 "sv),                                                                                       //
        //         //       check (" 13 "sv),                                                                                       //
        //         //       check (" 14 "sv),                                                                                       //
        //         //       check (" 15 "sv)                                                                                        //
        //         )); //

        // log (x);
        /*--------------------------------------------------------------------------*/

        // /*--------------------------------------------------------------------------*/

        // auto v = vbox (label ("  PIN:"sv), label (" 123456"sv),
        //                hbox (button ("[OK]"sv, [/* &showDialog */] { /* showDialog = false; */ }), button ("[Cl]"sv, [] {})), check ("
        //                15 "sv));

        // auto dialog = window<4, 1, 10, 5, true> (std::ref (v));

        auto menu = window<0, 0, 18, 7> (vbox (label ("----Main menu-----"sv),
                                               button ("inputApi"sv, [] { mySuiteP->current () = Windows::inputApi; }),
                                               button ("textWidget"sv, [] { mySuiteP->current () = Windows::textWidget; })));

        auto mySuite = suite<Windows> (element (Windows::menu, std::ref (menu)),            //
                                       element (Windows::inputApi, std::ref (inputApi ())), //
                                       element (Windows::textWidget, std::ref (textWidget ())));
        mySuiteP = &mySuite;

        // s.current () = Windows::dataReferencesRadio;

        // og::detail::augment::IWindow &window = s;

        // log (dialog);

        while (true) {
                draw (d1, mySuite);
                input (d1, mySuite, getKey ());
        }

        return 0;
}

/// Global functions help to implement "go back to main" menu button
void mainMenu () { mySuiteP->current () = Windows::menu; }