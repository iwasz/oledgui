/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "oledgui/ncurses.h"

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

int test2 ()
{
        using namespace og;
        using namespace std::string_view_literals;

        NcursesDisplay<18, 7> d1;

        bool showDialog{};

        // auto x = window<0, 0, 18, 7> (
        //         vbox (hbox (label ("Hello "), check (" 1 "), check (" 2 ")),                                                        //
        //               hbox (label ("World "), check (" 5 "), check (" 6 ")),                                                        //
        //               button ("Open dialog", [&showDialog] { showDialog = true; }),                                                 //
        //               line<18>,                                                                                                     //
        //               group ([] (auto const &o) {}, radio (0, " R "), radio (1, " G "), radio (1, " B "), radio (1, " A ")),        //
        //               line<18>,                                                                                                     //
        //               hbox (group ([] (auto const &o) {}, radio (0, " R "), radio (1, " G "), radio (1, " B "), radio (1, " A "))), //
        //               line<18>,                                                                                                     //
        //               Combo (Options (option (0, "red"), option (1, "green"), option (1, "blue")), [] (auto const &o) {}),          //
        //               line<18>,                                                                                                     //
        //               hbox (button ("Aaa", [] {}), hspace<1>, button ("Bbb", [] {}), hspace<1>, button ("Ccc", [] {})),             //
        //               line<18>,                                                                                                     //
        //               check (" 1 "),                                                                                                //
        //               check (" 2 "),                                                                                                //
        //               check (" 3 "),                                                                                                //
        //               check (" 4 "),                                                                                                //
        //               check (" 5 "),                                                                                                //
        //               check (" 6 "),                                                                                                //
        //               check (" 7 "),                                                                                                //
        //               check (" 8 "),                                                                                                //
        //               check (" 9 "),                                                                                                //
        //               check (" 10 "),                                                                                               //
        //               check (" 11 "),                                                                                               //
        //               check (" 12 "),                                                                                               //
        //               check (" 13 "),                                                                                               //
        //               check (" 14 "),                                                                                               //
        //               check (" 15 ")                                                                                                //
        //               ));                                                                                                           //

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

        auto x = window<0, 0, 18, 7> (std::ref (txtComp));
        // log (x);

        /*--------------------------------------------------------------------------*/

        auto v = vbox (label ("  PIN:"sv), label (" 123456"sv),
                       hbox (button ("[OK]"sv, [&showDialog] { showDialog = false; }), button ("[Cl]"sv, [] {})), check (" 15 "sv));

        auto dialog = window<4, 1, 10, 5, true> (std::ref (v));

        // log (dialog);

        while (true) {
                if (showDialog) {
                        draw (d1, x, dialog);
                        input (d1, dialog, getKey ()); // Blocking call.
                }
                else {
                        draw (d1, x);
                        int ch = getch ();

                        switch (ch) {
                        case 'w':
                                // startLine = txt.setStartLine (--startLine);
                                --x.context.currentScroll;
                                break;
                        case 's':
                                // startLine = txt.setStartLine (++startLine);
                                ++x.context.currentScroll;
                                break;
                        case 'a':
                                buff = "ala ma kota";
                                break;
                        case 'c':
                                chk.checked () = !chk.checked ();
                                break;
                        default:
                                input (d1, x, getKey (ch));
                                break;
                        }
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
