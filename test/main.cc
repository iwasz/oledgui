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
        static constexpr uint16_t width = 18; // Dimensions in charcters
        static constexpr uint16_t height = 7; // Dimensions in charcters
        int currentFocus = 0;

        WINDOW *win{};

        void incrementFocus (auto const &mainWidget)
        {
                ++currentFocus;
                currentFocus %= mainWidget.getFocusIncrement ();
        }

        void decrementFocus (auto const &mainWidget)
        {
                if (--currentFocus < 0) {
                        currentFocus = mainWidget.getFocusIncrement () - 1;
                }
        }

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
                        mvwprintw (d.win, d.y, d.x++, "─");
                }
        }
} // namespace detail

/*--------------------------------------------------------------------------*/

struct Line {
        void operator() (auto &d, int /*focusIndex*/ = 0) const { detail::line (d, len); }
        static constexpr int getFocusIncrement () { return 0; }

        uint16_t len;
};

auto line (uint16_t len = Display::width)
{
        // return [len] (auto &d) { detail::line (d, len); };
        return Line{len};
}

/*--------------------------------------------------------------------------*/

auto dialog (const char *str)
{
        return [str] (auto &d) {
                uint16_t len = strlen (str);

                mvwprintw (d.win, d.y, d.x++, "┌");
                detail::line (d, len);
                mvwprintw (d.win, d.y, d.x++, "┐");
                ++d.y;
                d.x = 0;

                mvwprintw (d.win, d.y, d.x, "│");
                ++d.x;

                mvwprintw (d.win, d.y, d.x, str);
                d.x += strlen (str);

                mvwprintw (d.win, d.y++, d.x, "│");
                d.x = 0;

                mvwprintw (d.win, d.y, d.x++, "└");
                detail::line (d, len);
                mvwprintw (d.win, d.y, d.x++, "┘");
        };
}

/*--------------------------------------------------------------------------*/

struct Check {
        template <typename Disp> void operator() (Disp &d, int focusIndex = 0) const
        {
                if (checked) {
                        mvwprintw (d.win, d.y, d.x, "☑");
                }
                else {
                        mvwprintw (d.win, d.y, d.x, "☐");
                }

                ++d.x;

                if (d.currentFocus == focusIndex) {
                        wattron (d.win, COLOR_PAIR (1));
                }

                mvwprintw (d.win, d.y, d.x, str);

                if (d.currentFocus == focusIndex) {
                        wattroff (d.win, COLOR_PAIR (1));
                }

                d.x += strlen (str);
        }

        constexpr int getFocusIncrement () const { return 1; }

        const char *str{};
        bool checked{};
};

Check check (const char *str, bool checked = false) { return {str, checked}; }

/*--------------------------------------------------------------------------*/

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

        template <typename W, typename... Rst> int getFocusIncrement (W const &widget, Rst const &...widgets)
        {
                if constexpr (sizeof...(widgets)) {
                        return widget.getFocusIncrement () + getFocusIncrement (widgets...);
                }
                else {
                        return widget.getFocusIncrement ();
                }
        }

} // namespace detail

template <typename Decor, typename WidgetsTuple> struct Layout {

        Layout (WidgetsTuple w) : widgets (std::move (w)) {}

        void operator() (auto &d, int focusIndex = 0) const
        {
                std::apply ([&d, &focusIndex] (auto const &...widgets) { detail::layout<Decor> (d, focusIndex, widgets...); }, widgets);
        }

        constexpr int getFocusIncrement () const
        {
                return std::apply ([] (auto const &...widgets) { return detail::getFocusIncrement (widgets...); }, widgets);
        }

        WidgetsTuple widgets;
};

template <typename... W> auto vbox (W const &...widgets)
{
        auto vbox = Layout<detail::VBoxDecoration, decltype (std::tuple (widgets...))>{std::tuple (widgets...)};
        return vbox;
}

template <typename... W> auto hbox (W const &...widgets)
{
        auto hbox = Layout<detail::HBoxDecoration, decltype (std::tuple (widgets...))>{std::tuple (widgets...)};
        return hbox;
}

/*--------------------------------------------------------------------------*/

auto label (const char *str)
{
        return [str] (auto &d, int focusIndex = -1) {
                mvwprintw (d.win, d.y, d.x, str);
                d.x += strlen (str);
        };
}

} // namespace og

int main ()
{
        // auto p;

        using namespace og;
        Display d1;
        // auto contents = d1.group (1, radio ("red"), radio ("green"), radio ("blue")); // "green is selected"
        // contents ();

        setlocale (LC_ALL, "");
        initscr (); /* Start curses mode 		  */
        curs_set (0);
        noecho ();
        cbreak ();
        use_default_colors ();
        start_color (); /* Start color 			*/
        init_pair (1, COLOR_RED, -1);

        d1.win = newwin (d1.height, d1.width, 0, 0);
        refresh ();

        // hbox (dialog (" Pin:668543 "), line (), check (true, " A"), check (true, " B"), check (false, " C")) (d1);
        // hbox (dialog (" Pin:668543 "), line (), check (true, " A"), check (true, " B"), check (false, " C")) (d1);

        auto vb = hbox (vbox (hbox (check (" A "), check (" B "), check (" C ")),  //
                              line (),                                             //
                              hbox (check (" D "), check (" e "), check (" f ")),  //
                              line (),                                             //
                              hbox (check (" G "), check (" H "), check (" I "))), //
                                                                                   //
                        vbox (hbox (check (" 1 "), check (" 2 "), check (" 3 ")),  //
                              line (),                                             //
                              hbox (check (" 4 "), check (" 5 "), check (" 6 ")),  //
                              line (),                                             //
                              hbox (check (" 7 "), check (" 8 "), check (" 9 "))));

        // auto menu = vbox (label ("1"), label ("2"), label ("3"));

        // menu (d1);

        while (true) {
                wclear (d1.win);
                d1.x = 0;
                d1.y = 0;
                vb (d1);
                wrefresh (d1.win);
                int ch = getch ();

                if (ch == 'q') {
                        break;
                }

                switch (ch) {
                case 's':
                        d1.incrementFocus (vb);
                        break;

                case 'w':
                        d1.decrementFocus (vb);
                        break;

                default:
                        break;
                }
        }

        clrtoeol ();
        refresh ();
        endwin (); /* End curses mode		  */
        return 0;
}
