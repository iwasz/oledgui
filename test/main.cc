/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "oledgui.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <ncurses.h>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

/*
A short code description:
There are two layers of class templates.

# Layer 1 (namespace og::)
User creates a static tree of widgets where dimensions and positions of these widgets are
fixed (compile time). Then, thanks to the static nature of the implementation certain attributes
can be calculated at compile time:
* height at which a widget is located looking from the top of the screen
* focusIndex

## Widgets
There are two layers of class templates here (I'll refer to them as simply classes for short).
First there are classes like Check, Radio and so on, and they are created by corresponding
factory methods named accordingly (check, radio etc). Widgets can have these fields:

* Coordinate height: height in characters.
* bool canFocus: tells if widget can have focus. You also may inherit from Focusable class.
* template <typename Wrapper> Visibility operator() (auto &d, Context const &ctx) const : draws
        the widget on the screen.
* template <typename Wrapper> void input (auto &display, Context const &ctx, char c) : handles
        the input

The first layer is meant to be extendeb by the user.

## ContainerWidgets
Then there are ContainerWidgets : Layout, Group and Window. When filled with children, they
are able to calculate uint16_t focusableWidgetCount at compile time. This variable tells how many
child widgets inside a container is able to accept focus. Other than children (or child) fields
there is nothing special about them. Both Layouts, Groups and Windows inherit from ContainerWidget
class (static polymorphism aka CRTP).

## Layouts
Although layout is a type of a widget, it can store arbitrary number of other widgets.

## (Radio) Groups
Stores one or more instances of Radio class.

## Windows
Can contain only 1 child. maintains a Context which stores runtime informations as current
cursor position (or a pointer to it).

# Layer 2 (namespace og::augment::)
This layer is not meant to be extended or inherited from by the user. It contains class templates
that wrap widgets and adds more information at compile time. This information includes:
* Dimension getHeight (): for container widgets this is a sum of all children heights.
        getHeight () - uses information from layer 1 solely. It can be calculated in the layer 1
        classes. This is because simple widgets (i.e. not containers) know their height from the very
        start (usually 1). And containers can simply sum all the heights of their children which is
        simple to do at compile time.
* Selection getRadioIndex (): this returns a child number in a container starting from 0. This
        leverages information from the layer 1 and is calculated in between the layers in the
        Wrapper helper classes. Wrapper helpers implement more involving compile time iterations
        over children. Due to the fact that every container starts incrementing the radio index
        from the 0, this value is also easy to obtain in a simple compile time "loop". See
        transformImpl and transform functions.
* Focus getFocusIndex (): every focusable widget in the tree has consecutive number assigned,
        and this methid returns that number. This is calculated in a similar way as the previous
        one, but this time addint 1 every time is not enough as the whole tree has to be traversed.
        What is more, the tree is not heterogeneous, because you can have a mix of Layouts, Groups
        and Windows (although not every combination makes sense and may be prohibited in the future
        by some static checks or concepts). Thus the getFocusableWidgetCount helper function returns
        the appropriate value for every child. If it's a simple widget, it returns a simple value,
        otherwise it sums the heights recursively.
* getY (): this method returns the position in characters of a widget starting from the top
        of the screen. What differentiates this field is that it uses information from both
        layers : height fields from layer 1 and getHeight() from layer 2 (og::augment::). The
        getHeightIncrement helper is used in the transformImpl to achieve that.

## Problem with Groups
The problem with this container is that goes between the Layout and the Radio instances. It implements
only grouping, while the positioning of the radios resulting from the concrete Layout type (hbox or vbox)
has to be maintained. This is troublesome in compile time environment, so the introspection ios used in
some places to differentiate between Layouts and Groups (by introspection i mean if constexpr (requires {
check for a method or field })). Also the GrandParent parameter was added solely for the purpose of
implementing Groups. See the getHeightIncrement.

## Problem with windows
Not so difficult to solve as previous one, but still. The runtime information (as opposed to compile
time stuff like positions, orderings and heights) has to be accessible for every widget in a tree.
For this reason the Context class was introduced. A display instance has its own context (this may
be subject to change) and then the Windows also have their own Context-s. This is because a window
can be drawn on top of another, and we have to maintain the runtime state of both. The way this is
implemented is that if a concrete ContainerWidget instance has its own context field it replaces the
previous (partent's or display's). The changeContext is used fo that.
*/

namespace og {
enum class Visibility {
        visible,    // Widget drawed something on the screen.
        outside,    // Widget is invisible because is outside the active region.
        nonDrawable // Widget is does not draw anything by itself.
};

struct Empty {
};

using Coordinate = uint16_t;
using Dimension = uint16_t;
using Focus = uint16_t;
using Selection = uint8_t;
using Color = uint16_t;

struct Point {
        Coordinate x{};
        Coordinate y{};
};

struct Dimensions {
        Dimension width{};
        Dimension height{};
};

/**
 * Runtime context for all the recursive loops.
 */
struct Context {
        Point *cursor{};
        Point const origin{};
        Dimensions const dimensions{};
        Focus currentFocus{};
        Coordinate currentScroll{};
        Selection *radioSelection{};
};

template <Dimension widthV, Dimension heightV, typename Child> class NcursesDisplay;

namespace c {

        /**
         * Level 1 widget.
         */
        template <typename T>
        concept widget = requires (T const t, NcursesDisplay<0, 0, Empty> &d, Context const &c)
        {
                {
                        T::height
                        } -> std::same_as<Dimension const &>;
                {
                        t.template operator()<int> (d, c)
                        } -> std::same_as<Visibility>;

                // t.template input<int> (d, c, 'a'); // input is not required. Some widget has it, some hasn't
        };

} // namespace c

/**
 * A helper.
 */
template <typename... T> struct First {
        using type = std::tuple_element_t<0, std::tuple<T...>>;
};

template <typename... T> using First_t = typename First<T...>::type;

template <typename ConcreteClass, Dimension widthV, Dimension heightV, typename Child = Empty> class Display {
public:
        Display (Child const &c = {}) : child{c} {}

        Visibility operator() ()
        {
                static_cast<ConcreteClass *> (this)->clear (); // How cool.
                auto v = child (*static_cast<ConcreteClass *> (this), context);
                static_cast<ConcreteClass *> (this)->refresh ();
                return v;
        }

        void move (Coordinate ox, Coordinate oy) // TODO make this a Point method
        {
                cursor.x += ox;
                cursor.y += oy;
        }

        void setCursorX (Coordinate x) { cursor.x = x; }
        void setCursorY (Coordinate y) { cursor.y = y; }

        void incrementFocus (auto const &mainWidget) { mainWidget.incrementFocus (context); }
        void incrementFocus () { incrementFocus (child); }
        void decrementFocus (auto const &mainWidget) { mainWidget.decrementFocus (context); }
        void decrementFocus () { decrementFocus (child); }

        // void calculatePositions () { child.calculatePositions (); }

        Context *getContext () { return &context; }

        static constexpr Dimension width = widthV;   // Dimensions in characters
        static constexpr Dimension height = heightV; // Dimensions in characters

protected:
        Context context{&cursor, {0, 0}, {width, height}};
        Child child;
        Point cursor{};
};

/**
 * Ncurses backend.
 */
template <Dimension widthV, Dimension heightV, typename Child = Empty>
class NcursesDisplay : public Display<NcursesDisplay<widthV, heightV, Child>, widthV, heightV, Child> {
public:
        using Base = Display<NcursesDisplay<widthV, heightV, Child>, widthV, heightV, Child>;
        using Base::width, Base::height;

        NcursesDisplay (Child c = {});
        ~NcursesDisplay ()
        {
                clrtoeol ();
                refresh ();
                endwin ();
        }

        void print (const char *str) { mvwprintw (win, cursor.y, cursor.x, str); }

        void clear ()
        {
                wclear (win);
                cursor.x = 0;
                cursor.y = 0;
        }

        void color (Color c) { wattron (win, COLOR_PAIR (c)); }

        void refresh ()
        {
                ::refresh ();
                wrefresh (win);
        }

        using Base::context; // TODO private

private:
        using Base::child;
        using Base::cursor;
        WINDOW *win{};
};

template <Dimension widthV, Dimension heightV, typename Child> NcursesDisplay<widthV, heightV, Child>::NcursesDisplay (Child c) : Base (c)
{
        // return;
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

template <Dimension widthV, Dimension heightV> auto ncurses (auto &&child)
{
        return NcursesDisplay<widthV, heightV, std::remove_reference_t<decltype (child)>>{std::forward<decltype (child)> (child)};
}

/****************************************************************************/

namespace detail {

        void line (auto &d, uint16_t len, const char *ch = "─")
        {
                for (uint16_t i = 0; i < len; ++i) {
                        d.print (ch);
                        d.move (1, 0);
                }
        }

} // namespace detail

namespace detail {
        template <typename T> constexpr bool heightsOverlap (T y1, T height1, T y2, T height2)
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

struct Focusable {
        static constexpr bool canFocus = true;
};

/****************************************************************************/

template <uint16_t len, char c> struct Line {
        static constexpr Dimension height = 1;

        template <typename> Visibility operator() (auto &d, Context const & /* ctx */) const
        {
                char cc = c; // TODO I don't like this
                detail::line (d, len, &cc);
                return Visibility::visible;
        }
};

template <uint16_t len> Line<len, '-'> line;
template <uint16_t len = 1> Line<len, ' '> hspace;

/****************************************************************************/
/* Check                                                                    */
/****************************************************************************/

class Check : public Focusable {
public:
        static constexpr Dimension height = 1;

        constexpr Check (const char *s, bool c) : label{s}, checked{c} {}

        template <typename Wrapper> Visibility operator() (auto &d, Context const &ctx) const;
        template <typename Wrapper> void input (auto & /* d */, Context const & /* ctx */, char c)
        {
                if (c == ' ') { // TODO character must be customizable (compile time)
                        checked = !checked;
                }
        }

private:
        const char *label{};
        bool checked{};
};

/*--------------------------------------------------------------------------*/

template <typename Wrapper> Visibility Check::operator() (auto &d, Context const &ctx) const
{
        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
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

        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                d.color (1);
        }

        d.move (strlen (label), 0); // TODO constexpr strings?
        return Visibility::visible;
}

/*--------------------------------------------------------------------------*/

constexpr Check check (const char *str, bool checked = false) { return {str, checked}; }

/****************************************************************************/
/* Radio                                                                    */
/****************************************************************************/

template <std::integral Id> // TODO less restrictive concept for Id
class Radio : public Focusable {
public:
        static constexpr Dimension height = 1;

        constexpr Radio (Id const &i, const char *l) : id{i}, label{l} {}
        template <typename Wrapper> Visibility operator() (auto &d, Context const &ctx) const;
        template <typename Wrapper> void input (auto &d, Context const &ctx, char c);

private:
        Id id;
        const char *label;
};

/*--------------------------------------------------------------------------*/

template <std::integral Id> template <typename Wrapper> Visibility Radio<Id>::operator() (auto &d, Context const &ctx) const
{
        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                d.color (2);
        }

        if (ctx.radioSelection != nullptr && Wrapper::getRadioIndex () == *ctx.radioSelection) {
                d.print ("◉");
        }
        else {
                d.print ("○");
        }

        d.move (1, 0);
        d.print (label);

        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                d.color (1);
        }

        d.move (strlen (label), 0);
        return Visibility::visible;
}

template <std::integral Id> template <typename Wrapper> void Radio<Id>::input (auto & /* d */, Context const &ctx, char c)
{
        // TODO character must be customizable (compile time)
        if (ctx.radioSelection != nullptr && c == ' ') {
                *ctx.radioSelection = Wrapper::getRadioIndex ();
        }
}

template <std::integral Id> constexpr auto radio (Id &&id, const char *label) { return Radio<Id> (std::forward<Id> (id), label); }

template <typename T> struct is_radio : public std::bool_constant<false> {
};

template <std::integral Id> struct is_radio<Radio<Id>> : public std::bool_constant<true> {
};

/****************************************************************************/
/* Label                                                                    */
/****************************************************************************/

/**
 * Single line (?)
 */
class Label {
public:
        static constexpr Dimension height = 1;

        constexpr Label (const char *l) : label{l} {}

        template <typename /* Wrapper */> Visibility operator() (auto &d, Context const & /* ctx */) const
        {
                d.print (label);
                d.move (strlen (label), 0);
                return Visibility::visible;
        }

private:
        const char *label;
};

auto label (const char *str) { return Label{str}; }

/****************************************************************************/
/* Button                                                                   */
/****************************************************************************/

/**
 *
 */
template <typename Callback> class Button : public Focusable {
public:
        static constexpr Dimension height = 1;

        constexpr Button (const char *l, Callback const &c) : label{l}, callback{c} {}

        template <typename Wrapper> Visibility operator() (auto &d, Context const &ctx) const;

        template <typename> void input (auto & /* d */, Context const & /* ctx */, char c)
        {
                if (c == ' ') {
                        callback ();
                }
        }

private:
        const char *label;
        Callback callback;
};

template <typename Callback> template <typename Wrapper> Visibility Button<Callback>::operator() (auto &d, Context const &ctx) const
{
        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                d.color (2);
        }

        d.print (label);
        d.move (strlen (label), 0);

        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                d.color (1);
        }

        return Visibility::visible;
}

template <typename Callback> auto button (const char *str, Callback &&c) { return Button{str, std::forward<Callback> (c)}; }

/****************************************************************************/
/* Combo                                                                    */
/****************************************************************************/

/**
 * Single combo option.
 */
template <std::integral Id> struct Option { // TODO consider other types than std::integrals
        Option (Id const &i, const char *l) : id{i}, label{l} {}
        Id id;
        const char *label;
};

template <typename Id> auto option (Id &&id, const char *label) { return Option (std::forward<Id> (id), label); }

/**
 * A container for options.
 */
template <std::integral I, size_t Num> struct Options {

        using OptionType = Option<I>;
        using Id = I;
        using ContainerType = std::array<Option<I>, Num>;
        using SelectionIndex = typename ContainerType::size_type; // std::array::at accepts this

        template <typename... J> constexpr Options (Option<J> &&...e) : elms{std::forward<Option<J>> (e)...} {}

        ContainerType elms;
};

template <typename... J> Options (Option<J> &&...e) -> Options<First_t<J...>, sizeof...(J)>;

/**
 *
 */
template <typename OptionCollection, typename Callback>
requires std::invocable<Callback, typename OptionCollection::Id>
class Combo : public Focusable {
public:
        static constexpr Dimension height = 1;
        using Option = typename OptionCollection::OptionType;
        using Id = typename OptionCollection::Id;
        using SelectionIndex = typename OptionCollection::SelectionIndex;

        constexpr Combo (OptionCollection const &o, Callback c) : options{o}, callback{c} {}

        template <typename Wrapper> Visibility operator() (auto &d, Context const &ctx) const;
        template <typename Wrapper> void input (auto &d, Context const &ctx, char c);

private:
        OptionCollection options;
        SelectionIndex currentSelection{};
        Callback callback;
};

/*--------------------------------------------------------------------------*/

template <typename OptionCollection, typename Callback>
template <typename Wrapper>
Visibility Combo<OptionCollection, Callback>::operator() (auto &d, Context const &ctx) const
{
        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                d.color (2);
        }

        const char *label = options.elms.at (currentSelection).label;
        d.print (label);

        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                d.color (1);
        }

        d.move (strlen (label), 0);
        return Visibility::visible;
}

/*--------------------------------------------------------------------------*/

template <typename OptionCollection, typename Callback>
template <typename Wrapper>
void Combo<OptionCollection, Callback>::input (auto & /* d */, Context const &ctx, char c)
{
        if (ctx.currentFocus == Wrapper::getFocusIndex () && c == ' ') { // TODO character must be customizable (compile time)
                ++currentSelection;
                currentSelection %= options.elms.size ();
        }
}

/****************************************************************************/
/* Layouts                                                                  */
/****************************************************************************/

namespace detail {

        template <typename T> constexpr uint16_t getFocusableWidgetCount ()
        {
                // This introspection is for widget API simplification.
                if constexpr (requires {
                                      {
                                              T::focusableWidgetCount
                                              } -> std::convertible_to<Focus>;
                              }) {
                        // focusableWidgetCount field is used in composite widgets like Layout and Group.
                        return T::focusableWidgetCount;
                }
                else if constexpr (requires {
                                           {
                                                   T::canFocus
                                                   } -> std::convertible_to<bool>;
                                   }) {
                        // This field is optional and is used in regular widgets. Its absence is treated as it were declared false.
                        return uint16_t (T::canFocus);
                }
                else {
                        return 0;
                }
        }

        template <typename T> constexpr Dimension getHeight ()
        {
                // Again this introspection is for sake of simplifying the API.
                // User defined widgets ought to define the Dimension height field,
                // while augment:: wrappers implement getHeight methods.
                if constexpr (requires { T::getHeight (); }) {
                        return T::getHeight ();
                }
                else {
                        return T::height;
                }
        };

        template <typename T> struct FocusableWidgetCountField {
                static constexpr auto value = getFocusableWidgetCount<T> ();
        };

        template <typename T> struct WidgetHeightField {
                static constexpr auto value = getHeight<T> ();
        };

        /*--------------------------------------------------------------------------*/

        template <typename Tuple, template <typename T> typename Field, size_t n = std::tuple_size_v<Tuple> - 1> struct Sum {
                static constexpr auto value = Field<std::tuple_element_t<n, Tuple>>::value + Sum<Tuple, Field, n - 1>::value;
        };

        template <typename Tuple, template <typename T> typename Field> struct Sum<Tuple, Field, 0> {
                static constexpr auto value = Field<std::tuple_element_t<0, Tuple>>::value;
        };

        template <typename Tuple, template <typename T> typename Field, size_t n = std::tuple_size_v<Tuple> - 1> struct Max {
                static constexpr auto value = std::max (Field<std::tuple_element_t<n, Tuple>>::value, Max<Tuple, Field, n - 1>::value);
        };

        template <typename Tuple, template <typename T> typename Field> struct Max<Tuple, Field, 0> {
                static constexpr auto value = Field<std::tuple_element_t<0, Tuple>>::value;
        };

        /*--------------------------------------------------------------------------*/
        /* Decorators for vertical and horizontal layouts.                          */
        /*--------------------------------------------------------------------------*/

        template <typename WidgetsTuple> struct VBoxDecoration {
                static constexpr Dimension height = detail::Sum<WidgetsTuple, detail::WidgetHeightField>::value;

                static void after (Context const &ctx)
                {
                        ctx.cursor->y += 1;
                        ctx.cursor->x = ctx.origin.x;
                }
                static constexpr Dimension getHeightIncrement (Dimension d) { return d; }
        };

        template <typename WidgetsTuple> struct HBoxDecoration {
                static constexpr Dimension height = detail::Max<WidgetsTuple, detail::WidgetHeightField>::value;

                static void after (Context const &ctx) {}
                static constexpr Dimension getHeightIncrement (Dimension /* d */) { return 0; }
        };

        struct NoDecoration {
                static constexpr Dimension height = 0;
                static constexpr void after (Context const & /* ctx */) {}
                static constexpr Dimension getHeightIncrement (Dimension /* d */) { return 0; }
        };

} // namespace detail

/****************************************************************************/

/**
 * Radio group.
 */
template <typename Callback, typename WidgetTuple> class Group {
public:
        Group (Callback const &c, WidgetTuple const &wt) : widgets{wt}, callback{c} {}

        static constexpr Focus focusableWidgetCount = detail::Sum<WidgetTuple, detail::FocusableWidgetCountField>::value;

        using Children = WidgetTuple;
        Children &getWidgets () { return widgets; }

private:
        WidgetTuple widgets;
        Callback callback;
};

template <typename T> struct is_group : public std::bool_constant<false> {
};

template <typename Callback, typename WidgetTuple> struct is_group<Group<Callback, WidgetTuple>> : public std::bool_constant<true> {
};

namespace c {
        template <typename T>
        concept group = is_group<T>::value;
} // namespace c

template <typename Callback, typename... W> auto group (Callback const &c, W const &...widgets)
{
        return Group{c, std::make_tuple (widgets...)};
}

/****************************************************************************/

namespace detail {
        template <bool frame> struct FrameHelper {
                static constexpr Coordinate offset = 0;
                static constexpr Dimension cut = 0;
        };

        template <> struct FrameHelper<true> {
                static constexpr Coordinate offset = 1;
                static constexpr Dimension cut = 2;
        };
} // namespace detail

/**
 * A window
 */
template <Coordinate ox, Coordinate oy, Dimension widthV, Dimension heightV, bool frame, typename ChildT> struct Window {

        using Child = ChildT;
        explicit Window (Child c) : child{std::move (c)} {}

        // TODO This always prints the frame. There should be option for a windows without one.
        template <typename Wrapper> Visibility operator() (auto &d, Context & /* ctx */) const
        {
                d.setCursorX (ox);
                d.setCursorY (oy);

                if constexpr (frame) {
                        d.print ("┌"); // TODO print does not move cursor, but line does. Inconsistent.
                        d.move (1, 0);
                        detail::line (d, width - F::cut);
                        d.print ("┐");

                        for (int i = 0; i < height - F::cut; ++i) {
                                d.setCursorX (ox);
                                d.move (0, 1);
                                d.print ("│");
                                // d.move (width - 1, 0);
                                d.move (1, 0);
                                detail::line (d, width - F::cut, " ");
                                d.print ("│");
                        }

                        d.setCursorX (ox);
                        d.move (0, 1);
                        d.print ("└");
                        d.move (1, 0);
                        detail::line (d, width - F::cut);
                        d.print ("┘");

                        d.setCursorX (ox + F::offset);
                        d.setCursorY (oy + F::offset);
                }

                return Visibility::visible;
        }

        static constexpr Coordinate x = ox;
        static constexpr Coordinate y = oy;
        static constexpr Dimension width = widthV;   // Dimensions in characters
        static constexpr Dimension height = heightV; // Dimensions in characters
        static constexpr Focus focusableWidgetCount = Child::focusableWidgetCount;

        using F = detail::FrameHelper<frame>;
        Child child;
};

template <typename T> struct is_window : public std::bool_constant<false> {
};

template <Coordinate ox, Coordinate oy, Dimension widthV, Dimension heightV, bool frame, typename ChildT>
struct is_window<Window<ox, oy, widthV, heightV, frame, ChildT>> : public std::bool_constant<true> {
};

namespace c {
        template <typename T>
        concept window = is_window<T>::value;
}

namespace detail {
        template <Coordinate ox, Coordinate oy, Dimension widthV, Dimension heightV, bool frame = false> auto windowRaw (auto &&c)
        {
                return Window<ox, oy, widthV, heightV, frame, std::remove_reference_t<decltype (c)>> (std::forward<decltype (c)> (c));
        }
} // namespace detail

/****************************************************************************/

// Forward declaration for is_layout
template <template <typename Wtu> typename Decor, typename WidgetsTuple> struct Layout;
// template <typename T> void log (T const &t, int indent);

template <typename Wtu> class DefaultDecor {
};

/// Type trait for discovering Layouts
template <typename T, template <typename Wtu> typename Decor = DefaultDecor, typename WidgetsTuple = Empty>
struct is_layout : public std::bool_constant<false> {
};

template <template <typename Wtu> typename Decor, typename WidgetsTuple>
struct is_layout<Layout<Decor, WidgetsTuple>> : public std::bool_constant<true> {
};

namespace c {
        template <typename T>
        concept layout = is_layout<T>::value;
}

namespace detail {

        template <typename T> void log (T const &t, int indent = 0);

        namespace augment {

                /**
                 * Additional information for all the widgetc contained in the Layout.
                 */
                template <c::widget T, Focus focusIndexV, Selection radioIndexV, Coordinate yV> class Widget {
                public:
                        using Wrapped = T;

                        constexpr Widget (T t) : widget{std::move (t)} {}

                        /// Height in characters.
                        static constexpr Dimension getHeight () { return T::height; }

                        /// Consecutive number in a radio group.
                        static constexpr Selection getRadioIndex () { return radioIndexV; }

                        /// Consecutive number (starting from 0) assigned for every focusable widget.
                        static constexpr Focus getFocusIndex () { return focusIndexV; }

                        /// Position starting from the top.
                        static constexpr Coordinate getY () { return yV; }

                        Visibility operator() (auto &d, Context const *ctx) const
                        {
                                if (!detail::heightsOverlap (getY (), getHeight (), ctx->currentScroll, ctx->dimensions.height)) {
                                        return Visibility::outside;
                                }

                                return widget.template operator()<Widget> (d, *ctx);
                        }

                        void input (auto &d, Context const &ctx, char c)
                        {
                                if (ctx.currentFocus == getFocusIndex ()) {
                                        if constexpr (requires { widget.template input<Widget> (d, ctx, c); }) {
                                                widget.template input<Widget> (d, ctx, c);
                                        }
                                }
                        }

                        void scrollToFocus (Context *ctx) const;

                        T widget; // Wrapped widget
                private:
                        // friend void log<T> (T const &t, int indent);
                };

                template <typename T> struct is_widget_wrapper : public std::bool_constant<false> {
                };

                template <typename T, Focus focusIndexV, Selection radioIndexV, Coordinate yV>
                class is_widget_wrapper<Widget<T, focusIndexV, radioIndexV, yV>> : public std::bool_constant<true> {
                };

                template <typename T>
                concept widget_wrapper = is_widget_wrapper<T>::value;

                template <c::widget T, Focus focusIndexV, Selection radioIndexV, Coordinate yV>
                void Widget<T, focusIndexV, radioIndexV, yV>::scrollToFocus (Context *ctx) const
                {
                        if (!detail::heightsOverlap (getY (), getHeight (), ctx->currentScroll, ctx->dimensions.height)) {
                                if (ctx->currentFocus == getFocusIndex ()) {
                                        if (getY () < ctx->currentScroll) {
                                                ctx->currentScroll = getY ();
                                        }
                                        else {
                                                ctx->currentScroll = getY () - ctx->dimensions.height + 1;
                                        }
                                }
                        }
                }

                /**
                 * Base for widgets in the augment:: namespace that can contain other widgets.
                 */
                template <typename ConcreteClass, typename Decor> struct ContainerWidget {

                        // TODO this class is exposed to the users. I want this methid to be private
                        Visibility operator() (auto &d, Context *ctx) const
                        {
                                if (!detail::heightsOverlap (ConcreteClass::getY (), ConcreteClass::getHeight (), ctx->currentScroll,
                                                             ctx->dimensions.height)) {
                                        return Visibility::outside;
                                }

                                // Groups contain radioSelection
                                if constexpr (requires { ConcreteClass::radioSelection; }) {
                                        ctx->radioSelection = &static_cast<ConcreteClass const *> (this)->radioSelection;
                                }

                                ctx = changeContext (ctx);

                                // Some composite widgets have their own display method (operator ()). For instance og::Window
                                if constexpr (requires {
                                                      static_cast<ConcreteClass const *> (this)->widget.template operator()<ConcreteClass> (
                                                              d, *ctx);
                                              }) {
                                        static_cast<ConcreteClass const *> (this)->widget.template operator()<ConcreteClass> (d, *ctx);
                                }

                                auto l = [&d, &ctx] (auto &itself, auto const &child, auto const &...children) {
                                        if (child (d, ctx) == Visibility::visible) {
                                                constexpr bool lastWidgetInLayout = (sizeof...(children) == 0);

                                                if (!lastWidgetInLayout) {
                                                        Decor::after (*ctx);
                                                }
                                        }

                                        if constexpr (sizeof...(children) > 0) {
                                                itself (itself, children...);
                                        }
                                };

                                std::apply ([&l] (auto const &...children) { l (l, children...); },
                                            static_cast<ConcreteClass const *> (this)->children);

                                return Visibility::visible;
                        }

                        Visibility operator() (auto &d) const { return operator() (d, &d.context); }

                        void input (auto &d, Context &ctx, char c)
                        {
                                if constexpr (requires { ConcreteClass::radioSelection; }) {
                                        ctx.radioSelection = &static_cast<ConcreteClass *> (this)->radioSelection;
                                }

                                auto l = [&d, &ctx, c] (auto &itself, auto &child, auto &...children) {
                                        child.input (d, ctx, c);

                                        if constexpr (sizeof...(children) > 0) {
                                                itself (itself, children...);
                                        }
                                };

                                std::apply ([&l] (auto &...children) { l (l, children...); }, static_cast<ConcreteClass *> (this)->children);
                        }

                        void input (auto &d, char c)
                        {
                                Context *ctx = d.getContext ();

                                // Windows contain their own context
                                if constexpr (requires { ConcreteClass::context; }) {
                                        ctx = &static_cast<ConcreteClass const *> (this)->context;
                                }

                                input (d, *ctx, c);
                        }

                        void incrementFocus (auto &display) const
                        {
                                Context *ctx = display.getContext ();

                                // Windows contain their own context
                                if constexpr (requires { ConcreteClass::context; }) {
                                        ctx = &static_cast<ConcreteClass const *> (this)->context;
                                }

                                if (ctx->currentFocus < ConcreteClass::Wrapped::focusableWidgetCount - 1) {
                                        ++ctx->currentFocus;
                                        scrollToFocus (ctx);
                                }
                        }

                        void decrementFocus (auto &display) const
                        {
                                Context *ctx = display.getContext ();

                                // Windows contain their own context
                                if constexpr (requires { ConcreteClass::context; }) {
                                        ctx = &static_cast<ConcreteClass const *> (this)->context;
                                }

                                if (ctx->currentFocus > 0) {
                                        --ctx->currentFocus;
                                        scrollToFocus (ctx);
                                }
                        }

                        void scrollToFocus (Context *ctx) const
                        {
                                // ctx = changeContext (ctx);

                                auto l = [&ctx] (auto &itself, auto const &child, auto const &...children) {
                                        child.scrollToFocus (ctx);

                                        if constexpr (sizeof...(children) > 0) {
                                                itself (itself, children...);
                                        }
                                };

                                std::apply ([&l] (auto const &...children) { l (l, children...); },
                                            static_cast<ConcreteClass const *> (this)->children);
                        }

                private:
                        Context *changeContext (Context *ctx) const
                        {
                                // Windows contain their own context
                                if constexpr (requires { ConcreteClass::context; }) {
                                        Context *newContext = &static_cast<ConcreteClass const *> (this)->context;
                                        // There can be many contexts, but only one cursor (assinged to a display).
                                        newContext->cursor = ctx->cursor;
                                        return newContext;
                                }

                                return ctx;
                        }
                };

                template <c::layout T, typename WidgetTuple, Coordinate yV>
                class Layout : public ContainerWidget<Layout<T, WidgetTuple, yV>, typename T::DecoratorType> {
                public:
                        using Wrapped = T;
                        constexpr Layout (Wrapped t, WidgetTuple c) : widget{std::move (t)}, children{std::move (c)} {}

                        static constexpr Dimension getHeight () { return Wrapped::template Decorator<WidgetTuple>::height; }
                        static constexpr Coordinate getY () { return yV; }

                        Wrapped widget; // TODO make private. I failed to add frined decl. for log function here. Ran into spiral of
                                        // template error giberish.

                        WidgetTuple children;

                private:
                        friend ContainerWidget<Layout, typename T::DecoratorType>;
                };

                template <typename T> struct is_layout_wrapper : public std::bool_constant<false> {
                };

                template <typename T, typename WidgetTuple, Coordinate yV>
                class is_layout_wrapper<Layout<T, WidgetTuple, yV>> : public std::bool_constant<true> {
                };

                template <typename T>
                concept layout_wrapper = is_layout_wrapper<T>::value;

                template <typename T, typename Child, Coordinate yV, Dimension heightV>
                class Window : public ContainerWidget<Window<T, Child, yV, heightV>, NoDecoration> {
                public:
                        using Wrapped = T;
                        constexpr Window (Wrapped t, Child c) : widget{std::move (t)}, children{std::move (c)} {}

                        static constexpr Dimension getHeight () { return heightV; }
                        static constexpr Coordinate getY () { return yV; }

                private:
                        Wrapped widget;
                        std::tuple<Child> children;
                        friend ContainerWidget<Window, NoDecoration>;
                        using F = T::F;
                        mutable Context context{
                                nullptr, {Wrapped::x + F::offset, Wrapped::y + F::offset}, {Wrapped::width - F::cut, Wrapped::height - F::cut}};
                };

                template <typename T> struct is_window_wrapper : public std::bool_constant<false> {
                };

                template <typename T, typename Child, Coordinate yV, Dimension heightV>
                class is_window_wrapper<Window<T, Child, yV, heightV>> : public std::bool_constant<true> {
                };

                template <typename T>
                concept window_wrapper = is_window_wrapper<T>::value;

                template <typename T, typename WidgetTuple, Coordinate yV, Dimension heightV, typename Decor>
                class Group : public ContainerWidget<Group<T, WidgetTuple, yV, heightV, Decor>, Decor> {
                public:
                        using Wrapped = T;
                        constexpr Group (Wrapped t, WidgetTuple c) : widget{std::move (t)}, children{std::move (c)} {}

                        static constexpr Dimension getHeight () { return heightV; }
                        static constexpr Coordinate getY () { return yV; }

                        Wrapped widget;
                        WidgetTuple children;

                private:
                        friend ContainerWidget<Group, Decor>;
                        mutable Selection radioSelection{};
                };

                template <typename T> struct is_group_wrapper : public std::bool_constant<false> {
                };

                template <typename T, typename WidgetTuple, Coordinate yV, Dimension heightV, typename Decor>
                class is_group_wrapper<Group<T, WidgetTuple, yV, heightV, Decor>> : public std::bool_constant<true> {
                };

                template <typename T>
                concept group_wrapper = is_group_wrapper<T>::value;

                template <typename GrandParent, typename Parent> constexpr Dimension getHeightIncrement (Dimension d)
                {
                        // Parent : og::Layout or og::Group
                        if constexpr (is_layout<Parent>::value) {
                                return Parent::DecoratorType::getHeightIncrement (d);
                        }
                        else {
                                return GrandParent::DecoratorType::getHeightIncrement (d);
                        }
                }

        } // namespace augment

        template <Focus f, Coordinate y, typename GrandParent, typename Parent, typename ChildrenTuple>
        constexpr auto transform (ChildrenTuple &tuple);

        // Wrapper for ordinary widgets
        template <typename T, typename Parent, Focus f, Selection r, Coordinate y> struct Wrap {

                template <typename W> static auto wrap (W &&t) { return augment::Widget<T, f, r, y>{std::forward<W> (t)}; }
        };

        // Wrapper for layouts
        template <c::layout T, typename Parent, Focus f, Selection r, Coordinate y>
        requires c::layout<Parent> || c::window<Parent> || std::same_as<Parent, void>
        struct Wrap<T, Parent, f, r, y> {

                template <typename W> static auto wrap (W &&t)
                {
                        return augment::Layout<T, decltype (transform<f, y, Parent, T> (t.getWidgets ())), y>{
                                std::forward<W> (t), transform<f, y, Parent, T> (t.getWidgets ())};
                }
        };

        // Wrapper for groups
        template <c::group T, c::layout Parent, Focus f, Selection r, Coordinate y> struct Wrap<T, Parent, f, r, y> {

                template <typename W> static auto wrap (W &&t)
                {
                        return augment::Group<T, decltype (transform<f, y, Parent, T> (t.getWidgets ())), y,
                                              Parent::template Decorator<typename T::Children>::height, typename Parent::DecoratorType>{
                                std::forward<W> (t), transform<f, y, Parent, T> (t.getWidgets ())};
                }
        };

        // Partial specialization for Windows
        template <c::window T, typename Parent, Focus f, Selection r, Coordinate y>
        requires std::same_as<Parent, void> // Means that windows are always top level
        struct Wrap<T, Parent, f, r, y> {

                static constexpr auto oy = T::y;
                static constexpr auto heightV = T::height;
                using Child = T::Child;

                template <typename W> static auto wrap (W &&t)
                {
                        return augment::Window<T, decltype (Wrap<Child, T, f, r, y>::wrap (t.child)), oy, heightV> (
                                std::forward<W> (t), Wrap<Child, T, f, r, y>::wrap (t.child));
                }
        };

        template <typename T, Focus f = 0, Selection r = 0, Coordinate y = 0> auto wrap (T &&t)
        {
                return Wrap<std::decay_t<T>, void, f, r, y>::wrap (std::forward<T> (t));
        }

        template <Focus f, Selection r, Coordinate y, typename GrandParent, typename Parent, typename Tuple, typename T, typename... Ts>
        constexpr auto transformImpl (Tuple &&prev, T &t, Ts &...ts)
        {
                using WidgetType = std::remove_cvref_t<T>;
                using Wrapper = Wrap<WidgetType, Parent, f, r, y>;
                using Wrapped = decltype (Wrapper::wrap (t));
                auto a = std::make_tuple (Wrapper::wrap (t));

                if constexpr (sizeof...(Ts) > 0) { // Parent(Layout)::Decor::filter -> hbox return 0 : vbox return height
                        constexpr auto childHeight = Wrapped::getHeight ();
                        constexpr auto childHeightIncrement = augment::getHeightIncrement<GrandParent, Parent> (childHeight);

                        return transformImpl<f + getFocusableWidgetCount<WidgetType> (), r + 1, y + childHeightIncrement, GrandParent, Parent> (
                                std::tuple_cat (prev, a), ts...);
                }
                else {
                        return std::tuple_cat (prev, a);
                }
        }

        template <Focus f, Coordinate y, typename GrandParent, typename Parent, typename ChildrenTuple>
        constexpr auto transform (ChildrenTuple &tuple) // TODO use concept instead of "Children" prefix.
        {
                return std::apply (
                        [] (auto &...element) {
                                std::tuple dummy{};
                                return transformImpl<f, 0, y, GrandParent, Parent> (dummy, element...);
                        },
                        tuple);
        }

} // namespace detail

/*--------------------------------------------------------------------------*/

namespace detail {
        template <typename T> void log (T const &t, int indent)
        {
                auto l = [indent]<typename Wrapper> (auto &itself, Wrapper const &w, auto const &...ws) {
                        using Wrapped = typename Wrapper::Wrapped;

                        // std::string used only for debug.
                        std::cout << std::string (indent, ' ') << "focusIndex: ";

                        if constexpr (requires {
                                              Wrapped::canFocus;
                                              requires Wrapped::canFocus == 1;
                                      }) {
                                std::cout << w.getFocusIndex ();
                        }
                        else {
                                std::cout << "NA";
                        }

                        std::cout << ", radioIndex: ";

                        if constexpr (is_radio<Wrapped>::value) {
                                std::cout << int (w.getRadioIndex ());
                        }
                        else {
                                std::cout << "NA";
                        }

                        std::cout << ", y: " << w.getY () << ", height: " << w.getHeight () << ", " << typeid (w.widget).name () << std::endl;

                        if constexpr (requires (decltype (w) x) { x.children; }) {
                                log (w.children, indent + 2);
                        }

                        if constexpr (sizeof...(ws) > 0) {
                                itself (itself, ws...);
                        }
                };

                std::apply ([&l] (auto const &...widgets) { l (l, widgets...); }, t);
        }
} // namespace detail

template <typename T> void log (T &&t) { detail::log (std::make_tuple (std::forward<T> (t))); }

/**
 * Container for other widgtes.
 */
template <template <typename Wtu> typename Decor, typename WidgetsTuple> class Layout {
public:
        static constexpr Focus focusableWidgetCount = detail::Sum<WidgetsTuple, detail::FocusableWidgetCountField>::value;
        template <typename Wtu> using Decorator = Decor<Wtu>;
        using DecoratorType = Decor<WidgetsTuple>;

        explicit Layout (WidgetsTuple w) : widgets{std::move (w)} {}
        WidgetsTuple &getWidgets () { return widgets; }

private:
        WidgetsTuple widgets;
};

/*--------------------------------------------------------------------------*/

template <typename... W> auto vbox (W const &...widgets)
{
        using WidgetsTuple = decltype (std::tuple (widgets...));
        auto vbox = Layout<detail::VBoxDecoration, WidgetsTuple>{std::tuple (widgets...)};
        return vbox;
}

template <typename... W> auto hbox (W const &...widgets)
{
        using WidgetsTuple = decltype (std::tuple (widgets...));
        auto hbox = Layout<detail::HBoxDecoration, WidgetsTuple>{std::tuple (widgets...)};
        return hbox;
}

/****************************************************************************/

/// window factory function has special meaning that is also wraps.
template <Coordinate ox, Coordinate oy, Dimension widthV, Dimension heightV, bool frame = false, typename W = void> auto window (W &&c)
{
        return detail::wrap (detail::windowRaw<ox, oy, widthV, heightV, frame> (std::forward<W> (c)));
}

void draw (auto &display, detail::augment::window_wrapper auto const &window)
{
        display.clear ();
        window (display);
        display.refresh ();
}

/****************************************************************************/

static_assert (c::widget<Line<0, '-'>>);
static_assert (c::widget<Check>);
static_assert (c::widget<Radio<int>>);
static_assert (c::widget<Label>);
static_assert (c::widget<Button<decltype ([] {})>>);
// These does not have the operator () and the height field

using MyLayout = Layout<detail::VBoxDecoration, decltype (std::tuple{check ("")})>;
static_assert (!c::widget<MyLayout>);
static_assert (!c::widget<Group<decltype ([] {}), decltype (std::make_tuple (radio (1, "")))>>);
static_assert (!c::widget<Window<0, 0, 0, 0, false, Label>>);

static_assert (is_layout<MyLayout>::value);
static_assert (!is_layout<decltype (check (""))>::value);
static_assert (!c::layout<Label>);
static_assert (c::layout<MyLayout>);

static_assert (!c::group<int>);
static_assert (!c::group<Label>);
static_assert (!c::group<MyLayout>);
static_assert (c::group<Group<decltype ([] {}), decltype (std::make_tuple (radio (1, "")))>>);

static_assert (c::window<Window<0, 0, 0, 0, false, Label>>);
static_assert (!c::window<Label>);

// static_assert (detail::augment::widget_wrapper<detail::augment::Widget<Label, 0, 0, 0>>);
// static_assert (!detail::augment::widget_wrapper<detail::augment::Layout<MyLayout, std::tuple<int>, 0>>);
// static_assert (!detail::augment::widget_wrapper<detail::augment::Window<int, int, 0, 0>>);
// static_assert (!detail::augment::widget_wrapper<detail::augment::Group<int, std::tuple<int>, 0, 0, DefaultDecor<int>>>);

// static_assert (!detail::augment::layout_wrapper<detail::augment::Widget<Label, 0, 0, 0>>);
// static_assert (detail::augment::layout_wrapper<detail::augment::Layout<MyLayout, std::tuple<int>, 0>>);
// static_assert (!detail::augment::layout_wrapper<detail::augment::Window<int, int, 0, 0>>);
// static_assert (!detail::augment::layout_wrapper<detail::augment::Group<int, std::tuple<int>, 0, 0, DefaultDecor<int>>>);

// static_assert (!detail::augment::window_wrapper<detail::augment::Widget<Label, 0, 0, 0>>);
// static_assert (!detail::augment::window_wrapper<detail::augment::Layout<MyLayout, std::tuple<int>, 0>>);
// static_assert (detail::augment::window_wrapper<detail::augment::Window<int, int, 0, 0>>);
// static_assert (!detail::augment::window_wrapper<detail::augment::Group<int, std::tuple<int>, 0, 0, DefaultDecor<int>>>);

// static_assert (!detail::augment::group_wrapper<detail::augment::Widget<Label, 0, 0, 0>>);
// static_assert (!detail::augment::group_wrapper<detail::augment::Layout<MyLayout, std::tuple<int>, 0>>);
// static_assert (!detail::augment::group_wrapper<detail::augment::Window<int, int, 0, 0>>);
// static_assert (detail::augment::group_wrapper<detail::augment::Group<int, std::tuple<int>, 0, 0, DefaultDecor<int>>>);

} // namespace og

/****************************************************************************/

// int test1 ()
// {
//         using namespace og;

//         /*
//          * TODO This has the problem that it woud be tedious and wasteful to declare ncurses<18, 7>
//          * with every new window/view. Alas I'm trying to investigate how to commonize Displays and Windows.
//          *
//          * This ncurses method can be left as an option.
//          */
//         auto vb = ncurses<18, 7> (vbox (vbox (radio (0, " A "), radio (1, " B "), radio (2, " C "), radio (3, " d ")), //
//                                         line<10>,                                                                      //
//                                         vbox (check (" 1 "), check (" 2 "), check (" 3 "), check (" 4 ")),             //
//                                         line<10>,                                                                      //
//                                         vbox (radio (0, " a "), radio (0, " b "), radio (0, " c "), radio (0, " d ")), //
//                                         line<10>,                                                                      //
//                                         vbox (check (" 5 "), check (" 6 "), check (" 7 "), check (" 8 ")),             //
//                                         line<10>,                                                                      //
//                                         vbox (radio (0, " E "), radio (0, " F "), radio (0, " G "), radio (0, " H "))  //
//                                         ));                                                                            //

//         // auto dialog = window<2, 2, 10, 10> (vbox (radio (" A "), radio (" B "), radio (" C "), radio (" d ")));
//         // // dialog.calculatePositions ();

//         bool showDialog{};

//         while (true) {
//                 // d1.clear ()
//                 // vb (d1);

//                 vb ();

//                 // if (showDialog) {
//                 //         dialog (d1);
//                 // }

//                 // wrefresh (d1.win);
//                 int ch = getch ();

//                 if (ch == 'q') {
//                         break;
//                 }

//                 switch (ch) {
//                 case KEY_DOWN:
//                         vb.incrementFocus ();
//                         break;

//                 case KEY_UP:
//                         vb.decrementFocus ();
//                         break;

//                 case 'd':
//                         showDialog = true;
//                         break;

//                 default:
//                         // d1.input (vb, char (ch));
//                         // vb.input (d1, char (ch), 0, 0, dummy); // TODO ugly, should be vb.input (d1, char (ch)) at most
//                         break;
//                 }
//         }

//         return 0;
// }

/****************************************************************************/

int test2 ()
{
        using namespace og;
        NcursesDisplay<18, 7> d1;

        bool showDialog{};

        auto x = window<0, 0, 18, 7> (
                vbox (hbox (label ("Hello "), check (" 1 "), check (" 2 ")),                                                        //
                      hbox (label ("World "), check (" 5 "), check (" 6 ")),                                                        //
                      button ("Open dialog", [&showDialog] { showDialog = true; }),                                                 //
                      line<18>,                                                                                                     //
                      group ([] (auto const &o) {}, radio (0, " R "), radio (1, " G "), radio (1, " B "), radio (1, " A ")),        //
                      line<18>,                                                                                                     //
                      hbox (group ([] (auto const &o) {}, radio (0, " R "), radio (1, " G "), radio (1, " B "), radio (1, " A "))), //
                      line<18>,                                                                                                     //
                      Combo (Options (option (0, "red"), option (1, "green"), option (1, "blue")), [] (auto const &o) {}),          //
                      line<18>,                                                                                                     //
                      hbox (button ("Aaa", [] {}), hspace<1>, button ("Bbb", [] {}), hspace<1>, button ("Ccc", [] {})),             //
                      line<18>,                                                                                                     //
                      check (" 1 "),                                                                                                //
                      check (" 2 "),                                                                                                //
                      check (" 3 "),                                                                                                //
                      check (" 4 "),                                                                                                //
                      check (" 5 "),                                                                                                //
                      check (" 6 "),                                                                                                //
                      check (" 7 "),                                                                                                //
                      check (" 8 "),                                                                                                //
                      check (" 9 "),                                                                                                //
                      check (" 10 "),                                                                                               //
                      check (" 11 "),                                                                                               //
                      check (" 12 "),                                                                                               //
                      check (" 13 "),                                                                                               //
                      check (" 14 "),                                                                                               //
                      check (" 15 ")));

        // log (x);

        auto dialog = window<4, 1, 10, 5, true> (vbox (label ("  PIN:"),  //
                                                       label (" 123456"), //
                                                       hbox (button ("[OK]", [&showDialog] { showDialog = false; }), button ("[Cl]", [] {})),
                                                       check (" 15 ")));

        // log (dialog);

        // TODO simplify this mess to a few lines. Minimal verbosity.
        while (true) {

                if (showDialog) {
                        draw (d1, dialog);
                }
                else {
                        draw (d1, x);
                }

                int ch = getch ();

                if (ch == 'q') {
                        break;
                }

                switch (ch) {
                case KEY_DOWN:
                        if (showDialog) {
                                dialog.incrementFocus (d1);
                        }
                        else {
                                x.incrementFocus (d1);
                        }
                        break;

                case KEY_UP:
                        if (showDialog) {
                                dialog.decrementFocus (d1);
                        }
                        else {
                                x.decrementFocus (d1);
                        }
                        break;

                case 'd':
                        showDialog = true;
                        break;

                default:
                        if (showDialog) {
                                dialog.input (d1, char (ch));
                        }
                        else {
                                x.input (d1, char (ch));
                        }
                        break;
                }
        }

        return 0;
}

/****************************************************************************/

int main ()
{

        test2 ();
        // test1 ();
}
