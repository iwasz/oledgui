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

using Coordinate = uint16_t;

struct Point {
        Coordinate x{};
        Coordinate y{};
};

using Dimension = uint16_t;

struct Dimensions {
        Dimension width{};
        Dimension height{};
};

/**
 * Runtime context for all the recursive loops.
 */
struct Context {
        Point origin{};
        Dimensions dimensions{};
        uint16_t currentFocus{};
        uint16_t currentScroll{};
        uint8_t *radioSelection{};
};

/// These classes are for simplyfying the Widget API. Instead of n function arguments we have those 2 aggreagtes.
struct Iteration {
        uint16_t focusIndex{};
        uint16_t groupIndex{};
};

template <typename ConcreteClass, uint16_t widthV, uint16_t heightV, typename Child = Empty> class Display {
public:
        Display (Child c = {}) : child{std::move (c)} {}

        Visibility operator() ()
        {
                static_cast<ConcreteClass *> (this)->clear (); // How cool.
                auto v = child (*static_cast<ConcreteClass *> (this), context);
                static_cast<ConcreteClass *> (this)->refresh ();
                return v;
        }

        void move (uint16_t ox, uint16_t oy)
        {
                cursorX += ox;
                cursorY += oy;
        }

        void incrementFocus (auto const &mainWidget)
        {
                if (context.currentFocus < mainWidget.widgetCount - 1) {
                        ++context.currentFocus;
                        mainWidget.reFocus (context);
                }
        }

        void incrementFocus () { incrementFocus (child); }

        void decrementFocus (auto const &mainWidget)
        {
                if (context.currentFocus > 0) {
                        --context.currentFocus;
                        mainWidget.reFocus (context);
                }
        }

        void decrementFocus () { decrementFocus (child); }

        void calculatePositions () { child.calculatePositions (); }

        uint16_t cursorX{};                         /// Current cursor position in characters
        uint16_t cursorY{};                         /// Current cursor position in characters
        static constexpr uint16_t width = widthV;   // Dimensions in charcters
        static constexpr uint16_t height = heightV; // Dimensions in charcters
        Context context{{0, 0}, {width, height}};
        Child child;
};

/*--------------------------------------------------------------------------*/

/**
 * Ncurses backend.
 */
template <uint16_t widthV, uint16_t heightV, typename Child = Empty>
class NcursesDisplay : public Display<NcursesDisplay<widthV, heightV, Child>, widthV, heightV, Child> {
public:
        using Base = Display<NcursesDisplay<widthV, heightV, Child>, widthV, heightV, Child>;
        using Base::child;
        using Base::context;
        using Base::cursorX, Base::cursorY, Base::width, Base::height;

        NcursesDisplay (Child c = {});
        ~NcursesDisplay ();

        void print (const char *str) { mvwprintw (win, cursorY, cursorX, str); }

        void clear ()
        {
                wclear (win);
                cursorX = 0;
                cursorY = 0;
        }

        void color (uint16_t c) { wattron (win, COLOR_PAIR (c)); }

        void refresh () { wrefresh (win); }

private:
        WINDOW *win{};
};

template <uint16_t widthV, uint16_t heightV, typename Child> NcursesDisplay<widthV, heightV, Child>::NcursesDisplay (Child c) : Base (c)
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
        win = newwin (height, width, 0, 0);
        wbkgd (win, COLOR_PAIR (1));
        refresh ();
}

template <uint16_t widthV, uint16_t heightV, typename Child> NcursesDisplay<widthV, heightV, Child>::~NcursesDisplay ()
{
        clrtoeol ();
        refresh ();
        endwin ();
}

template <uint16_t widthV, uint16_t heightV> auto ncurses (auto &&child)
{
        return NcursesDisplay<widthV, heightV, std::remove_reference_t<decltype (child)>>{std::forward<decltype (child)> (child)};
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
        template <typename T> constexpr bool heightsOverlap (T y1, T height1, T y2, T height2) // TODO not int.
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
        void reFocus (Context &ctx, uint16_t focusIndex = 0) const;
        void input (auto &d, Context const &ctx, Iteration iter, char c) {}
        // TODO remove
        void calculatePositions (uint16_t) {}

        uint16_t y{};
        static constexpr uint16_t widgetCount = widgetCountV;
        static constexpr uint16_t height = heightV;
};

template <uint16_t widgetCountV, uint16_t heightV> void Widget<widgetCountV, heightV>::reFocus (Context &ctx, uint16_t focusIndex) const
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

template <uint16_t len> struct Line : public Widget<0, 1> {

        Visibility operator() (auto &d, Context const &ctx, Iteration /* iter */) const
        {
                if (!detail::heightsOverlap (y, height, ctx.currentScroll, ctx.dimensions.height)) {
                        return Visibility::outside;
                }

                detail::line (d, len);
                return Visibility::visible;
        }
};

// template <uint16_t len> constexpr auto line () { return Line<len>{}; }

template <uint16_t len> Line<len> line;

/****************************************************************************/
/* Check                                                                    */
/****************************************************************************/

class Check : public Widget<1, 1> {
public:
        constexpr Check (const char *s, bool c) : label{s}, checked{c} {}

        // TODO move outside the class.
        // TODO remove groupIndex and groupSelection. Not needed here.
        Visibility operator() (auto &d, Context const &ctx, Iteration iter) const
        {
                if (!detail::heightsOverlap (y, height, ctx.currentScroll, ctx.dimensions.height)) {
                        return Visibility::outside;
                }

                if (ctx.currentFocus == iter.focusIndex) {
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

                if (ctx.currentFocus == iter.focusIndex) {
                        d.color (1);
                }

                d.move (strlen (label), 0); // TODO this strlen should be constexpr expression
                return Visibility::visible;
        }

        void input (auto &d, Context const &ctx, Iteration iter, char c)
        {
                if (ctx.currentFocus == iter.focusIndex && c == ' ') { // TODO character must be customizable (compile time)
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
        Visibility operator() (auto &d, Context const &ctx, Iteration iter) const;
        void input (auto &d, Context const &ctx, Iteration iter, char c);

private:
        const char *label;
};

Visibility Radio::operator() (auto &d, Context const &ctx, Iteration iter) const
{
        if (!detail::heightsOverlap (y, height, ctx.currentScroll, ctx.dimensions.height)) {
                return Visibility::outside;
        }

        if (ctx.currentFocus == iter.focusIndex) {
                d.color (2);
        }

        if (iter.groupIndex == *ctx.radioSelection) {
                d.print ("◉");
        }
        else {
                d.print ("○");
        }

        d.move (1, 0);
        d.print (label);

        if (ctx.currentFocus == iter.focusIndex) {
                d.color (1);
        }

        d.move (strlen (label), 0);
        return Visibility::visible;
}

void Radio::input (auto &d, Context const &ctx, Iteration iter, char c)
{
        if (ctx.currentFocus == iter.focusIndex && c == ' ') { // TODO character must be customizable (compile time)
                *ctx.radioSelection = iter.groupIndex;
        }
}

constexpr Radio radio (const char *label) { return {label}; }

/*--------------------------------------------------------------------------*/

namespace detail {

        struct VBoxDecoration {
                static void after (auto &d, Context const &ctx)
                {
                        d.cursorY += 1;
                        d.cursorX = ctx.origin.x;
                }
        };

        struct HBoxDecoration {
                static void after (auto &d)
                {
                        // d.y += 1;
                        // d.x = 0;
                }
        };

        template <typename T> struct WidgetCountField {
                static constexpr uint16_t value = T::widgetCount;
        };

        template <typename T> struct WidgetHeightField {
                static constexpr uint16_t value = T::height;
        };

        template <typename Tuple, template <typename T> typename Field, size_t n = std::tuple_size_v<Tuple> - 1> struct Sum {
                static constexpr auto value = Field<std::tuple_element_t<n, Tuple>>::value + Sum<Tuple, Field, n - 1>::value;
        };

        template <typename Tuple, template <typename T> typename Field> struct Sum<Tuple, Field, 0> {
                static constexpr auto value = Field<std::tuple_element_t<0, Tuple>>::value;
        };

} // namespace detail

template <typename Decor, typename WidgetsTuple> struct Layout {

        static constexpr uint16_t widgetCount = detail::Sum<WidgetsTuple, detail::WidgetCountField>::value;
        static constexpr uint16_t height = detail::Sum<WidgetsTuple, detail::WidgetHeightField>::value;

        Layout (WidgetsTuple w) : widgets (std::move (w)) { calculatePositions (); }

        Visibility operator() (auto &d) const { return operator() (d, d.context, {}); }
        Visibility operator() (auto &d, Context &ctx, Iteration iter = {}) const
        {
                // TODO move to separate function. Code duplication.
                if (!detail::heightsOverlap (y, height, ctx.currentScroll, ctx.dimensions.height)) {
                        return Visibility::outside;
                }
                ctx.radioSelection = &radioSelection;

                auto l = [&d, &ctx] (auto &itself, Iteration iter, auto const &widget, auto const &...widgets) {
                        if (widget (d, ctx, iter) == Visibility::visible) {
                                Decor::after (d, ctx);
                        }

                        if constexpr (sizeof...(widgets) > 0) {
                                itself (itself, {uint16_t (iter.focusIndex + widget.widgetCount), uint16_t (iter.groupIndex + 1)}, widgets...);
                        }
                };

                std::apply ([&d, &ctx, &iter, &l] (auto const &...widgets) { l (l, {iter.focusIndex, 0}, widgets...); }, widgets);
                return Visibility::nonDrawable;
        }

        void reFocus (Context &ctx, uint16_t focusIndex = 0) const
        {
                auto l = [&ctx] (auto &itself, uint16_t focusIndex, auto const &widget, auto const &...widgets) {
                        widget.reFocus (ctx, focusIndex);

                        if constexpr (sizeof...(widgets)) {
                                itself (itself, focusIndex + widget.widgetCount, widgets...);
                        }
                };

                std::apply ([focusIndex, &l] (auto const &...widgets) { l (l, focusIndex, widgets...); }, widgets);
        }

        void input (auto &d, char c) { input (d, d.context, {}, c); }
        void input (auto &d, Context &ctx, Iteration iter, char c)
        {
                ctx.radioSelection = &radioSelection;

                auto l = [&d, &ctx, c] (auto &itself, Iteration iter, auto &widget, auto &...widgets) {
                        widget.input (d, ctx, iter, c);

                        if constexpr (sizeof...(widgets)) {
                                itself (itself, {uint16_t (iter.focusIndex + widget.widgetCount), uint16_t (iter.groupIndex + 1)}, widgets...);
                        }
                };

                std::apply ([&l, &iter] (auto &...widgets) { l (l, {iter.focusIndex, 0}, widgets...); }, widgets);
        }

        void calculatePositions (uint16_t /* parentY */ = 0)
        {
                auto l = [this] (auto &itself, uint16_t prevY, uint16_t prevH, auto &widget, auto &...widgets) {
                        widget.y = prevY + prevH; // First statement is an equivalent to : widget[0].y = y
                        widget.calculatePositions (y);

                        if constexpr (sizeof...(widgets) > 0) {
                                // Next statements : widget[n].y = widget[n-1].y + widget[n-1].height
                                itself (itself, widget.y, widget.height, widgets...);
                        }
                };

                std::apply ([&l, y = this->y] (auto &...widgets) { l (l, y, 0, widgets...); }, widgets);
        }

        uint16_t y{}; // TODO constexpr

private:
        mutable uint8_t radioSelection{};
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
template <uint16_t ox, uint16_t oy, uint16_t widthV, uint16_t heightV, typename Child> struct Window : public Widget<0, 0> {

        explicit Window (Child c) : child{std::move (c)} {}

        Visibility operator() (auto &d) const { return operator() (d, d.context, {}); }
        Visibility operator() (auto &d, Context &ctx, Iteration iter = {}) const
        {
                d.cursorX = ox;
                d.cursorY = oy;
                return child (d, context, iter);
        }

        void input (auto &d, char c, uint16_t focusIndex, uint16_t groupIndex, uint8_t &groupSelection)
        {
                child.input (d, c, focusIndex, groupIndex, groupSelection);
        }

        static constexpr uint16_t width = widthV;   // Dimensions in charcters
        static constexpr uint16_t height = heightV; // Dimensions in charcters
        mutable Context context{{ox, oy}, {width, height}};
        Child child;
};

template <uint16_t ox, uint16_t oy, uint16_t widthV, uint16_t heightV> auto window (auto &&c)
{
        return Window<ox, oy, widthV, heightV, std::remove_reference_t<decltype (c)>> (std::forward<decltype (c)> (c));
}

/*--------------------------------------------------------------------------*/
// auto label (const char *str)
// {
//         return [str] (auto &d, uint16_t focusIndex = 0) {
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
        auto vb = vbox (vbox (radio (" A "), radio (" B "), radio (" C "), radio (" d ")),       //
                        line<10>,                                                                //
                        vbox (check (" 1 "), check (" 2 "), check (" 3 "), check (" 4 ")),       //
                        line<10>,                                                                //
                        vbox (radio (" a "), radio (" b "), radio (" c "), radio (" d ")),       //
                        line<10>,                                                                //
                        vbox (check (" 5 "), check (" 6 "), check (" 7 "), check (" 8 ")),       //
                        line<10>,                                                                //
                        vbox (vbox (radio (" x "), radio (" y "), radio (" z "), radio (" ź ")), //
                              vbox (radio (" ż "), radio (" ą "), radio (" ę "), radio (" ł "))) //
        );                                                                                       //

        auto dialog = window<2, 2, 10, 10> (vbox (line<10>, line<10>, line<10>, line<10>));

        bool showDialog{};

        while (true) {
                d1.clear ();
                vb (d1);

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
                        vb.input (d1, char (ch));
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
