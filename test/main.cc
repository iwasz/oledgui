/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "oledgui.h"
#include <algorithm>
#include <array>
#include <bits/utility.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
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
        and this method returns that number. This is calculated in a similar way as the previous
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
using LineOffset = int;

class Point {
public:
        constexpr Point (Coordinate x = 0, Coordinate y = 0) : x_{x}, y_{y} {}
        Point (Point const &) = default;
        Point &operator= (Point const &p) = default;
        Point (Point &&) = default;
        Point &operator= (Point &&p) = default;
        ~Point () = default;

        Point &operator+= (Point const &p)
        {
                x_ += p.x_;
                y_ += p.y_;
                return *this;
        }

        Point &operator-= (Point const &p)
        {
                x_ -= p.x_;
                y_ -= p.y_;
                return *this;
        }

        Point operator+ (Point p) const
        {
                p += *this;
                return p;
        }

        Point operator- (Point const &p)
        {
                Point ret = *this;
                ret -= p;
                return ret;
        }

        Coordinate &x () { return x_; }
        Coordinate x () const { return x_; }
        Coordinate &y () { return y_; }
        Coordinate y () const { return y_; }

private:
        Coordinate x_;
        Coordinate y_;
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

enum class Key { unknown, incrementFocus, decrementFocus, select };

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

        static constexpr Dimension width = widthV;   // Dimensions in characters
        static constexpr Dimension height = heightV; // Dimensions in characters

        Point &cursor () { return cursor_; }
        Point const &cursor () const { return cursor_; }

protected:
        Point cursor_{};
        Child child;
};

/**
 * Ncurses backend.
 */
template <Dimension widthV, Dimension heightV, typename Child = Empty>
class NcursesDisplay : public Display<NcursesDisplay<widthV, heightV, Child>, widthV, heightV, Child> {
public:
        using Base = Display<NcursesDisplay<widthV, heightV, Child>, widthV, heightV, Child>;
        using Base::cursor;
        using Base::width, Base::height;

        NcursesDisplay (Child c = {});
        ~NcursesDisplay ()
        {
                clrtoeol ();
                refresh ();
                endwin ();
        }

        void print (const char *str) { mvwprintw (win, cursor ().y (), cursor ().x (), str); }

        void clear ()
        {
                wclear (win);
                cursor ().x () = 0;
                cursor ().y () = 0;
        }

        void color (Color c) { wattron (win, COLOR_PAIR (c)); }

        void refresh ()
        {
                ::refresh ();
                wrefresh (win);
        }

private:
        using Base::child;
        WINDOW *win{};
};

template <Dimension widthV, Dimension heightV, typename Child> NcursesDisplay<widthV, heightV, Child>::NcursesDisplay (Child c) : Base (c)
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

og::Key getKey (int ch)
{
        // TODO characters must be customizable (compile time)
        switch (ch) {
        case KEY_DOWN:
                return Key::incrementFocus;

        case KEY_UP:
                return Key::decrementFocus;

        case int (' '):
                return Key::select;

        default:
                return Key::unknown;
        }
}

og::Key getKey () { return getKey (getch ()); }

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
                        d.cursor () += {1, 0};
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

template <uint16_t len> Line<len, '-'> const line;
template <uint16_t len = 1> Line<len, ' '> const hspace;

/****************************************************************************/
/* Check                                                                    */
/****************************************************************************/

class Check : public Focusable {
public:
        static constexpr Dimension height = 1;

        constexpr Check (const char *s, bool c) : label_{s}, checked_{c} {}

        template <typename Wrapper> Visibility operator() (auto &d, Context const &ctx) const;
        template <typename Wrapper> void input (auto & /* d */, Context const & /* ctx */, Key c)
        {
                if (c == Key::select) {
                        checked_ = !checked_;
                }
        }

        bool &checked () { return checked_; }
        bool const &checked () const { return checked_; }

        char const *&label () { return label_; }
        char const *const &label () const { return label_; }

private:
        char const *label_{};
        bool checked_{};
};

/*--------------------------------------------------------------------------*/

template <typename Wrapper> Visibility Check::operator() (auto &d, Context const &ctx) const
{
        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                d.color (2);
        }

        if (checked_) {
                d.print ("☑");
        }
        else {
                d.print ("☐");
        }

        d.cursor () += {1, 0};
        d.print (label_);

        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                d.color (1);
        }

        d.cursor () += {Dimension (strlen (label_)), 0}; // TODO constexpr strings?
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
        template <typename Wrapper> void input (auto &d, Context const &ctx, Key c);

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

        d.cursor () += {1, 0};
        d.print (label);

        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                d.color (1);
        }

        d.cursor () += {Dimension (strlen (label)), 0};
        return Visibility::visible;
}

template <std::integral Id> template <typename Wrapper> void Radio<Id>::input (auto & /* d */, Context const &ctx, Key c)
{
        if (ctx.radioSelection != nullptr && c == Key::select) {
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

template <typename T>
concept text_buffer = requires (T buf)
{
        {
                buf.size ()
                } -> std::convertible_to<std::size_t>;

        // typename T::const_iterator; // TODO add
        // buf.cbegin();
        // buf.cend ();
};

/**
 * Multiline text
 */
template <Dimension widthV, Dimension heightV, text_buffer Buffer> class Text {
public:
        using BufferType = std::remove_reference_t<Buffer>;
        static constexpr Dimension height = heightV;

        /// Tip use std::ref
        constexpr explicit Text (Buffer const &b) : buffer{b} {}

        template <typename /* Wrapper */> Visibility operator() (auto &d, Context const & /* ctx */) const;

        typename BufferType::const_iterator skipToLine (LineOffset line)
        {
                size_t charactersToSkip = line * widthV;
                return std::next (buffer.cbegin (), std::min (charactersToSkip, buffer.size ()));
        }

        LineOffset setStartLine (LineOffset line)
        {
                size_t len = buffer.size ();
                size_t linesNumber = len / widthV + Dimension ((len % widthV) > 0);
                LineOffset maxStartLine = std::max (0, int (linesNumber - heightV));
                startLine = std::min (std::max (line, 0), maxStartLine);
                start = skipToLine (startLine);
                return startLine;
        }

private:
        Buffer buffer;
        typename BufferType::const_iterator start = buffer.cbegin ();
        LineOffset startLine{};
        bool scrollToBottom{};
};

template <Dimension widthV, Dimension heightV, text_buffer Buffer>
template <typename Wrapper>
Visibility Text<widthV, heightV, Buffer>::operator() (auto &d, Context const &ctx) const
{
        size_t widgetScroll = std::max (ctx.currentScroll - Wrapper::getY (), 0);
        size_t heightToPrint = heightV - widgetScroll;

        Dimension len = buffer.size ();
        std::array<char, widthV + 1> line;
        size_t totalCharactersCopied{};
        size_t linesPrinted{};

        // start = skipToLine (startLine + widgetScroll);
        size_t charactersToSkip = widgetScroll * widthV;
        auto iter = std::next (start, std::min (charactersToSkip, buffer.size ()));

        // auto iter = start;
        Point tmpCursor = d.cursor ();
        while (totalCharactersCopied < len && linesPrinted++ < heightToPrint) {
                // TODO this line is wasteful. What could have been tracked easily by incrementing/decrementing a variable is otherwise counted
                // in a loop every iteration.
                auto lineCharactersCopied = std::min (size_t (std::distance (iter, buffer.cend ())), size_t (widthV));
                auto i = std::copy_n (iter, lineCharactersCopied, line.begin ());
                *i = '\0';
                std::advance (iter, lineCharactersCopied);

                totalCharactersCopied += lineCharactersCopied;
                d.print (line.data ());
                d.cursor () += {0, 1};
        }

        d.cursor () = tmpCursor + Point{widthV - 1, heightV - 1};
        return Visibility::visible;
}

template <Dimension widthV, Dimension heightV, typename Buffer> auto text (Buffer &&b)
{
        return Text<widthV, heightV, std::unwrap_ref_decay_t<Buffer>>{std::forward<Buffer> (b)};
}

/****************************************************************************/
/* Label                                                                    */
/****************************************************************************/

/**
 *
 */
class Label {
public:
        static constexpr Dimension height = 1;

        constexpr Label (const char *l) : label{l} {}

        template <typename /* Wrapper */> Visibility operator() (auto &d, Context const & /* ctx */) const
        {
                d.print (label);
                d.cursor () += {Dimension (strlen (label)), 0};
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

        template <typename> void input (auto & /* d */, Context const & /* ctx */, Key c)
        {
                if (c == Key::select) {
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
        d.cursor () += {Dimension (strlen (label)), 0};

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
        template <typename Wrapper> void input (auto &d, Context const &ctx, Key c);

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
void Combo<OptionCollection, Callback>::input (auto & /* d */, Context const &ctx, Key c)
{
        if (ctx.currentFocus == Wrapper::getFocusIndex () && c == Key::select) {
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
                        // focusableWidgetCount field is used in container widgets like Layout and Group.
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
                static constexpr auto value = getFocusableWidgetCount<std::remove_reference_t<std::unwrap_ref_decay_t<T>>> ();
        };

        template <typename T> struct WidgetHeightField {
                static constexpr auto value = getHeight<std::remove_reference_t<std::unwrap_ref_decay_t<T>>> ();
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

                static void after (Context const &ctx, auto &display)
                {
                        display.cursor ().y () += 1;
                        display.cursor ().x () = ctx.origin.x ();
                }
                static constexpr Dimension getHeightIncrement (Dimension d) { return d; }
        };

        template <typename WidgetsTuple> struct HBoxDecoration {
                static constexpr Dimension height = detail::Max<WidgetsTuple, detail::WidgetHeightField>::value;

                static void after (Context const &ctx, auto &display) {}
                static constexpr Dimension getHeightIncrement (Dimension /* d */) { return 0; }
        };

        struct NoDecoration {
                static constexpr Dimension height = 0;
                static constexpr void after (Context const &ctx, auto &display) {}
                static constexpr Dimension getHeightIncrement (Dimension /* d */) { return 0; }
        };

} // namespace detail

/****************************************************************************/

/**
 * Radio group.
 */
template <typename Callback, typename WidgetTuple> class Group {
public:
        Group (Callback const &c, WidgetTuple wt) : widgets_{std::move (wt)}, callback{c} {}

        static constexpr Focus focusableWidgetCount = detail::Sum<WidgetTuple, detail::FocusableWidgetCountField>::value;

        using Children = WidgetTuple;

        Children &widgets () { return widgets_; }
        Children const &widgets () const { return widgets_; }

private:
        WidgetTuple widgets_;
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

template <typename Callback, typename... W> auto group (Callback &&c, W &&...widgets)
{
        return Group{std::forward<Callback> (c), std::tuple{std::forward<W> (widgets)...}};
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
template <Coordinate ox, Coordinate oy, Dimension widthV, Dimension heightV, bool frame, typename ChildT> class Window {
public:
        using Child = std::remove_reference_t<std::unwrap_ref_decay_t<ChildT>>;
        explicit Window (ChildT c) : child_{std::move (c)} {} // ChildT can be a widget (like Label) or reference_wrapper <Label>

        template <typename Wrapper> Visibility operator() (auto &d, Context & /* ctx */) const
        {
                d.cursor () = {ox, oy};

                if constexpr (frame) {
                        d.print ("┌"); // TODO print does not move cursor, but line does. Inconsistent.
                        d.cursor () += {1, 0};
                        detail::line (d, width - F::cut);
                        d.print ("┐");

                        for (int i = 0; i < height - F::cut; ++i) {
                                d.cursor ().x () = ox;
                                d.cursor () += {0, 1};
                                d.print ("│");
                                d.cursor () += {1, 0};
                                detail::line (d, width - F::cut, " ");
                                d.print ("│");
                        }

                        d.cursor ().x () = ox;
                        d.cursor () += {0, 1};
                        d.print ("└");
                        d.cursor () += {1, 0};
                        detail::line (d, width - F::cut);
                        d.print ("┘");

                        d.cursor () = {ox + F::offset, oy + F::offset};
                }

                return Visibility::visible;
        }

        ChildT &child () { return child_; }
        ChildT const &child () const { return child_; }

        static constexpr Coordinate x = ox;
        static constexpr Coordinate y = oy;
        static constexpr Dimension width = widthV;   // Dimensions in characters
        static constexpr Dimension height = heightV; // Dimensions in characters
        static constexpr Focus focusableWidgetCount = Child::focusableWidgetCount;

        using F = detail::FrameHelper<frame>;

private:
        ChildT child_;
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
                return Window<ox, oy, widthV, heightV, frame, std::decay_t<decltype (c)>> (std::forward<decltype (c)> (c));
        }
} // namespace detail

/****************************************************************************/

// Forward declaration for is_layout
template <template <typename Wtu> typename Decor, typename WidgetsTuple> struct Layout;

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

        namespace augment {

                /**
                 * Additional information for all the widget contained in the Layout.
                 */
                template <typename T, Focus focusIndexV, Selection radioIndexV, Coordinate yV>
                requires c::widget<std::remove_reference_t<T>> // T can be for example Label or Label &. Only these 2 options.
                class Widget {
                public:
                        using Wrapped = std::remove_reference_t<T>;

                        constexpr Widget (T const &t) : widget{t} {}

                        /// Height in characters.
                        static constexpr Dimension getHeight () { return Wrapped::height; }

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

                        void input (auto &d, Context const &ctx, Key c)
                        {
                                if (ctx.currentFocus == getFocusIndex ()) {
                                        if constexpr (requires { widget.template input<Widget> (d, ctx, c); }) {
                                                widget.template input<Widget> (d, ctx, c);
                                        }
                                }
                        }

                        void scrollToFocus (Context *ctx) const;

                private:
                        template <typename X> friend void log (X const &, int);
                        T widget; // Wrapped widget. 2 options X or X&
                };

                template <typename T> struct is_widget_wrapper : public std::bool_constant<false> {
                };

                template <typename T, Focus focusIndexV, Selection radioIndexV, Coordinate yV>
                class is_widget_wrapper<Widget<T, focusIndexV, radioIndexV, yV>> : public std::bool_constant<true> {
                };

                template <typename T>
                concept widget_wrapper = is_widget_wrapper<T>::value;

                template <typename T, Focus focusIndexV, Selection radioIndexV, Coordinate yV>
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
                protected:
                        Visibility operator() (auto &d, Context *ctx) const
                        {
                                // Groups contain radioSelection
                                if constexpr (requires { ConcreteClass::radioSelection; }) {
                                        ctx->radioSelection = &static_cast<ConcreteClass const *> (this)->radioSelection;
                                }

                                // ctx = changeContext (ctx);

                                // Some container widgets have their own display method (operator ()). For instance og::Window
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
                                                        Decor::after (*ctx, d);
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

                        void input (auto &d, Context &ctx, Key c)
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
                };

                /**
                 * Layout
                 */
                template <typename T, typename WidgetTuple, Coordinate yV>
                requires c::layout<std::remove_reference_t<T>>
                class Layout : public ContainerWidget<Layout<T, WidgetTuple, yV>, typename std::remove_reference_t<T>::DecoratorType> {
                public:
                        using Wrapped = std::remove_reference_t<T>;
                        constexpr Layout (T const &t, WidgetTuple c) : widget{t}, children{std::move (c)} {}

                        static constexpr Dimension getHeight () { return Wrapped::template Decorator<WidgetTuple>::height; }
                        static constexpr Coordinate getY () { return yV; }

                        Visibility operator() (auto &d, Context *ctx) const
                        {
                                if (!detail::heightsOverlap (getY (), getHeight (), ctx->currentScroll, ctx->dimensions.height)) {
                                        return Visibility::outside;
                                }

                                return BaseClass::operator() (d, ctx);
                        }

                private:
                        using BaseClass = ContainerWidget<Layout<T, WidgetTuple, yV>, typename std::remove_reference_t<T>::DecoratorType>;
                        template <typename X> friend void log (X const &, int);
                        template <typename CC, typename D> friend class ContainerWidget;

                        T widget;
                        WidgetTuple children;
                };

                template <typename T> struct is_layout_wrapper : public std::bool_constant<false> {
                };

                template <typename T, typename WidgetTuple, Coordinate yV>
                class is_layout_wrapper<Layout<T, WidgetTuple, yV>> : public std::bool_constant<true> {
                };

                template <typename T>
                concept layout_wrapper = is_layout_wrapper<T>::value;

                /**
                 * Window
                 */
                template <typename T, typename Child, Coordinate yV, Dimension heightV>
                class Window : public ContainerWidget<Window<T, Child, yV, heightV>, NoDecoration> {
                public:
                        using Wrapped = std::remove_reference_t<T>;
                        constexpr explicit Window (T const &t, Child c) : widget{t}, children{std::move (c)} {}

                        static constexpr Dimension getHeight () { return heightV; }
                        static constexpr Coordinate getY () { return yV; }

                        Visibility operator() (auto &d) const { return BaseClass::operator() (d, &context); }

                        void input (auto &d, Key c) { BaseClass::input (d, context, c); }

                        void incrementFocus (auto & /* display */) const
                        {
                                if (context.currentFocus < Wrapped::focusableWidgetCount - 1) {
                                        ++context.currentFocus;
                                        scrollToFocus (&context);
                                }
                        }

                        void decrementFocus (auto & /* display */) const
                        {
                                if (context.currentFocus > 0) {
                                        --context.currentFocus;
                                        scrollToFocus (&context);
                                }
                        }

                private:
                        template <typename X> friend void log (X const &, int);
                        template <typename CC, typename D> friend class ContainerWidget;

                        T widget;
                        std::tuple<Child> children;
                        friend ContainerWidget<Window, NoDecoration>;
                        using F = typename Wrapped::F;
                        mutable Context context{
                                nullptr, {Wrapped::x + F::offset, Wrapped::y + F::offset}, {Wrapped::width - F::cut, Wrapped::height - F::cut}};

                        using BaseClass = ContainerWidget<Window<T, Child, yV, heightV>, NoDecoration>;
                        using BaseClass::scrollToFocus, BaseClass::input, BaseClass::operator();
                };

                template <typename T> struct is_window_wrapper : public std::bool_constant<false> {
                };

                template <typename T, typename Child, Coordinate yV, Dimension heightV>
                class is_window_wrapper<Window<T, Child, yV, heightV>> : public std::bool_constant<true> {
                };

                template <typename T>
                concept window_wrapper = is_window_wrapper<T>::value;

                /**
                 * Group
                 */
                template <typename T, typename WidgetTuple, Coordinate yV, Dimension heightV, typename Decor>
                class Group : public ContainerWidget<Group<T, WidgetTuple, yV, heightV, Decor>, Decor> {
                public:
                        using Wrapped = std::remove_reference_t<T>;
                        constexpr Group (T const &t, WidgetTuple c) : widget{t}, children{std::move (c)} {}

                        static constexpr Dimension getHeight () { return heightV; }
                        static constexpr Coordinate getY () { return yV; }

                        Visibility operator() (auto &d, Context *ctx) const
                        {
                                if (!detail::heightsOverlap (getY (), getHeight (), ctx->currentScroll, ctx->dimensions.height)) {
                                        return Visibility::outside;
                                }

                                return BaseClass::operator() (d, ctx);
                        }

                private:
                        using BaseClass = ContainerWidget<Group<T, WidgetTuple, yV, heightV, Decor>, Decor>;
                        template <typename X> friend void log (X const &, int);
                        template <typename CC, typename D> friend class ContainerWidget;

                        T widget;
                        WidgetTuple children;
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

                template <typename W> static auto wrap (W &&t)
                {
                        return augment::Widget<std::unwrap_ref_decay_t<W>, f, r, y>{std::forward<W> (t)};
                }
        };

        // Wrapper for layouts
        template <c::layout T, typename Parent, Focus f, Selection r, Coordinate y>
        requires c::layout<Parent> || c::window<Parent> || std::same_as<Parent, void>
        struct Wrap<T, Parent, f, r, y> {

                template <typename W> static auto wrap (W &&t)
                {
                        return augment::Layout<std::unwrap_ref_decay_t<W>, decltype (transform<f, y, Parent, T> (static_cast<T> (t).widgets ())),
                                               y>{std::forward<W> (t), transform<f, y, Parent, T> (static_cast<T> (t).widgets ())};
                }
        };

        // Wrapper for groups
        template <c::group T, c::layout Parent, Focus f, Selection r, Coordinate y> struct Wrap<T, Parent, f, r, y> {

                template <typename W> static auto wrap (W &&t)
                {
                        return augment::Group<std::unwrap_ref_decay_t<W>, decltype (transform<f, y, Parent, T> (static_cast<T> (t).widgets ())),
                                              y, Parent::template Decorator<typename T::Children>::height, typename Parent::DecoratorType>{
                                std::forward<W> (t), transform<f, y, Parent, T> (static_cast<T> (t).widgets ())};
                }
        };

        template <typename Parent = void, Focus f = 0, Selection r = 0, Coordinate y = 0> auto wrap (auto &&t);

        // Partial specialization for Windows
        template <c::window T, typename Parent, Focus f, Selection r, Coordinate y>
        requires std::same_as<Parent, void> // Means that windows are always top level
        struct Wrap<T, Parent, f, r, y> {

                static constexpr auto oy = T::y;
                static constexpr auto heightV = T::height;

                template <typename W> static auto wrap (W &&t)
                {
                        return augment::Window<std::unwrap_ref_decay_t<W>, decltype (og::detail::wrap<T, f, r, y> (t.child ())), oy, heightV> (
                                std::forward<W> (t), og::detail::wrap<T, f, r, y> (t.child ()));
                }
        };

        template <typename Parent, Focus f, Selection r, Coordinate y> auto wrap (auto &&t)
        {
                // RawType is for selecting the Wrapper partial specialization.
                using RawType = std::remove_reference_t<std::unwrap_ref_decay_t<decltype (t)>>;
                return Wrap<RawType, Parent, f, r, y>::wrap (std::forward<decltype (t)> (t));
        }

        template <Focus f, Selection r, Coordinate y, typename GrandParent, typename Parent, typename Tuple, typename T, typename... Ts>
        constexpr auto transformImpl (Tuple &&prev, T &t, Ts &...ts)
        {
                auto a = std::make_tuple (og::detail::wrap<Parent, f, r, y> (t));
                using Wrapped = std::tuple_element_t<0, decltype (a)>;

                if constexpr (sizeof...(Ts) > 0) { // Parent(Layout)::Decor::filter -> hbox return 0 : vbox return height
                        constexpr auto childHeight = Wrapped::getHeight ();
                        constexpr auto childHeightIncrement = augment::getHeightIncrement<GrandParent, Parent> (childHeight);

                        using WidgetType = std::remove_reference_t<std::unwrap_ref_decay_t<T>>;
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

namespace detail::augment {
        template <typename T> void log (T const &t, int indent = 0)
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
} // namespace detail::augment

template <typename T> void log (T &&t) { detail::augment::log (std::make_tuple (std::forward<T> (t))); }

/**
 * Container for other widgets.
 */
template <template <typename Wtu> typename Decor, typename WidgetsTuple> class Layout {
public:
        static constexpr Focus focusableWidgetCount = detail::Sum<WidgetsTuple, detail::FocusableWidgetCountField>::value;
        template <typename Wtu> using Decorator = Decor<Wtu>;
        using DecoratorType = Decor<WidgetsTuple>;

        explicit Layout (WidgetsTuple w) : widgets_{std::move (w)} {}

        WidgetsTuple &widgets () { return widgets_; }
        WidgetsTuple const &widgets () const { return widgets_; }

private:
        WidgetsTuple widgets_;
};

/*--------------------------------------------------------------------------*/

template <typename... W> auto vbox (W &&...widgets)
{
        using WidgetsTuple = decltype (std::tuple{std::forward<W> (widgets)...});
        auto vbox = Layout<detail::VBoxDecoration, WidgetsTuple>{WidgetsTuple{std::forward<W> (widgets)...}};
        return vbox;
}

template <typename... W> auto hbox (W &&...widgets)
{
        using WidgetsTuple = decltype (std::tuple{std::forward<W> (widgets)...});
        auto hbox = Layout<detail::HBoxDecoration, WidgetsTuple>{WidgetsTuple{std::forward<W> (widgets)...}};
        return hbox;
}

/****************************************************************************/

/// window factory function has special meaning that is also wraps.
template <Coordinate ox, Coordinate oy, Dimension widthV, Dimension heightV, bool frame = false, typename W = void> auto window (W &&c)
{
        return detail::wrap (detail::windowRaw<ox, oy, widthV, heightV, frame> (std::forward<W> (c)));
}

void draw (auto &display, detail::augment::window_wrapper auto const &...window)
{
        display.clear ();
        (window (display), ...);
        display.refresh ();
}

void input (auto &display, detail::augment::window_wrapper auto &window, Key key)
{
        switch (key) {
        case Key::incrementFocus:
                window.incrementFocus (display);
                break;

        case Key::decrementFocus:
                window.decrementFocus (display);
                break;

        default:
                window.input (display, key);
                break;
        }
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
//                         // vb.input (d1, char (ch), 0, 0, dummy);
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

        // auto x = window<0, 0, 18, 7> (
        //         vbox (hbox (label ("Hello "), check (" 1 "), check (" 2 ")),                                                        //
        //               hbox (label ("World "), check (" 5 "), check (" 6 ")),                                                        //
        //               button ("Open dialog", [&showDialog] { showDialog = true; }),                                                 //
        //               line<18>,                                                                                                     //
        //               group ([] (auto const &o) {}, radio (0, " R "), radio (1, " G "), radio (1, " B "), radio (1, " A ")),        //
        //               line<18>,                                                                                                     //
        //               hbox (group ([] (auto const &o) {}, radio (0, " R "), radio (1, " G "), radio (1, " B "), radio (1, " A "))), //
        //               line<18>,                                                                                                     //
        //               Combo (Options (option (0, "red"), option (1, "green"), option (1, "blue")), [] (auto const &o) {}),          //
        //               line<18>,                                                                                                     //
        //               hbox (button ("Aaa", [] {}), hspace<1>, button ("Bbb", [] {}), hspace<1>, button ("Ccc", [] {})),             //
        //               line<18>,                                                                                                     //
        //               check (" 1 "),                                                                                                //
        //               check (" 2 "),                                                                                                //
        //               check (" 3 "),                                                                                                //
        //               check (" 4 "),                                                                                                //
        //               check (" 5 "),                                                                                                //
        //               check (" 6 "),                                                                                                //
        //               check (" 7 "),                                                                                                //
        //               check (" 8 "),                                                                                                //
        //               check (" 9 "),                                                                                                //
        //               check (" 10 "),                                                                                               //
        //               check (" 11 "),                                                                                               //
        //               check (" 12 "),                                                                                               //
        //               check (" 13 "),                                                                                               //
        //               check (" 14 "),                                                                                               //
        //               check (" 15 ")                                                                                                //
        //               ));                                                                                                           //

        // log (x);
        /*--------------------------------------------------------------------------*/

        std::string buff{"The class template basic_string_view describes an object that can refer to a constant contiguous sequence of "
                         "char-like objects with the first element of the sequence at position zero."};
        // std::string buff{"aaa"};

        auto txt = text<18, 3> (std::ref (buff));

        auto chk = check (" 1 ");

        auto grp = group ([] (auto o) {}, radio (0, " R "), radio (1, " G "), radio (1, " B "), radio (1, " A "), radio (1, " C "),
                          radio (1, " M "), radio (1, " Y "), radio (1, " K "));

        auto vv = vbox (vbox (std::ref (txt)),                //
                        hbox (std::ref (chk), check (" 2 ")), //
                        std::ref (grp)                        //
        );

        auto x = window<0, 0, 18, 7> (std::ref (vv));

        /*--------------------------------------------------------------------------*/

        auto v = vbox (label ("  PIN:"), label (" 123456"),
                       hbox (button ("[OK]", [&showDialog] { showDialog = false; }), button ("[Cl]", [] {})), check (" 15 "));

        auto dialog = window<4, 1, 10, 5, true> (std::ref (v));

        // log (dialog);
        LineOffset startLine{};

        while (true) {
                if (showDialog) {
                        draw (d1, x, dialog);
                        input (d1, dialog, getKey ()); // Blocking call.
                }
                else {
                        draw (d1, x);
                        int ch = getch ();

                        switch (ch) {
                        case 'w':
                                startLine = txt.setStartLine (--startLine);
                                break;
                        case 's':
                                startLine = txt.setStartLine (++startLine);
                                break;
                        case 'a':
                                buff = "ala ma kota";
                                break;
                        case 'c':
                                chk.checked () = !chk.checked ();
                                break;
                        default:
                                input (d1, x, getKey (ch));
                                break;
                        }
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
