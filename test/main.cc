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

enum class Visibility {
        visible,    // Widget drawed something on the screen.
        outside,    // Widget is invisible because is outside the active region.
        nonDrawable // Widget is does not draw anything by itself.
};

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
};

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

template <uint16_t widgetCountV, uint16_t heightV> struct Widget {
        void reFocus (auto &d, int focusIndex = 0) const;
        void input (auto &d, char c, int focusIndex, int /* groupIndex */, int & /* groupSelection */) {}
        // TODO remove
        void calculatePositions (int) {}

        int y{};
        static constexpr int widgetCount = widgetCountV;
        static constexpr int height = heightV;
};

template <uint16_t widgetCountV, uint16_t heightV> void Widget<widgetCountV, heightV>::reFocus (auto &d, int focusIndex) const
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

/*--------------------------------------------------------------------------*/

template <int len> struct Line : public Widget<0, 1> {

        Visibility operator() (auto &d, int focusIndex, int groupIndex, int groupSelection) const
        {
                if (!detail::heightsOverlap (y, height, d.currentScroll, d.height)) {
                        return Visibility::outside;
                }

                detail::line (d, len);
                return Visibility::visible;
        }
};

// template <int len> constexpr auto line () { return Line<len>{}; }

template <int len> Line<len> line;

/*--------------------------------------------------------------------------*/

/**
 *
 */
template <uint16_t ox, uint16_t oy, uint16_t width, uint16_t height, typename Child> struct Window : public Widget<0, 0> {

        explicit Window (Child c) : child{std::move (c)} {}

        Visibility operator() (auto &d, int focusIndex, int groupIndex, int groupSelection) const;

        uint16_t cursorX{};
        uint16_t cursorY{};
        uint16_t currentFocus{};
        uint16_t currentScroll{};
        Child child;
};

template <uint16_t ox, uint16_t oy, uint16_t width, uint16_t height, typename Child>
Visibility Window<ox, oy, width, height, Child>::operator() (auto &d, int focusIndex, int groupIndex, int groupSelection) const
{
        d.x = ox;
        d.y = oy;

        return child (d, focusIndex, groupIndex, groupSelection);
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

struct Check : public Widget<1, 1> {
        constexpr Check (const char *s, bool c) : label{s}, checked{c} {}

        // TODO move outside the class.
        // TODO remove groupIndex and groupSelection. Not needed here.
        Visibility operator() (auto &d, int focusIndex, int groupIndex, int groupSelection) const
        {
                if (!detail::heightsOverlap (y, height, d.currentScroll, d.height)) {
                        return Visibility::outside;
                }

                if (d.currentFocus == focusIndex) {
                        wattron (d.win, COLOR_PAIR (1));
                }

                if (checked) {
                        mvwprintw (d.win, d.y, d.x, "☑");
                }
                else {
                        mvwprintw (d.win, d.y, d.x, "☐");
                }

                ++d.x;

                mvwprintw (d.win, d.y, d.x, label);

                if (d.currentFocus == focusIndex) {
                        wattroff (d.win, COLOR_PAIR (1));
                }

                d.x += strlen (label);
                return Visibility::visible;
        }

        void input (auto &d, char c, int focusIndex, int /* groupIndex */, int & /* groupSelection */)
        {
                if (d.currentFocus == focusIndex && c == ' ') { // TODO character must be customizable (compile time)
                        checked = !checked;
                }
        }

        const char *label{};
        bool checked{};
};

constexpr Check check (const char *str, bool checked = false) { return {str, checked}; }

/*--------------------------------------------------------------------------*/

struct Radio : public Widget<1, 1> {

        constexpr Radio (const char *l) : label{l} {}
        Visibility operator() (auto &d, int focusIndex, int groupIndex, int groupSelection) const;
        void input (auto &d, char c, int focusIndex, int groupIndex, int &groupSelection);

        const char *label;
};

Visibility Radio::operator() (auto &d, int focusIndex, int groupIndex, int groupSelection) const
{
        if (!detail::heightsOverlap (y, height, d.currentScroll, d.height)) {
                return Visibility::outside;
        }

        if (d.currentFocus == focusIndex) {
                wattron (d.win, COLOR_PAIR (1));
        }

        if (groupIndex == groupSelection) {
                mvwprintw (d.win, d.y, d.x, "◉");
        }
        else {
                mvwprintw (d.win, d.y, d.x, "○");
        }

        ++d.x;

        mvwprintw (d.win, d.y, d.x, label);

        if (d.currentFocus == focusIndex) {
                wattroff (d.win, COLOR_PAIR (1));
        }

        d.x += strlen (label);
        return Visibility::visible;
}

void Radio::input (auto &d, char c, int focusIndex, int groupIndex, int &groupSelection)
{
        if (d.currentFocus == focusIndex && c == ' ') { // TODO character must be customizable (compile time)
                groupSelection = groupIndex;
        }
}

constexpr Radio radio (const char *label) { return {label}; }

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
                if (widget (d, focusIndex, groupIndex, groupSelection) == Visibility::visible) {
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

        Visibility operator() (auto &d, int focusIndex = 0, int /* groupIndex */ = 0, int /* groupSelection */ = 0) const
        {
                if (!detail::heightsOverlap (y, height, d.currentScroll, d.height)) {
                        return Visibility::outside;
                }

                std::apply ([&d, &focusIndex, groupSelection = groupSelection] (
                                    auto const &...widgets) { detail::layout<Decor> (d, focusIndex, 0, groupSelection, widgets...); },
                            widgets);

                return Visibility::nonDrawable;
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

        void calculatePositions (int parentY = 0)
        {
                auto l = [this] (auto &itself, int prevY, int prevH, auto &widget, auto &...widgets) {
                        widget.y = prevY + prevH; // First statement is an equivalent to : widget[0].y = y
                        widget.calculatePositions (y);
                        // std::cout << widget.y << std::endl;

                        if constexpr (sizeof...(widgets) > 0) {
                                // Next statements : widget[n].y = widget[n-1].y + widget[n-1].height
                                itself (itself, widget.y, widget.height, widgets...);
                        }
                };

                std::apply ([&l, y = this->y] (auto &...widgets) { l (l, y, 0, widgets...); }, widgets);
        }

        int getY () const { return y; }
        int y{}; // TODO constexpr
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
        init_pair (1, COLOR_BLUE, COLOR_WHITE);
        init_pair (2, COLOR_WHITE, COLOR_BLUE);
        keypad (stdscr, true);
        d1.win = newwin (d1.height, d1.width, 0, 0);
        wbkgd (d1.win, COLOR_PAIR (2));
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

        auto vb = vbox (vbox (radio (" A "), radio (" B "), radio (" C "), radio (" d ")), //
                        line<10>,                                                          //
                        vbox (check (" 1 "), check (" 2 "), check (" 3 "), check (" 4 ")), //
                        line<10>,                                                          //
                        vbox (radio (" a "), radio (" b "), radio (" c "), radio (" d ")), //
                        line<10>,                                                          //
                        vbox (check (" 5 "), check (" 6 "), check (" 7 "), check (" 8 ")), //
                        line<10>,                                                          //
                        vbox (radio (" E "), radio (" F "), radio (" G "), radio (" H "))  //
        );                                                                                 //

        // auto vb = vbox (check (" 1"), check (" 2"), check (" 3"), check (" 4"), check (" 5"), check (" 6"), check (" 7"), check (" 8"),
        //                 check (" 9"), check (" 10"), check (" 11"), check (" 12"), check (" 13"), check (" 14"), check (" 15"),
        //                 check (" 16-last"));

        // TODO compile time
        // TODO no additional call
        vb.calculatePositions (); // Only once. After composition

        while (true) {
                wclear (d1.win);
                d1.x = 0; // TODO remove, this should reset on its own.
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
