/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "ncurses.h"
#include "oledgui.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <tuple>

namespace og {

class Display {
public:
        uint16_t x{};                         /// Current cursor position [in characters]
        uint16_t y{};                         /// Current cursor position [in characters]
        static constexpr uint16_t width = 16; // Dimensions in charcters
        static constexpr uint16_t height = 8; // Dimensions in charcters

        // screen wide/tall
        //  current focus
        //  scrolling container
        //  checkboxes
        //  radiobutons (no state)
        //  radiobutton group (still no state, but somehow manages the radios. Maybe integer?)
        //  menu
        // icon aka indicator

        // std::string - like strings??? naah...
};

// void move (uint16_t x, uint16_t y)
// {
//         d1.x = x;
//         d1.y = y;
// }

namespace detail {
        void line (auto &d, uint16_t len)
        {
                for (uint16_t i = 0; i < len; ++i) {
                        mvprintw (d.y, d.x++, "─");
                }
        }
} // namespace detail

auto line (uint16_t len = Display::width)
{
        return [len] (auto &d) { detail::line (d, len); };
}

auto dialog (const char *str)
{
        return [str] (auto &d) {
                uint16_t len = strlen (str);

                mvprintw (d.y, d.x++, "┌");
                detail::line (d, len);
                mvprintw (d.y, d.x++, "┐");
                ++d.y;
                d.x = 0;

                mvprintw (d.y, d.x, "│");
                ++d.x;

                mvprintw (d.y, d.x, str);
                d.x += strlen (str);

                mvprintw (d.y++, d.x, "│");
                d.x = 0;

                mvprintw (d.y, d.x++, "└");
                detail::line (d, len);
                mvprintw (d.y, d.x++, "┘");
        };
}

struct Check {
        template <typename Disp> void operator() (Disp &d) const
        {
                if (checked) {
                        mvprintw (d.y, d.x, "☑");
                }
                else {
                        mvprintw (d.y, d.x, "☐");
                }

                ++d.x;
                mvprintw (d.y, d.x, str);
                d.x += strlen (str);
        }

        bool checked{};
        const char *str{};
};

Check check (bool checked, const char *str) { return {checked, str}; }

namespace detail {

        template <typename Disp, typename W, typename... Rst> void vboxPrint (Disp &d, W const &widget, Rst const &...widgets)
        {
                widget (d);
                d.y += 1;
                d.x = 0;

                if constexpr (sizeof...(widgets) > 0) {
                        vboxPrint (d, widgets...);
                }
        }

} // namespace detail

template <typename... W> auto vbox (W const &...widgets)
{
        auto vbox = [&] (auto &d) { detail::vboxPrint (d, widgets...); };
        return vbox;
}

template <typename... W> void hbox (W const &...widget)
{
        // check 1
        // d1.y = 0;
        // d1.x = 0;

        // check 2
        // d1.y = 1;
        // d1.x = 0;
}

} // namespace og

int main ()
{
        using namespace og;
        Display d1;
        // auto contents = d1.group (1, radio ("red"), radio ("green"), radio ("blue")); // "green is selected"
        // contents ();

        setlocale (LC_ALL, "");
        initscr (); /* Start curses mode 		  */

        // dialog (" Pin:668543 ");
        vbox (dialog (" Pin:668543 "), line (), check (true, " A"), check (true, " B"), check (false, " C")) (d1);

        refresh (); /* Print it on to the real screen */
        getch ();   /* Wait for user input */
        endwin ();  /* End curses mode		  */
        return 0;
}
