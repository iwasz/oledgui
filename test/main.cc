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
        int currentFocus{};
        int currentScroll{};

        WINDOW *win{};

        void incrementFocus (auto const &mainWidget)
        {
                // ++currentFocus;
                // currentFocus %= mainWidget.getFocusIncrement ();

                if (currentFocus < mainWidget.getFocusIncrement () - 1) {
                        ++currentFocus;
                        mainWidget.reFocus (*this);
                }
        }

        void decrementFocus (auto const &mainWidget)
        {
                // if (--currentFocus < 0) {
                //         currentFocus = mainWidget.getFocusIncrement () - 1;
                // }

                if (currentFocus > 0) {
                        --currentFocus;
                        mainWidget.reFocus (*this);
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

namespace detail {
        constexpr bool heightsOverlap (int y1, int height1, int y2, int height2)
        {
                auto y1d = y1 + height1 - 1;
                auto y2d = y2 + height2 - 1;
                return y1 <= y2d && y2 <= y1d;
        }

        static_assert (!heightsOverlap (-1, 1, 0, 2));
        static_assert (heightsOverlap (0, 1, 0, 2));
        static_assert (heightsOverlap (1, 1, 0, 2));
        static_assert (!heightsOverlap (2, 1, 0, 2));

} // namespace detail

/*--------------------------------------------------------------------------*/

struct Line {
        void operator() (auto &d, int /*focusIndex*/ = 0) const
        {
                // if (!detail::heightsOverlap (y, getHeight (), d.currentScroll, d.height)) {
                //         return;
                // }

                detail::line (d, len);
        }
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
        Check (const char *s, bool c) : str{s}, checked{c} {}

        bool operator() (auto &d, int focusIndex = 0) const
        {
                if (!detail::heightsOverlap (y, getHeight (), d.currentScroll, d.height)) {
                        return false;
                }

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
                return true;
        }

        void reFocus (auto &d, int focusIndex = 0) const
        {
                if (!detail::heightsOverlap (y, getHeight (), d.currentScroll, d.height)) {
                        if (d.currentFocus == focusIndex) {
                                if (y < d.currentScroll) {
                                        d.currentScroll = y;
                                }
                                else {
                                        d.currentScroll = y - d.height + 1;
                                }
                        }
                }
        }

        void input (auto &d, char c, int focusIndex = 0)
        {
                if (d.currentFocus == focusIndex && c == ' ') { // TODO character must be customizable (compile time)
                        checked = !checked;
                }
        }

        static constexpr int getFocusIncrement () { return 1; }
        static constexpr int getHeight () { return 1; }

        // static constexpr int getY () { return 1; }
        int getY () const { return y; }
        int y{};

        const char *str{};
        bool checked{};
};

Check check (const char *str, bool checked = false) { return {str, checked}; }

/*--------------------------------------------------------------------------*/

namespace detail {

        struct VBoxDecoration {
                static void after (auto &d)
                {
                        d.y += 1; // TODO generates empty lines when more than 1 vbox is nested
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
                if (widget (d, focusIndex)) {
                        Layout::after (d);
                }

                if constexpr (sizeof...(widgets) > 0) {
                        layout<Layout> (d, focusIndex + widget.getFocusIncrement (), widgets...);
                }
        }

        template <typename W, typename... Rst> static constexpr int getFocusIncrement (W const &widget, Rst const &...widgets)
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

        bool operator() (auto &d, int focusIndex = 0) const
        {
                if (!detail::heightsOverlap (y, getHeight (), d.currentScroll, d.height)) {
                        return false;
                }

                std::apply ([&d, &focusIndex] (auto const &...widgets) { detail::layout<Decor> (d, focusIndex, widgets...); }, widgets);
                return true;
        }

        void reFocus (auto &d, int focusIndex = 0) const
        {
                auto l = [&d] (auto &itself, int focusIndex, auto const &widget, auto const &...widgets) {
                        widget.reFocus (d, focusIndex);

                        if constexpr (sizeof...(widgets)) {
                                itself (itself, focusIndex + widget.getFocusIncrement (), widgets...);
                        }
                };

                std::apply ([focusIndex, &l] (auto const &...widgets) { l (l, focusIndex, widgets...); }, widgets);
        }

        void input (auto &d, char c, int focusIndex = 0)
        {
                auto l = [&d, c] (auto &itself, int focusIndex, auto &widget, auto &...widgets) {
                        widget.input (d, c, focusIndex);

                        if constexpr (sizeof...(widgets)) {
                                itself (itself, focusIndex + widget.getFocusIncrement (), widgets...);
                        }
                };

                std::apply ([focusIndex, &l] (auto &...widgets) { l (l, focusIndex, widgets...); }, widgets);
        }

        constexpr int getFocusIncrement () const
        {
                return std::apply ([] (auto const &...widgets) { return detail::getFocusIncrement (widgets...); }, widgets);
        }

        constexpr int getHeight () const
        {
                auto l = [] (auto &itself, auto const &widget, auto const &...widgets) -> int {
                        if constexpr (sizeof...(widgets) > 0) {
                                return widget.getHeight () + itself (itself, widgets...);
                        }

                        return widget.getHeight ();
                };

                return std::apply ([&l] (auto const &...widgets) { return l (l, widgets...); }, widgets);
        }

        void calculatePositions ()
        {
                auto l = [] (auto &itself, int prevY, int prevH, auto &widget, auto &...widgets) {
                        widget.y = prevY + prevH; // First statement is an equivalent to : widget[0].y = y
                        std::cout << widget.y << std::endl;

                        if constexpr (sizeof...(widgets) > 0) {
                                // Next statements : widget[n].y = widget[n-1].y + widget[n-1].getHeight
                                itself (itself, widget.y, widget.getHeight (), widgets...);
                        }
                };

                std::apply ([&l, y = this->y] (auto &...widgets) { l (l, y, 0, widgets...); }, widgets);
        }

        int getY () const { return y; }
        int y{};

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
        return [str] (auto &d, int focusIndex = 0) {
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

        setlocale (LC_ALL, "");
        initscr (); /* Start curses mode 		  */
        curs_set (0);
        noecho ();
        cbreak ();
        use_default_colors ();
        start_color (); /* Start color 			*/
        init_pair (1, COLOR_RED, -1);
        keypad (stdscr, true);
        d1.win = newwin (d1.height, d1.width, 0, 0);
        refresh ();

        // hbox (dialog (" Pin:668543 "), line (), check (true, " A"), check (true, " B"), check (false, " C")) (d1);
        // hbox (dialog (" Pin:668543 "), line (), check (true, " A"), check (true, " B"), check (false, " C")) (d1);

        // auto vb = hbox (vbox (hbox (check (" A "), check (" B "), check (" C ")),  //
        //                       line (),                                             //
        //                       hbox (check (" D "), check (" e "), check (" f ")),  //
        //                       line (),                                             //
        //                       hbox (check (" G "), check (" H "), check (" I "))), //
        //                                                                            //
        //                 vbox (hbox (check (" 1 "), check (" 2 "), check (" 3 ")),  //
        //                       line (),                                             //
        //                       hbox (check (" 4 "), check (" 5 "), check (" 6 ")),  //
        //                       line (),                                             //
        //                       hbox (check (" 7 "), check (" 8 "), check (" 9 "))));

        // vb.calculatePositions ();

        // TODO this case has a flaw that empty lines are insered when onbe of the nested vboxes connects with the next.
        // auto vb = vbox (vbox (check (" A "), check (" B "), check (" C "), check (" d ")), //
        //                 vbox (check (" 1 "), check (" 2 "), check (" 3 "), check (" 4 ")), //
        //                 vbox (check (" a "), check (" b "), check (" c "), check (" d ")),
        //                 vbox (check (" 5 "), check (" 6 "), check (" 7 "), check (" 8 ")),
        //                 vbox (check (" E "), check (" F "), check (" G "), check (" H "))

        // ); //

        auto vb = vbox (check (" 1"), check (" 2"), check (" 3"), check (" 4"), check (" 5"), check (" 6"), check (" 7"), check (" 8"),
                        check (" 9"), check (" 10"), check (" 11"), check (" 12"), check (" 13"), check (" 14"), check (" 15"),
                        check (" 16-last"));

        vb.calculatePositions (); // Only once. After composition

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
                case KEY_DOWN:
                        d1.incrementFocus (vb);
                        break;

                case KEY_UP:
                        d1.decrementFocus (vb);
                        break;

                default:
                        // d1.input (vb, char (ch));
                        vb.input (d1, char (ch));
                        break;
                }
        }

        clrtoeol ();
        refresh ();
        endwin (); /* End curses mode		  */
        return 0;
}
