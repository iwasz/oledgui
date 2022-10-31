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
#include "customStyles.h"
#include "inputApi.h"
#include "layout.h"
#include "number.h"
#include "oledgui/debug.h"
#include "oledgui/ncurses.h"
#include "progress.h"
#include "styles.h"
#include "textWidget.h"

using namespace og;
using namespace std::string_view_literals;

enum class Windows { menu, allFeatures, dialog, cellPhone, layouts, number, progress, styles };
ISuite<Windows> *mySuiteP{};

/****************************************************************************/

int main ()
{
        NcursesDisplay<18, 7> d1;

        auto menu = window<0, 0, 18, 7> (vbox (label ("----Main menu-----"sv),
                                               button ([] { mySuiteP->current () = Windows::allFeatures; }, "1. Input API   "sv),
                                               button ([] { mySuiteP->current () = Windows::dialog; }, "2. Text widget "sv),
                                               button ([] { mySuiteP->current () = Windows::cellPhone; }, "3. Cell phone  "sv),
                                               button ([] { mySuiteP->current () = Windows::layouts; }, "4. Layouts     "sv),  //
                                               button ([] { mySuiteP->current () = Windows::number; }, "5. Numbers     "sv),   //
                                               button ([] { mySuiteP->current () = Windows::progress; }, "6. Progress bar"sv), //
                                               button ([] { mySuiteP->current () = Windows::styles; }, "7. Styles      "sv)    //
                                               ));

        auto mySuite = suite<Windows> (element (Windows::menu, std::ref (menu)),               //
                                       element (Windows::allFeatures, std::ref (inputApi ())), //
                                       element (Windows::dialog, std::ref (textWidget ())),    //
                                       element (Windows::cellPhone, std::ref (cellPhone ())),  //
                                       element (Windows::layouts, std::ref (layout ())),       //
                                       element (Windows::number, std::ref (number ())),        //
                                       element (Windows::progress, std::ref (progress ())),    //
                                       element (Windows::styles, std::ref (styles ())));
        mySuiteP = &mySuite;

        while (true) {
                draw (d1, mySuite);
                input (d1, mySuite, getKey ());
        }

        return 0;
}

/// Global functions help to implement "go back to main" menu button
void mainMenu () { mySuiteP->current () = Windows::menu; }