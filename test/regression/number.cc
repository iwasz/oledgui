/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "number.h"
#include "customStyles.h"
#include "regression.h"
#include "utils.h"

namespace {
using namespace og;
using namespace std::string_view_literals;

enum class Windows {
        menu,
        integer,
        compositeInt,
        someUseCases,
};

ISuite<Windows> *mySuite{};
std::string chkBxLabel = "-";
std::string rdoBxLabel = "-";
std::string cboLabel = "-";
std::string buttonLabel = "-";
int buttonCnt{};

auto menu = window<0, 0, 18, 7> (vbox (button ([] { mainMenu (); }, "[back]"sv),                          //
                                       group ([] (Windows s) { mySuite->current () = s; }, Windows::menu, //
                                              item (Windows::integer, "Simple integer"sv),                //
                                              item (Windows::compositeInt, "Composite integer"sv),        //
                                              item (Windows::someUseCases, "Some use cases"sv)            //
                                              )));

auto backButton = button ([] { mySuite->current () = Windows::menu; }, "[back]"sv);

uint8_t sharedVal{};

struct FocusEnabled {
        static constexpr auto focus = style::Focus::enabled;
};

struct FocusDisabled {
        static constexpr auto focus = style::Focus::disabled;
};

/**
 * All available inputs with callbacks.
 */
auto integer = window<0, 0, 18, 7> (vbox (
        std::ref (backButton),                                                              //
        hbox (label ("8b, 0-9(1): "sv), og::number (uint8_t (0))),                          // No callback, only initial value
        hbox (label ("8b, 0-40(5): "sv), og::number<0, 40, 5, uint8_t> ([] (auto val) {})), // No initial value, only callback
        // hbox (label ("8b, 0-40(5): "sv), og::number<0, 40, 5> ([] (auto val) {})),                        // No initial value, only callback
        hbox (label ("16b: "sv), og::number<-30000, 29999, 1000, FocusEnabled> ([] (auto val) {}, int16_t (-30000))), //
        hbox (label ("Shared: "sv), og::number (std::ref (sharedVal)), og::hspace<1>,
              og::number (std::ref (sharedVal))) //
        ));

/*--------------------------------------------------------------------------*/

std::array<uint8_t, 4> digits{};
int glued{};

auto glueNumber = [] (uint8_t d) {
        glued = 0;

        for (int i = 0; i < digits.size (); ++i) {
                glued += og::detail::pow (10, i) * digits.at (digits.size () - i - 1);
        }
};

/*--------------------------------------------------------------------------*/

auto compositeInt = window<0, 0, 18, 7> (vbox (std::ref (backButton),                                                //
                                               label ("4 digit PIN."sv),                                             //
                                               label ("Change each digit"sv),                                        //
                                               label ("separately"sv),                                               //
                                               hbox (number<0, 9, 1> (glueNumber, std::ref (std::get<0> (digits))),  // digit 0
                                                     number<0, 9, 1> (glueNumber, std::ref (std::get<1> (digits))),  // digit 1
                                                     number<0, 9, 1> (glueNumber, std::ref (std::get<2> (digits))),  // digit 2
                                                     number<0, 9, 1> (glueNumber, std::ref (std::get<3> (digits)))), // digit 3
                                               hbox (label ("Number: "sv), number<0, 9999, 1, FocusDisabled> (std::ref (glued)))));

/*--------------------------------------------------------------------------*/

Cfg<int> cfg{glued};

auto someUseCases = window<0, 0, 18, 7> (vbox (
        std::ref (backButton),                                                                                                                //
        hbox (label ("Clbck, 4 templ: "sv), og::number<0, 40, 5, int> ([] (auto val) {})),                                                    //
        hbox (label ("Custom type: "sv), og::number<0, 40, 5> (std::ref (cfg)), hspace<1>, number<0, 0, 0, FocusDisabled> (std::ref (glued))) //
        ));

/*--------------------------------------------------------------------------*/

auto s = suite<Windows> (element (Windows::menu, std::ref (menu)),                 //
                         element (Windows::integer, std::ref (integer)),           //
                         element (Windows::compositeInt, std::ref (compositeInt)), //
                         element (Windows::someUseCases, std::ref (someUseCases)));

} // namespace

og::detail::augment::IWindow &number ()
{
        mySuite = &s;
        return s;
}