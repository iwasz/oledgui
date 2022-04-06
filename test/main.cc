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
#include <type_traits>
#include <utility>

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
                // currentFocus %= mainWidget..widgetCount;
                // constexpr int ccc = mainWidget.widgetCount;

                if (currentFocus < mainWidget.widgetCount - 1) {
                        ++currentFocus;
                        mainWidget.reFocus (*this);
                }
        }

        void decrementFocus (auto const &mainWidget)
        {
                // if (--currentFocus < 0) {
                //         currentFocus = mainWidget.widgetCount - 1;
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
        // menu / list
        // combo - "action" key changes the value (works for numbers as well)
        // icon aka indicator aka animation (icon with states)
        // Button (with callback).

        // std::string - like strings??? naah...
        // callbacks and / or references
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
                // if (!detail::heightsOverlap (y, height, d.currentScroll, d.height)) {
                //         return;
                // }

                detail::line (d, len);
        }
        static constexpr int widgetCount = 0;
        static constexpr int height = 1;

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
        constexpr Check (const char *s, bool c) : label{s}, checked{c} {}

        // TODO move outside the class.
        // TODO remove groupIndex and groupSelection. Not needed here.
        bool operator() (auto &d, int focusIndex, int groupIndex, int groupSelection) const
        {
                if (!detail::heightsOverlap (y, height, d.currentScroll, d.height)) {
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

                mvwprintw (d.win, d.y, d.x, label);

                if (d.currentFocus == focusIndex) {
                        wattroff (d.win, COLOR_PAIR (1));
                }

                d.x += strlen (label);
                return true;
        }

        void reFocus (auto &d, int focusIndex = 0) const
        {
                if (!detail::heightsOverlap (y, height, d.currentScroll, d.height)) {
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

        void input (auto &d, char c, int focusIndex, int /* groupIndex */, int & /* groupSelection */)
        {
                if (d.currentFocus == focusIndex && c == ' ') { // TODO character must be customizable (compile time)
                        checked = !checked;
                }
        }

        static constexpr int widgetCount = 1;
        static constexpr int height = 1;

        // static constexpr int getY () { return 1; }
        int getY () const { return y; }
        int y{};

        const char *label{};
        bool checked{};
};

constexpr Check check (const char *str, bool checked = false) { return {str, checked}; }

/*--------------------------------------------------------------------------*/

struct Radio {

        constexpr Radio (const char *l) : label{l} {}
        bool operator() (auto &d, int focusIndex, int groupIndex, int groupSelection) const;
        void reFocus (auto &d, int focusIndex = 0) const;
        void input (auto &d, char c, int focusIndex, int groupIndex, int &groupSelection);

        const char *label;
        static constexpr int widgetCount = 1;
        static constexpr int height = 1;
        int getY () const { return y; }
        int y{};
};

bool Radio::operator() (auto &d, int focusIndex, int groupIndex, int groupSelection) const
{
        if (!detail::heightsOverlap (y, height, d.currentScroll, d.height)) {
                return false;
        }

        if (groupIndex == groupSelection) {
                mvwprintw (d.win, d.y, d.x, "◉");
        }
        else {
                mvwprintw (d.win, d.y, d.x, "○");
        }

        ++d.x;

        if (d.currentFocus == focusIndex) {
                wattron (d.win, COLOR_PAIR (1));
        }

        mvwprintw (d.win, d.y, d.x, label);

        if (d.currentFocus == focusIndex) {
                wattroff (d.win, COLOR_PAIR (1));
        }

        d.x += strlen (label);
        return true;
}

void Radio::reFocus (auto &d, int focusIndex) const
{
        // if (!detail::heightsOverlap (y, height, d.currentScroll, d.height)) {
        //         if (d.currentFocus == focusIndex) {
        //                 if (y < d.currentScroll) {
        //                         d.currentScroll = y;
        //                 }
        //                 else {
        //                         d.currentScroll = y - d.height + 1;
        //                 }
        //         }
        // }
}

void Radio::input (auto &d, char c, int focusIndex, int groupIndex, int &groupSelection)
{
        if (d.currentFocus == focusIndex && c == ' ') { // TODO character must be customizable (compile time)
                groupSelection = groupIndex;
        }
}

constexpr Radio radio (const char *label) { return {label}; }

// template <typename Tuple> struct Group {

//         constexpr Group (Tuple t) : radios{std::move (t)} {}

//         Tuple radios;
// };

// template <typename... R> constexpr auto group (R const &...r) { return Group{std::tuple{r...}}; }

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
        void layout (Disp &d, int focusIndex, int groupIndex, int groupSelection, W const &widget, Rst const &...widgets)
        {
                if (widget (d, focusIndex, groupIndex, groupSelection)) {
                        Layout::after (d);
                }

                if constexpr (sizeof...(widgets) > 0) {
                        layout<Layout> (d, focusIndex + widget.widgetCount, groupIndex + 1, groupSelection, widgets...);
                }
        }

        template <typename T> struct WidgetCountField {
                static constexpr int value = T::widgetCount;
        };

        template <typename T> struct WidgetHeightField {
                static constexpr int value = T::height;
        };

        template <typename Tuple, template <typename T> typename Field, int n = std::tuple_size_v<Tuple> - 1> struct Sum {
                static constexpr auto value = Field<std::tuple_element_t<n, Tuple>>::value + Sum<Tuple, Field, n - 1>::value;
        };

        template <typename Tuple, template <typename T> typename Field> struct Sum<Tuple, Field, 0> {
                static constexpr int value = Field<std::tuple_element_t<0, Tuple>>::value;
        };

} // namespace detail

template <typename Decor, typename WidgetsTuple> struct Layout {

        static constexpr int widgetCount = detail::Sum<WidgetsTuple, detail::WidgetCountField>::value;
        static constexpr int height = detail::Sum<WidgetsTuple, detail::WidgetHeightField>::value;

        Layout (WidgetsTuple w) : widgets (std::move (w)) {}

        bool operator() (auto &d, int focusIndex = 0, int /* groupIndex */ = 0, int /* groupSelection */ = 0) const
        {
                if (!detail::heightsOverlap (y, height, d.currentScroll, d.height)) {
                        return false;
                }

                std::apply ([&d, &focusIndex, groupSelection = groupSelection] (
                                    auto const &...widgets) { detail::layout<Decor> (d, focusIndex, 0, groupSelection, widgets...); },
                            widgets);
                return true;
        }

        void reFocus (auto &d, int focusIndex = 0) const
        {
                auto l = [&d] (auto &itself, int focusIndex, auto const &widget, auto const &...widgets) {
                        widget.reFocus (d, focusIndex);

                        if constexpr (sizeof...(widgets)) {
                                itself (itself, focusIndex + widget.widgetCount, widgets...);
                        }
                };

                std::apply ([focusIndex, &l] (auto const &...widgets) { l (l, focusIndex, widgets...); }, widgets);
        }

        void input (auto &d, char c, int focusIndex, int /* groupIndex */, int & /* groupSelection */)
        {
                auto &groupSelectionReference = groupSelection;
                auto l = [&d, c, &groupSelectionReference] (auto &itself, int focusIndex, int groupIndex, auto &widget, auto &...widgets) {
                        widget.input (d, c, focusIndex, groupIndex, groupSelectionReference);

                        if constexpr (sizeof...(widgets)) {
                                itself (itself, focusIndex + widget.widgetCount, groupIndex + 1, widgets...);
                        }
                };

                std::apply ([focusIndex, &l] (auto &...widgets) { l (l, focusIndex, 0, widgets...); }, widgets);
        }

        void calculatePositions ()
        {
                auto l = [] (auto &itself, int prevY, int prevH, auto &widget, auto &...widgets) {
                        widget.y = prevY + prevH; // First statement is an equivalent to : widget[0].y = y
                        std::cout << widget.y << std::endl;

                        if constexpr (sizeof...(widgets) > 0) {
                                // Next statements : widget[n].y = widget[n-1].y + widget[n-1].height
                                itself (itself, widget.y, widget.height, widgets...);
                        }
                };

                std::apply ([&l, y = this->y] (auto &...widgets) { l (l, y, 0, widgets...); }, widgets);
        }

        int getY () const { return y; }
        int y{};
        int groupSelection{};

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
        auto vb = vbox (vbox (radio (" A "), radio (" B "), radio (" C "), radio (" d ")), //
                        vbox (check (" 1 "), check (" 2 "), check (" 3 "), check (" 4 ")), //
                        vbox (check (" a "), check (" b "), check (" c "), check (" d ")), //
                        vbox (check (" 5 "), check (" 6 "), check (" 7 "), check (" 8 ")), //
                        vbox (check (" E "), check (" F "), check (" G "), check (" H "))  //
        );                                                                                 //

        // auto vb = vbox (check (" 1"), check (" 2"), check (" 3"), check (" 4"), check (" 5"), check (" 6"), check (" 7"), check (" 8"),
        //                 check (" 9"), check (" 10"), check (" 11"), check (" 12"), check (" 13"), check (" 14"), check (" 15"),
        //                 check (" 16-last"));

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
                        int dummy;                             // TODO ugly
                        vb.input (d1, char (ch), 0, 0, dummy); // TODO ugly, should be vb.input (d1, char (ch)) at most
                        break;
                }
        }

        clrtoeol ();
        refresh ();
        endwin (); /* End curses mode		  */
        return 0;
}
