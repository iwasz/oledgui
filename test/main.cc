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

struct Empty {
};

struct Point {
        uint16_t x{};
        uint16_t y{};
};

struct Dimensions {
        uint16_t width{};
        uint16_t height{};
};

/**
 * Runtime context for all the recursive loops.
 */
struct Context {
        Point origin{};
        Dimensions dimensions{};
        uint16_t currentFocus{};
        uint16_t currentScroll{};
};

template <uint16_t widthV, uint16_t heightV> class Display {
public:
        uint16_t x{};                               /// Current cursor position [in characters] TODO change name
        uint16_t y{};                               /// Current cursor position [in characters] TODO change type
        static constexpr uint16_t width = widthV;   // Dimensions in charcters
        static constexpr uint16_t height = heightV; // Dimensions in charcters
        // uint16_t currentFocus{};
        // uint16_t currentScroll{};
        Context context{{0, 0}, {width, height}};

        void move (uint16_t ox, uint16_t oy)
        {
                x += ox;
                y += oy;
        }

        void incrementFocus (auto const &mainWidget)
        {
                if (context.currentFocus < mainWidget.widgetCount - 1) {
                        ++context.currentFocus;
                        mainWidget.reFocus (context);
                }
        }

        void decrementFocus (auto const &mainWidget)
        {
                if (context.currentFocus > 0) {
                        --context.currentFocus;
                        mainWidget.reFocus (context);
                }
        }
};

/*--------------------------------------------------------------------------*/

/**
 * TODO move Child to the base class since all concrete implementations will own one child.
 * TODO move sa much as possble to the base class to ease concrete class implementations.
 *      Ideally leave only the constructor, print, clear and color.
 *
 */
template <uint16_t widthV, uint16_t heightV, typename Child = Empty> class NcursesDisplay : public Display<widthV, heightV> {
public:
        using Base = Display<widthV, heightV>;
        using Base::context;
        using Base::incrementFocus, Base::decrementFocus, Base::x, Base::y;

        NcursesDisplay (Child c = {});
        ~NcursesDisplay ();

        Visibility operator() ()
        {
                clear ();
                auto v = child (*this, context);
                wrefresh (win);
                return v;
        }

        void print (const char *str) { mvwprintw (win, y, x, str); }

        void clear ()
        {
                wclear (win);
                x = 0;
                y = 0;
        }

        void color (uint16_t c) { wattron (win, COLOR_PAIR (c)); }

        // TODO to be moved or removed
        void incrementFocus () { incrementFocus (child); }
        void decrementFocus () { decrementFocus (child); }
        void calculatePositions () { child.calculatePositions (); }

        // TODO to be removed
        void refresh () { wrefresh (win); }

private:
        WINDOW *win{};
        Child child;
};

template <uint16_t widthV, uint16_t heightV> auto ncurses (auto &&child)
{
        return NcursesDisplay<widthV, heightV, std::remove_reference_t<decltype (child)>>{std::forward<decltype (child)> (child)};
}

template <uint16_t widthV, uint16_t heightV, typename Child>
NcursesDisplay<widthV, heightV, Child>::NcursesDisplay (Child c) : child{std::move (c)}
{
        setlocale (LC_ALL, "");
        initscr ();
        curs_set (0);
        noecho ();
        cbreak ();
        use_default_colors ();
        start_color ();
        init_pair (1, COLOR_WHITE, COLOR_BLUE);
        init_pair (2, COLOR_BLUE, COLOR_WHITE);
        keypad (stdscr, true);
        win = newwin (Display<widthV, heightV>::height, Display<widthV, heightV>::width, 0, 0);
        wbkgd (win, COLOR_PAIR (1));
        refresh ();
}

template <uint16_t widthV, uint16_t heightV, typename Child> NcursesDisplay<widthV, heightV, Child>::~NcursesDisplay ()
{
        clrtoeol ();
        refresh ();
        endwin ();
}

/****************************************************************************/

namespace detail {
        void line (auto &d, uint16_t len)
        {
                for (uint16_t i = 0; i < len; ++i) {
                        // mvwprintw (d.win, d.y, d.x++, "─");
                        d.print ("-");
                        d.move (1, 0);
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

/**
 * Base class.
 * widgetCountV : number of focusable widgets. Some examples : for a label whihc is
 *                not focusable, use 0. For a button use 1. For containers return the
 *                number of focusable children insdide.
 * heightV : height of a widget in characters. For a mere (single line) label it would
 *           equal to 1, for a container it would be sum of all children heights.
 */
template <uint16_t widgetCountV = 0, uint16_t heightV = 0> struct Widget {
        void reFocus (Context &ctx, int focusIndex = 0) const;
        void input (auto &d, Context const &ctx, char c, int focusIndex, int /* groupIndex */, int & /* groupSelection */) {}
        // TODO remove
        void calculatePositions (int) {}

        int y{};
        static constexpr int widgetCount = widgetCountV;
        static constexpr int height = heightV;
};

template <uint16_t widgetCountV, uint16_t heightV> void Widget<widgetCountV, heightV>::reFocus (Context &ctx, int focusIndex) const
{
        if (!detail::heightsOverlap (y, height, ctx.currentScroll, ctx.dimensions.height)) {
                if (ctx.currentFocus == focusIndex) {
                        if (y < ctx.currentScroll) {
                                ctx.currentScroll = y;
                        }
                        else {
                                ctx.currentScroll = y - ctx.dimensions.height + 1;
                        }
                }
        }
}

/****************************************************************************/

template <int len> struct Line : public Widget<0, 1> {

        Visibility operator() (auto &d, Context const &ctx, int focusIndex, int groupIndex, int groupSelection) const
        {
                if (!detail::heightsOverlap (y, height, ctx.currentScroll, ctx.dimensions.height)) {
                        return Visibility::outside;
                }

                detail::line (d, len);
                return Visibility::visible;
        }
};

// template <int len> constexpr auto line () { return Line<len>{}; }

template <int len> Line<len> line;

/****************************************************************************/
/* Check                                                                    */
/****************************************************************************/

class Check : public Widget<1, 1> {
public:
        constexpr Check (const char *s, bool c) : label{s}, checked{c} {}

        // TODO move outside the class.
        // TODO remove groupIndex and groupSelection. Not needed here.
        Visibility operator() (auto &d, Context const &ctx, int focusIndex, int groupIndex, int groupSelection) const
        {
                if (!detail::heightsOverlap (y, height, ctx.currentScroll, ctx.dimensions.height)) {
                        return Visibility::outside;
                }

                if (ctx.currentFocus == focusIndex) {
                        d.color (2);
                }

                if (checked) {
                        d.print ("☑");
                }
                else {
                        d.print ("☐");
                }

                d.move (1, 0);
                d.print (label);

                if (ctx.currentFocus == focusIndex) {
                        d.color (1);
                }

                d.move (strlen (label), 0); // TODO this strlen should be constexpr expression
                return Visibility::visible;
        }

        void input (auto &d, Context const &ctx, char c, int focusIndex, int /* groupIndex */, int & /* groupSelection */)
        {
                if (ctx.currentFocus == focusIndex && c == ' ') { // TODO character must be customizable (compile time)
                        checked = !checked;
                }
        }

private:
        const char *label{};
        bool checked{};
};

constexpr Check check (const char *str, bool checked = false) { return {str, checked}; }

/****************************************************************************/
/* Radio                                                                    */
/****************************************************************************/

class Radio : public Widget<1, 1> {
public:
        constexpr Radio (const char *l) : label{l} {}
        Visibility operator() (auto &d, Context const &ctx, int focusIndex, int groupIndex, int groupSelection) const;
        void input (auto &d, Context const &ctx, char c, int focusIndex, int groupIndex, int &groupSelection);

private:
        const char *label;
};

Visibility Radio::operator() (auto &d, Context const &ctx, int focusIndex, int groupIndex, int groupSelection) const
{
        if (!detail::heightsOverlap (y, height, ctx.currentScroll, ctx.dimensions.height)) {
                return Visibility::outside;
        }

        if (ctx.currentFocus == focusIndex) {
                d.color (2);
        }

        if (groupIndex == groupSelection) {
                d.print ("◉");
        }
        else {
                d.print ("○");
        }

        d.move (1, 0);
        d.print (label);

        if (ctx.currentFocus == focusIndex) {
                d.color (1);
        }

        d.move (strlen (label), 0);
        return Visibility::visible;
}

void Radio::input (auto &d, Context const &ctx, char c, int focusIndex, int groupIndex, int &groupSelection)
{
        if (ctx.currentFocus == focusIndex && c == ' ') { // TODO character must be customizable (compile time)
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
        void layout (Disp &d, Context const &ctx, int focusIndex, int groupIndex, int groupSelection, W const &widget, Rst const &...widgets)
        {
                if (widget (d, ctx, focusIndex, groupIndex, groupSelection) == Visibility::visible) {
                        Layout::after (d);
                }

                if constexpr (sizeof...(widgets) > 0) {
                        layout<Layout> (d, ctx, focusIndex + widget.widgetCount, groupIndex + 1, groupSelection, widgets...);
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

        Visibility operator() (auto &d, Context const &ctx, int focusIndex = 0, int /* groupIndex */ = 0, int /* groupSelection */ = 0) const
        {
                // TODO move to separate function. Code duplication.
                if (!detail::heightsOverlap (y, height, ctx.currentScroll, ctx.dimensions.height)) {
                        return Visibility::outside;
                }

                std::apply ([&d, &focusIndex, groupSelection = groupSelection,
                             &ctx] (auto const &...widgets) { detail::layout<Decor> (d, ctx, focusIndex, 0, groupSelection, widgets...); },
                            widgets);

                return Visibility::nonDrawable;
        }

        void reFocus (Context &ctx, int focusIndex = 0) const
        {
                auto l = [&ctx] (auto &itself, int focusIndex, auto const &widget, auto const &...widgets) {
                        widget.reFocus (ctx, focusIndex);

                        if constexpr (sizeof...(widgets)) {
                                itself (itself, focusIndex + widget.widgetCount, widgets...);
                        }
                };

                std::apply ([focusIndex, &l] (auto const &...widgets) { l (l, focusIndex, widgets...); }, widgets);
        }

        void input (auto &d, Context const &ctx, char c, int focusIndex, int /* groupIndex */, int & /* groupSelection */)
        {
                auto &groupSelectionReference = groupSelection;
                auto l = [&d, c, &groupSelectionReference, &ctx] (auto &itself, int focusIndex, int groupIndex, auto &widget, auto &...widgets) {
                        widget.input (d, ctx, c, focusIndex, groupIndex, groupSelectionReference);

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

/**
 *
 */
template <uint16_t ox, uint16_t oy, uint16_t width, uint16_t height, typename Child> struct Window : public Widget<0, 0> {

        explicit Window (Child c) : child{std::move (c)} {}

        Visibility operator() (auto &d, Context const &ctx, int focusIndex = 0, int groupIndex = 0, int groupSelection = 0) const
        {
                d.x = ox;
                d.y = oy;
                return child (d, ctx, focusIndex, groupIndex, groupSelection);
        }

        void input (auto &d, char c, int focusIndex, int groupIndex, int &groupSelection)
        {
                child.input (d, c, focusIndex, groupIndex, groupSelection);
        }

        Context context;
        Child child;
};

template <uint16_t ox, uint16_t oy, uint16_t width, uint16_t height> auto window (auto &&c)
{
        return Window<ox, oy, width, height, std::remove_reference_t<decltype (c)>> (std::forward<decltype (c)> (c));
}

/*--------------------------------------------------------------------------*/
// auto label (const char *str)
// {
//         return [str] (auto &d, int focusIndex = 0) {
//                 mvwprintw (d.win, d.y, d.x, str);
//                 d.x += strlen (str);
//         };
// }
/*--------------------------------------------------------------------------*/

// auto dialog (const char *str)
// {
//         return [str] (auto &d) {
//                 uint16_t len = strlen (str);

//                 mvwprintw (d.win, d.y, d.x++, "┌");
//                 detail::line (d, len);
//                 mvwprintw (d.win, d.y, d.x++, "┐");
//                 ++d.y;
//                 d.x = 0;

//                 mvwprintw (d.win, d.y, d.x, "│");
//                 ++d.x;

//                 mvwprintw (d.win, d.y, d.x, str);
//                 d.x += strlen (str);

//                 mvwprintw (d.win, d.y++, d.x, "│");
//                 d.x = 0;

//                 mvwprintw (d.win, d.y, d.x++, "└");
//                 detail::line (d, len);
//                 mvwprintw (d.win, d.y, d.x++, "┘");
//         };
// }
} // namespace og

/****************************************************************************/

int test1 ()
{
        using namespace og;

        /*
         * TODO This has the problem that it woud be tedious and wasteful to declare ncurses<18, 7>
         * with every new window/view. Alas I'm trying to investigate how to commonize Displays and Windows.
         *
         * This ncurses method can be left as an option.
         */
        auto vb = ncurses<18, 7> (vbox (vbox (radio (" A "), radio (" B "), radio (" C "), radio (" d ")), //
                                        line<10>,                                                          //
                                        vbox (check (" 1 "), check (" 2 "), check (" 3 "), check (" 4 ")), //
                                        line<10>,                                                          //
                                        vbox (radio (" a "), radio (" b "), radio (" c "), radio (" d ")), //
                                        line<10>,                                                          //
                                        vbox (check (" 5 "), check (" 6 "), check (" 7 "), check (" 8 ")), //
                                        line<10>,                                                          //
                                        vbox (radio (" E "), radio (" F "), radio (" G "), radio (" H "))  //
                                        ));                                                                //

        // TODO compile time
        // TODO no additional call
        vb.calculatePositions (); // Only once. After composition

        // auto dialog = window<2, 2, 10, 10> (vbox (radio (" A "), radio (" B "), radio (" C "), radio (" d ")));
        // // dialog.calculatePositions ();

        bool showDialog{};

        while (true) {
                // d1.clear ()
                // vb (d1);

                vb ();

                // if (showDialog) {
                //         dialog (d1);
                // }

                // wrefresh (d1.win);
                int ch = getch ();

                if (ch == 'q') {
                        break;
                }

                switch (ch) {
                case KEY_DOWN:
                        // d1.incrementFocus (vb);
                        vb.incrementFocus ();
                        break;

                case KEY_UP:
                        vb.decrementFocus ();
                        break;

                case 'd':
                        showDialog = true;
                        break;

                default:
                        // d1.input (vb, char (ch));
                        int dummy; // TODO ugly
                                   // vb.input (d1, char (ch), 0, 0, dummy); // TODO ugly, should be vb.input (d1, char (ch)) at most
                        break;
                }
        }

        return 0;
}

/****************************************************************************/

int test2 ()
{
        using namespace og;
        NcursesDisplay<18, 7> d1;

        /*
         * TODO This has the problem that it woud be tedious and wasteful to declare ncurses<18, 7>
         * with every new window/view. Alas I'm trying to investigate how to commonize Displays and Windows.
         *
         * This ncurses method can be left as an option.
         */
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

        // TODO compile time
        // TODO no additional call
        vb.calculatePositions (); // Only once. After composition

        auto dialog = window<2, 2, 10, 10> (vbox (radio (" A "), radio (" B "), radio (" C "), radio (" d ")));
        // dialog.calculatePositions ();

        bool showDialog{};

        while (true) {
                d1.clear ();
                vb (d1, d1.context);

                if (showDialog) {
                        dialog (d1, d1.context);
                }

                d1.refresh ();
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

                case 'd':
                        showDialog = true;
                        break;

                default:
                        // d1.input (vb, char (ch));
                        int dummy;                                         // TODO ugly
                        vb.input (d1, d1.context, char (ch), 0, 0, dummy); // TODO ugly, should be vb.input (d1, char (ch)) at most
                        break;
                }
        }

        return 0;
}

/****************************************************************************/

int main ()
{
        test2 ();
        test1 ();
}
