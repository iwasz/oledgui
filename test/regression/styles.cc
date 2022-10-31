/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "styles.h"
#include "customStyles.h"
#include "regression.h"

namespace {
using namespace og;
using namespace std::string_view_literals;

enum class Windows {
        menu,
        combo,
        check,
        radio,
};

ISuite<Windows> *mySuite{};
std::string chkBxLabel = "-";
std::string rdoBxLabel = "-";
std::string cboLabel = "-";
std::string buttonLabel = "-";
int buttonCnt{};

auto menu = window<0, 0, 18, 7> (vbox (button ([] { mainMenu (); }, "[back]"sv),                           //
                                       group ([] (Windows s) { mySuite->current () = s; }, Windows::combo, //
                                              item (Windows::combo, "Combo style"sv),                      //
                                              item (Windows::check, "Checkbox style"sv),                   //
                                              item (Windows::radio, "Radio style"sv)                       //
                                              )));

auto backButton = button ([] { mySuite->current () = Windows::menu; }, "[back]"sv);

/*--------------------------------------------------------------------------*/

struct CustomStyleNoFocus {
        static constexpr style::Focus focus = style::Focus::disabled;
};

struct CustomStyleNoEdit {
        static constexpr style::Editable editable = style::Editable::no;
};

auto combo
        = window<0, 0, 18, 7> (vbox (std::ref (backButton), //
                                     og::combo<CustomStyleNoFocus> (std::ref (buttonCnt), option (8, "combo no focus"sv), option (8, "blue"sv)),
                                     og::combo<CustomStyleNoEdit> (std::ref (buttonCnt), option (8, "combo no edit"sv), option (8, "blue"sv)),
                                     og::combo (std::ref (buttonCnt), option (0, "normal combo"sv), option (1, "that's right"sv))));

/*--------------------------------------------------------------------------*/

struct CustomStyleCheck {
        static constexpr style::Focus focus = style::Focus::disabled;
        static constexpr style::Editable editable = style::Editable::no;
};

auto checkbox = window<0, 0, 18, 7> (vbox (std::ref (backButton),         //
                                           check (true, " PR value "sv),  //
                                           line<18>,                      //
                                           check (true, " std::ref 1"sv), //
                                           line<18>,                      //
                                           check<CustomStyleCheck> (true, " std::ref 2"sv)));

/*--------------------------------------------------------------------------*/

int gid{2};

auto radio1 = window<0, 0, 18, 7> (vbox (std::ref (backButton), //
                                         hbox (group ([] (int) {}, 1, radio (0, " c "sv), radio (1, " m "sv), radio (2, " y "sv))),
                                         hbox (group (std::ref (gid), radio (0, " r "sv), radio (1, " g "sv), radio (2, " b "sv))),
                                         hbox (group ([] (int) {}, std::ref (gid), radio (0, " R "sv), radio (1, " G "sv), radio (2, " B "sv))))

);

/*--------------------------------------------------------------------------*/

auto s = suite<Windows> (element (Windows::menu, std::ref (menu)),      //
                         element (Windows::combo, std::ref (combo)),    //
                         element (Windows::check, std::ref (checkbox)), //
                         element (Windows::radio, std::ref (radio1)));

} // namespace

og::detail::augment::IWindow &styles ()
{
        mySuite = &s;
        return s;
}