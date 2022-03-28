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
        int currentFocus = 0;

        // screen wide/tall
        // current focus
        // text wrapping (without scrolling up - only for showinng bigger chunkgs of text, like logs)
        // scrolling container
        // checkboxes
        // radiobutons (no state)
        // radiobutton group (still no state, but somehow manages the radios. Maybe integer?)
        // menu
        // icon aka indicator aka animation (icon with states)

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
        template <typename Disp> void operator() (Disp &d, int focusIndex = 0) const
        {
                if (checked) {
                        mvprintw (d.y, d.x, "☑");
                }
                else {
                        mvprintw (d.y, d.x, "☐");
                }

                ++d.x;

                if (d.currentFocus == focusIndex) {
                        attron (COLOR_PAIR (1));
                }

                mvprintw (d.y, d.x, str);

                if (d.currentFocus == focusIndex) {
                        attroff (COLOR_PAIR (1));
                }

                d.x += strlen (str);
        }

        constexpr int getFocusIncrement () const { return 1; }

        bool checked{};
        const char *str{};
};

Check check (bool checked, const char *str) { return {checked, str}; }

namespace detail {

        struct VBoxDecoration {
                static void after (auto &d)
                {
                        d.y += 1;
                        d.x = 0;
                }
        };

        struct HBoxDecoration {
                static void after (auto &d)
                {
                        // d.y += 1;
                        // d.x = 0;
                }
        };

        template <typename Layout, typename Disp, typename W, typename... Rst>
        void layout (Disp &d, int focusIndex, W const &widget, Rst const &...widgets)
        {
                widget (d, focusIndex);
                Layout::after (d);

                if constexpr (sizeof...(widgets) > 0) {
                        layout<Layout> (d, focusIndex + widget.getFocusIncrement (), widgets...);
                }
        }

} // namespace detail

template <typename Decor, typename WidgetsTuple> struct Layout {

        Layout (WidgetsTuple w) : widgets (std::move (w)) {}

        void operator() (auto &d, int focusIndex = 0) const
        {
                std::apply ([&d, &focusIndex] (auto const &...widgets) { detail::layout<Decor> (d, focusIndex, widgets...); }, widgets);
        }

        constexpr int getFocusIncrement () const { return std::tuple_size<WidgetsTuple> (); }

        WidgetsTuple widgets;
};

template <typename... W> auto vbox (W const &...widgets)
{
        // auto vbox = [&] (auto &d, int focusIndex = 0) { detail::layout<detail::VBoxDecoration> (d, focusIndex, widgets...); };
        auto vbox = Layout<detail::VBoxDecoration, decltype (std::tuple (widgets...))>{std::tuple (widgets...)};
        return vbox;
}

template <typename... W> auto hbox (W const &...widgets)
{
        // auto hbox = [&] (auto &d, int focusIndex = 0) { detail::layout<detail::HBoxDecoration> (d, focusIndex, widgets...); };
        auto hbox = Layout<detail::HBoxDecoration, decltype (std::tuple (widgets...))>{std::tuple (widgets...)};
        return hbox;
}

auto label (const char *str)
{
        return [str] (auto &d, int focusIndex = -1) {
                mvprintw (d.y, d.x, str);
                d.x += strlen (str);
        };
}

} // namespace og

int main ()
{
        using namespace og;
        Display d1;
        d1.currentFocus = 3;
        // auto contents = d1.group (1, radio ("red"), radio ("green"), radio ("blue")); // "green is selected"
        // contents ();

        setlocale (LC_ALL, "");
        initscr (); /* Start curses mode 		  */
        use_default_colors ();
        start_color (); /* Start color 			*/
        init_pair (1, COLOR_RED, -1);

        // hbox (dialog (" Pin:668543 "), line (), check (true, " A"), check (true, " B"), check (false, " C")) (d1);
        // hbox (dialog (" Pin:668543 "), line (), check (true, " A"), check (true, " B"), check (false, " C")) (d1);

        auto vb = vbox (hbox (check (true, " A"), check (true, " B"), check (false, " C")),
                        hbox (check (true, " D"), check (true, " e"), check (false, " f")),
                        hbox (check (true, " G"), check (true, " H"), check (false, " I")));

        vb (d1);

        // auto menu = vbox (label ("1"), label ("2"), label ("3"));

        // menu (d1);

        refresh (); /* Print it on to the real screen */
        getch ();   /* Wait for user input */
        endwin ();  /* End curses mode		  */
        return 0;
}
