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

/****************************************************************************/

struct MyConfiguration {

        bool light0{};
        bool light1{};

        enum class Color { red, green, blue, alpha };
        Color color{};
};

/****************************************************************************/

int main ()
{
        using namespace og;
        using namespace std::string_view_literals;

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
        //                                 //       hbox (label ("World "sv), check (" 5 "sv), check (" 6 "sv)), // button ("Text widget"sv,
        //                                 //       [&mySuite] { mySuite->current () = Windows::textReferences; }),                       //
        //                                 button
        //                                 //       ("Layouts"sv, [&mySuite] { mySuite->current () = Windows::layouts; }), // button
        //                                 ("Callbacks"sv,
        //                                 //       [&mySuite] { mySuite->current () = Windows::callbacks; }),                              //
        //         line<18>,               //
        //         group ([] (auto const &o) {}, radio (0, " R "sv), radio (1, " G "sv), radio (1, " B "sv), radio (1, " A "sv)),        //
        //         line<18>,                                                                                                             //
        //         hbox (group ([] (auto const &o) {}, radio (0, " R "sv), radio (1, " G "sv), radio (1, " B "sv), radio (1, " A "sv))), //

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

        std::string buff{R"(The
class
template
basic_string_view describes an object that can refer to a constant contiguous sequence of
char-like
objects
with
the first element of the sequence at position zero.)"};
        // std::string buff{"aaa"};

        // auto txt = text<17, 7> (std::ref (buff));
        // LineOffset startLine{};
        // auto up = button ("▲"sv, [&txt, &startLine] { startLine = txt.setStartLine (--startLine); });
        // auto dwn = button ("▼"sv, [&txt, &startLine] { startLine = txt.setStartLine (++startLine); });
        // auto txtComp = hbox (std::ref (txt), vbox<1> (std::ref (up), vspace<5>, std::ref (dwn)));

        // auto chk = check (" 1 "sv);

        // auto grp = group ([] (auto o) {}, radio (0, " R "sv), radio (1, " G "sv), radio (1, " B "sv), radio (1, " A "sv), radio (1, " C "sv),
        //                   radio (1, " M "sv), radio (1, " Y "sv), radio (1, " K "sv));

        // auto vv = vbox (txtComp, //
        //                 hbox (hbox<1> (label ("▲"sv)), label ("▲"sv), label ("▲"sv)), vbox ());
        // //                 hbox (std::ref (chk), check (" 2 "sv)), //
        // //                 std::ref (grp)                          //
        // // );

        // auto textReferences = window<0, 0, 18, 7> (std::ref (txtComp));
        // // log (x);

        // /*--------------------------------------------------------------------------*/

        // auto v = vbox (label ("  PIN:"sv), label (" 123456"sv),
        //                hbox (button ("[OK]"sv, [/* &showDialog */] { /* showDialog = false; */ }), button ("[Cl]"sv, [] {})), check (" 15
        //                "sv));

        // auto dialog = window<4, 1, 10, 5, true> (std::ref (v));

        // auto s = suite<Windows> (element (Windows::dialog, std::ref (dialog), std::ref (x)),
        //                                               element (Windows::xWindow, std::ref (x)));

        // s.current () = Windows::dataReferencesRadio;

        // og::detail::augment::IWindow &window = s;

        // log (dialog);

        while (true) {
                draw (d1, inputApi ());
                int ch = getch ();

                switch (ch) {
                // case 'w':
                //         // startLine = txt.setStartLine (--startLine);
                //         --textReferences.context.currentScroll;
                //         break;
                // case 's':
                //         // startLine = txt.setStartLine (++startLine);
                //         ++textReferences.context.currentScroll;
                //         break;
                // case 'a':
                //         buff = "ala ma kota";
                //         break;
                // case 'c':
                //         chk.checked () = !chk.checked ();
                //         break;
                default:
                        input (d1, inputApi (), getKey (ch));
                        break;
                }

                // std::cout << cid << std::endl;
        }

        return 0;
}
