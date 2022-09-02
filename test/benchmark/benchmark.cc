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
// #include "oledgui/debug.h"
#include "oledgui/ncurses.h"

using namespace og;
using namespace std::string_view_literals;

enum class Windows { menu, allFeatures, dialog };

/****************************************************************************/

int main ()
{

        NcursesDisplay<18, 7> d1;

        ISuite<Windows> *mySuiteP{};

        int cnt{};

        auto backButton = button ("[back]"sv, [&mySuiteP] { mySuiteP->current () = Windows::menu; });

        auto allFeatures = window<0, 0, 18, 7> (
                vbox (std::ref (backButton),                                                                                                 //
                      hbox (label ("Hello "sv), check (true, " 1 "sv), check (false, " 2 "sv)),                                              //
                      hbox (label ("World "sv), check (false, " 5 "sv), check (true, " 6 "sv)),                                              //
                      button ("Open dialog"sv, [&mySuiteP] { mySuiteP->current () = Windows::dialog; }),                                     //
                      line<18>,                                                                                                              //
                      group ([&cnt] (auto o) { cnt += o; }, radio (0, " R "sv), radio (1, " G "sv), radio (2, " B "sv), radio (3, " A "sv)), //
                      line<18>,                                                                                                              //
                      hbox (group ([&cnt] (auto o) { cnt += o; }, 2, radio (0, " R "sv), radio (1, " G "sv), radio (2, " B "sv),
                                   radio (3, " A "sv))),                                                                                  //
                      line<18>,                                                                                                           //
                      combo ([&cnt] (auto o) { cnt += o; }, option (0, "red"sv), option (1, "green"sv), option (2, "blue"sv)),            //
                      line<18>,                                                                                                           //
                      hbox (button ("Aaa"sv, [&cnt] { ++cnt; }), hspace<1>, button ("Bbb"sv, [] {}), hspace<1>, button ("Ccc"sv, [] {})), //
                      line<18>,                                                                                                           //
                      check ([&cnt] (bool checked) { cnt += int (checked); }, " 1 "sv),                                                   //
                      check ([&cnt] (bool checked) { cnt += int (checked); }, false, " 2 "sv),                                            //
                      check (false, " 3 "sv),                                                                                             //
                      check (false, " 4 "sv),                                                                                             //
                      check (false, " 5 "sv),                                                                                             //
                      check (false, " 6 "sv),                                                                                             //
                      check (false, " 7 "sv),                                                                                             //
                      check (false, " 8 "sv),                                                                                             //
                      check (false, " 9 "sv),                                                                                             //
                      check (false, " 10 "sv),                                                                                            //
                      check (false, " 11 "sv),                                                                                            //
                      check (false, " 12 "sv),                                                                                            //
                      check (false, " 13 "sv),                                                                                            //
                      check (false, " 14 "sv),                                                                                            //
                      check (false, " 15 "sv)                                                                                             //
                      ));                                                                                                                 //

        // log (allFeatures);

        /*--------------------------------------------------------------------------*/

        auto v = vbox (label ("  PIN:"sv), label (" 123456"sv),
                       hbox (button ("[OK]"sv, [&mySuiteP] { mySuiteP->current () = Windows::allFeatures; }), button ("[Cl]"sv, [] {})),
                       check (false, "15 "sv));

        auto dialog = window<4, 1, 10, 5, true> (std::ref (v));
        // log (dialog);

        auto menu = window<0, 0, 18, 7> (vbox (label ("----Main menu-----"sv),
                                               button ("input API  "sv, [&mySuiteP] { mySuiteP->current () = Windows::allFeatures; }),
                                               button ("text widget"sv, [&mySuiteP] { mySuiteP->current () = Windows::dialog; })));

        auto mySuite = suite<Windows> (element (Windows::menu, std::ref (menu)),                            //
                                       element (Windows::allFeatures, std::ref (allFeatures)),              //
                                       element (Windows::dialog, std::ref (allFeatures), std::ref (dialog)) //
        );

        mySuiteP = &mySuite;

        while (true) {
                draw (d1, mySuite);
                input (d1, mySuite, getKey ());
        }

        return 0;
}
