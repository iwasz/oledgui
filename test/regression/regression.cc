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
#include "cellPhone.h"
#include "inputApi.h"
#include "layout.h"
#include "number.h"
#include "oledgui/debug.h"
#include "oledgui/ncurses.h"
#include "textWidget.h"

using namespace og;
using namespace std::string_view_literals;

enum class Windows { menu, allFeatures, dialog, cellPhone, layouts, number };
ISuite<Windows> *mySuiteP{};

/****************************************************************************/

int main ()
{

        NcursesDisplay<18, 7> d1;

        auto menu = window<0, 0, 18, 7> (vbox (label ("----Main menu-----"sv),
                                               button ("input API  "sv, [] { mySuiteP->current () = Windows::allFeatures; }),
                                               button ("text widget"sv, [] { mySuiteP->current () = Windows::dialog; }),
                                               button ("cell phone "sv, [] { mySuiteP->current () = Windows::cellPhone; }),
                                               button ("Layouts    "sv, [] { mySuiteP->current () = Windows::layouts; }), //
                                               button ("Numbers    "sv, [] { mySuiteP->current () = Windows::number; })   //
                                               ));

        auto mySuite = suite<Windows> (element (Windows::menu, std::ref (menu)),               //
                                       element (Windows::allFeatures, std::ref (inputApi ())), //
                                       element (Windows::dialog, std::ref (textWidget ())),    //
                                       element (Windows::cellPhone, std::ref (cellPhone ())),  //
                                       element (Windows::layouts, std::ref (layout ())),       //
                                       element (Windows::number, std::ref (number ())));
        mySuiteP = &mySuite;

        while (true) {
                draw (d1, mySuite);
                input (d1, mySuite, getKey ());
        }

        return 0;
}

/// Global functions help to implement "go back to main" menu button
void mainMenu () { mySuiteP->current () = Windows::menu; }