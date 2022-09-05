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

enum class Windows { menu, allFeatures, dialog, textBox };

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
                      check (false, " 16 "sv)                                                                                             //
                      ));                                                                                                                 //

        // log (allFeatures);

        /*--------------------------------------------------------------------------*/

        auto v = vbox (label ("  PIN:"sv), label (" 123456"sv),
                       hbox (button ("[OK]"sv, [&mySuiteP] { mySuiteP->current () = Windows::allFeatures; }), button ("[Cl]"sv, [] {})),
                       check (false, "15 "sv));

        auto dialog = window<4, 1, 10, 5, true> (std::ref (v));
        // log (dialog);

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
        auto up = button ("▲"sv, [&txt, &startLine] { startLine = txt.setStartLine (--startLine); });
        auto dwn = button ("▼"sv, [&txt, &startLine] { startLine = txt.setStartLine (++startLine); });
        auto txtComp = hbox (std::ref (txt), vbox<1> (std::ref (up), vspace<4>, std::ref (dwn)));

        auto textReferences = window<0, 0, 18, 7> (vbox (std::ref (backButton), std::ref (txtComp)));

        /*--------------------------------------------------------------------------*/

        auto menu = window<0, 0, 18, 7> (vbox (label ("----Main menu-----"sv),
                                               button ("input API  "sv, [&mySuiteP] { mySuiteP->current () = Windows::allFeatures; }),
                                               button ("text widget"sv, [&mySuiteP] { mySuiteP->current () = Windows::textBox; })));

        auto mySuite = suite<Windows> (element (Windows::menu, std::ref (menu)),               //
                                       element (Windows::allFeatures, std::ref (allFeatures)), //
                                       element (Windows::dialog, std::ref (allFeatures), std::ref (dialog)),
                                       element (Windows::textBox, std::ref (textReferences)) //
        );

        mySuiteP = &mySuite;

        for (int i = 0; i < 50000; ++i) {
                draw (d1, mySuite);
                input (d1, mySuite, Key::select);

                draw (d1, mySuite);
                input (d1, mySuite, Key::incrementFocus);
        }

        return 0;
}
