/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "oledgui/ncurses.h"
#include "oledgui/debug.h"

// int test1 ()
// {
//         using namespace og;

//         /*
//          * TODO This has the problem that it woud be tedious and wasteful to declare ncurses<18, 7>
//          * with every new window/view. Alas I'm trying to investigate how to commonize Displays and Windows.
//          *
//          * This ncurses method can be left as an option.
//          */
//         auto vb = ncurses<18, 7> (vbox (vbox (radio (0, " A "), radio (1, " B "), radio (2, " C "), radio (3, " d ")), //
//                                         line<10>,                                                                      //
//                                         vbox (check (" 1 "), check (" 2 "), check (" 3 "), check (" 4 ")),             //
//                                         line<10>,                                                                      //
//                                         vbox (radio (0, " a "), radio (0, " b "), radio (0, " c "), radio (0, " d ")), //
//                                         line<10>,                                                                      //
//                                         vbox (check (" 5 "), check (" 6 "), check (" 7 "), check (" 8 ")),             //
//                                         line<10>,                                                                      //
//                                         vbox (radio (0, " E "), radio (0, " F "), radio (0, " G "), radio (0, " H "))  //
//                                         ));                                                                            //

//         // auto dialog = window<2, 2, 10, 10> (vbox (radio (" A "), radio (" B "), radio (" C "), radio (" d ")));
//         // // dialog.calculatePositions ();

//         bool showDialog{};

//         while (true) {
//                 // d1.clear ()
//                 // vb (d1);

//                 vb ();

//                 // if (showDialog) {
//                 //         dialog (d1);
//                 // }

//                 // wrefresh (d1.win);
//                 int ch = getch ();

//                 if (ch == 'q') {
//                         break;
//                 }

//                 switch (ch) {
//                 case KEY_DOWN:
//                         vb.incrementFocus ();
//                         break;

//                 case KEY_UP:
//                         vb.decrementFocus ();
//                         break;

//                 case 'd':
//                         showDialog = true;
//                         break;

//                 default:
//                         // d1.input (vb, char (ch));
//                         // vb.input (d1, char (ch), 0, 0, dummy);
//                         break;
//                 }
//         }

//         return 0;
// }

/****************************************************************************/

struct MyConfiguration {

        bool light0{};
        bool light1{};

        enum class Color { red, green, blue, alpha };
        Color color{};
};

/****************************************************************************/

int test2 ()
{
        using namespace og;
        using namespace std::string_view_literals;

        NcursesDisplay<18, 7> d1;

        enum class Windows : int { callbacks, allFeatures, dialog, textReferences, layouts };
        ISuite<Windows> *mySuite{};

        /*--------------------------------------------------------------------------*/

        std::string chkBxLabel = "false";
        std::string rdoBxLabel = "0";

        // auto callbacks = window<0, 0, 18, 7> (
        //         vbox (hbox (check (" chkbx "sv, [&chkBxLabel] (bool active) { chkBxLabel = (active) ? ("true") : ("false"); }),
        //                     label (std::ref (chkBxLabel))),

        //               hbox (group ([&rdoBxLabel] (auto selected) { rdoBxLabel = std::to_string (selected); }, radio (6, " R "sv),
        //                            radio (7, " G "sv), radio (8, " B "sv)),
        //                     label (std::ref (rdoBxLabel)))

        //                       ));
        auto callbacks = window<0, 0, 18, 7> (hbox (

                // hbox (group ([&rdoBxLabel] (auto selected) { rdoBxLabel = std::to_string (selected); }, radio (6, " R "sv), radio (7, " G
                // "sv),
                //              radio (8, " B "sv)),
                //       label (std::ref (rdoBxLabel)))
                hbox (check (" aaaa "sv)), label ("B"sv)

                        ));

        // log (callbacks);

        /*--------------------------------------------------------------------------*/

        auto layouts = window<0, 0, 18, 7> (vbox (                         //
                hbox (hbox<9> (label ("h1"sv)), hbox<9> (label ("h2"sv))), //
                hbox (hbox<9> (label ("h3"sv)), hbox<9> (label ("h4"sv)))) //
        );

        /*--------------------------------------------------------------------------*/

        auto allFeatures = window<0, 0, 18, 7> (
                vbox (hbox (label ("Hello "sv), check (" 1 "sv), check (" 2 "sv)),                                                   //
                      hbox (label ("World "sv), check (" 5 "sv), check (" 6 "sv)),                                                   //
                      button ("Text widget"sv, [&mySuite] { mySuite->current () = Windows::textReferences; }),                       //
                      button ("Layouts"sv, [&mySuite] { mySuite->current () = Windows::layouts; }),                                  //
                      button ("Callbacks"sv, [&mySuite] { mySuite->current () = Windows::callbacks; }),                              //
                      line<18>,                                                                                                      //
                      group ([] (auto const &o) {}, radio (0, " R "sv), radio (1, " G "sv), radio (1, " B "sv), radio (1, " A "sv)), //
                      line<18>,                                                                                                      //
                      hbox (group ([] (auto const &o) {}, radio (0, " R "sv), radio (1, " G "sv), radio (1, " B "sv), radio (1, " A "sv))),

                      //       line<18>, //
                      //       Combo (Options (option (0, "red"), option (1, "green"), option (1, "blue")), [] (auto const &o) {}), //
                      line<18>,                                                                                               //
                      hbox (button ("Aaa"sv, [] {}), hspace<1>, button ("Bbb"sv, [] {}), hspace<1>, button ("Ccc"sv, [] {})), //
                      line<18>,                                                                                               //
                      check (" 1 "sv, [] (bool checked) { /* ... */ }),                                                       //
                      check (" 2 "sv),                                                                                        //
                      check (" 3 "sv),                                                                                        //
                      check (" 4 "sv),                                                                                        //
                      check (" 5 "sv),                                                                                        //
                      check (" 6 "sv),                                                                                        //
                      check (" 7 "sv),                                                                                        //
                      check (" 8 "sv),                                                                                        //
                      check (" 9 "sv),                                                                                        //
                      check (" 10 "sv),                                                                                       //
                      check (" 11 "sv),                                                                                       //
                      check (" 12 "sv),                                                                                       //
                      check (" 13 "sv),                                                                                       //
                      check (" 14 "sv),                                                                                       //
                      check (" 15 "sv)                                                                                        //
                      ));                                                                                                     //

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

        auto txt = text<17, 7> (std::ref (buff));
        LineOffset startLine{};
        auto up = button ("▲"sv, [&txt, &startLine] { startLine = txt.setStartLine (--startLine); });
        auto dwn = button ("▼"sv, [&txt, &startLine] { startLine = txt.setStartLine (++startLine); });
        auto txtComp = hbox (std::ref (txt), vbox<1> (std::ref (up), vspace<5>, std::ref (dwn)));

        auto chk = check (" 1 "sv);

        auto grp = group ([] (auto o) {}, radio (0, " R "sv), radio (1, " G "sv), radio (1, " B "sv), radio (1, " A "sv), radio (1, " C "sv),
                          radio (1, " M "sv), radio (1, " Y "sv), radio (1, " K "sv));

        auto vv = vbox (txtComp, //
                        hbox (hbox<1> (label ("▲"sv)), label ("▲"sv), label ("▲"sv)), vbox ());
        //                 hbox (std::ref (chk), check (" 2 "sv)), //
        //                 std::ref (grp)                          //
        // );

        auto textReferences = window<0, 0, 18, 7> (std::ref (txtComp));
        // log (x);

        /*--------------------------------------------------------------------------*/

        auto v = vbox (label ("  PIN:"sv), label (" 123456"sv),
                       hbox (button ("[OK]"sv, [/* &showDialog */] { /* showDialog = false; */ }), button ("[Cl]"sv, [] {})), check (" 15 "sv));

        auto dialog = window<4, 1, 10, 5, true> (std::ref (v));

        // auto s = suite<Windows> (element (Windows::dialog, std::ref (dialog), std::ref (x)),
        //                                               element (Windows::xWindow, std::ref (x)));
        auto s = suite<Windows> (element (Windows::callbacks, std::ref (callbacks)),           //
                                 element (Windows::textReferences, std::ref (textReferences)), //
                                 element (Windows::dialog, std::ref (dialog)),                 //
                                 element (Windows::allFeatures, std::ref (allFeatures)),       //
                                 element (Windows::layouts, std::ref (layouts))                //
        );
        mySuite = &s;
        s.current () = Windows::allFeatures;

        // log (dialog);

        while (true) {
                draw (d1, s);
                int ch = getch ();

                switch (ch) {
                case 'w':
                        // startLine = txt.setStartLine (--startLine);
                        --textReferences.context.currentScroll;
                        break;
                case 's':
                        // startLine = txt.setStartLine (++startLine);
                        ++textReferences.context.currentScroll;
                        break;
                case 'a':
                        buff = "ala ma kota";
                        break;
                case 'c':
                        chk.checked () = !chk.checked ();
                        break;
                default:
                        input (d1, s, getKey (ch));
                        break;
                }
        }

        return 0;
}

/****************************************************************************/

int main ()
{

        test2 ();
        // test1 ();
}
