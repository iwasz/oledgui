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

namespace og {
enum class Visibility {
        visible,    // Widget drawed something on the screen.
        outside,    // Widget is invisible because is outside the active region.
        nonDrawable // Widget is does not draw anything by itself.
};

struct Empty {
};

using Coordinate = uint16_t; // TODO change uint16_t's in the code to proper types.
using Dimension = uint16_t;
using Focus = uint16_t;
using Selection = uint8_t;

struct Point {
        Coordinate x{};
        Coordinate y{};
};

struct Dimensions {
        Dimension width{};
        Dimension height{};
};

struct Context;

/**
 * Runtime context for all the recursive loops.
 */
struct Context {
        Point *cursor{};
        Point origin{};          // TODO const
        Dimensions dimensions{}; // TODO const
        Focus currentFocus{};
        Coordinate currentScroll{};
        Selection *radioSelection{}; // TODO remove if nre radio proves to be feasible
};

/// These classes are for simplyfying the Widget API. Instead of n function arguments we have those 2 aggreagtes.
struct Iteration {
        uint16_t focusIndex{};
        Selection radioIndex{};
};

/**
 * A helper.
 */
template <typename... T> struct First {
        using type = std::tuple_element_t<0, std::tuple<T...>>;
};

template <typename... T> using First_t = typename First<T...>::type;

template <typename ConcreteClass, uint16_t widthV, uint16_t heightV, typename Child = Empty> class Display {
public:
        Display (Child const &c = {}) : child{c} {}

        Visibility operator() ()
        {
                static_cast<ConcreteClass *> (this)->clear (); // How cool.
                Iteration iter{};
                auto v = child (*static_cast<ConcreteClass *> (this), context, iter);
                static_cast<ConcreteClass *> (this)->refresh ();
                return v;
        }

        void move (uint16_t ox, uint16_t oy) // TODO make this a Point method
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

        void calculatePositions () { child.calculatePositions (); }

        static constexpr uint16_t width = widthV;   // Dimensions in charcters
        static constexpr uint16_t height = heightV; // Dimensions in charcters

        // protected: // TODO protected
        Context context{&cursor, {0, 0}, {width, height}};
        Child child;
        // uint16_t cursorX{}; /// Current cursor position in characters
        // uint16_t cursorY{}; /// Current cursor position in characters
        Point cursor{};
};

/**
 * Ncurses backend.
 */
template <uint16_t widthV, uint16_t heightV, typename Child = Empty>
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

        void color (uint16_t c) { wattron (win, COLOR_PAIR (c)); }

        void refresh ()
        {
                ::refresh ();
                wrefresh (win);
        }

        // private: // TODO private
        using Base::child;
        using Base::context;
        using Base::cursor;

private:
        WINDOW *win{};
};

template <uint16_t widthV, uint16_t heightV, typename Child> NcursesDisplay<widthV, heightV, Child>::NcursesDisplay (Child c) : Base (c)
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

template <uint16_t widthV, uint16_t heightV> auto ncurses (auto &&child)
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
        template <typename Wrapper> void input (auto & /* d */, Context const &ctx, char c)
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

template <typename Wrapper> Visibility Check::operator() (auto &d, Context const &ctx) const // TODO this def. cannot be in a header file!
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

        d.move (strlen (label), 0); // TODO this strlen should be constexpr expression
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
                if constexpr (requires { T::focusableWidgetCount; }) { // TODO how to check for T::focusableWidgetCount type!?
                        // focusableWidgetCount field is used in composite widgets like Layout and Group.
                        return T::focusableWidgetCount;
                }
                else if constexpr (requires { T::canFocus; }) {
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
                // while augument:: wrappers implement getHeight methods.
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

} // namespace detail

/****************************************************************************/

/**
 * Radio group. Take 3.
 */
template <typename Callback, typename WidgetTuple> class Group {
public:
        Group (Callback const &c, WidgetTuple const &wt) : widgets{wt}, callback{c} {}

        static constexpr uint16_t focusableWidgetCount = detail::Sum<WidgetTuple, detail::FocusableWidgetCountField>::value;

        static constexpr Dimension height = 0; // TODO implement
        Coordinate y{};                        // TODO remove or implement

        using Children = WidgetTuple;

        // TODO input, scrollToFocus and operator() have to be re-implemented or deleted here
        Visibility operator() (auto &d, Context &ctx, Iteration &iter) const { return {}; }

        WidgetTuple widgets;

private:
        Callback callback;
};

template <typename Callback, typename... W> auto group (Callback const &c, W const &...widgets)
{
        return Group{c, std::make_tuple (widgets...)};
}

/****************************************************************************/

// Forward declaration for is_layout
template <template <typename Wtu> typename Decor, typename WidgetsTuple> struct Layout;

namespace detail {
        template <typename Wtu> class DefaultDecor {
        };

        /// Type trait for discovering Layouts
        template <typename T, template <typename Wtu> typename Decor = DefaultDecor, typename WidgetsTuple = Empty>
        struct is_layout : public std::bool_constant<false> {
        };

        template <template <typename Wtu> typename Decor, typename WidgetsTuple>
        struct is_layout<Layout<Decor, WidgetsTuple>> : public std::bool_constant<true> {
        };

        /// Quick "unit test"
        static_assert (is_layout<Layout<detail::VBoxDecoration, decltype (std::tuple{check ("")})>>::value);
        static_assert (!is_layout<decltype (check (""))>::value);

        namespace augument {

                /**
                 * Additional information for all the widgetc contained in the Layout.
                 * TODO use concepts for ensuring that T is a widget and Parent is Layout or Group
                 */
                template <typename T, Focus focusIndexV, Selection radioIndexV = 0, Coordinate yV = 0> class Widget {
                public:
                        constexpr Widget (T &t) : widget{t} /* , focusIndex{f} */ {}
                        // constexpr bool operator== (Widget const &other) const { return other.t == t && other.focusIndex == focusIndex; }

                        /// Height in characters.
                        static constexpr Dimension getHeight () { return T::height; }

                        /// Consecutiive number in a radio group.
                        static constexpr Selection getRadioIndex () { return radioIndexV; }

                        /// Consecutive number (starting from 0) assigned for every focusable widget.
                        static constexpr Focus getFocusIndex () { return focusIndexV; }

                        /// Position starting from the top.
                        static constexpr Coordinate getY () { return yV; }

                        Visibility operator() (auto &d, Context const &ctx) const
                        {
                                if (!detail::heightsOverlap (getY (), getHeight (), ctx.currentScroll, ctx.dimensions.height)) {
                                        return Visibility::outside;
                                }

                                return widget.template operator()<Widget> (d, ctx);
                        }

                        void input (auto &d, Context const &ctx, char c)
                        {
                                if (ctx.currentFocus == getFocusIndex ()) {
                                        if constexpr (requires { widget.template input<Widget> (d, ctx, c); }) {
                                                widget.template input<Widget> (d, ctx, c);
                                        }
                                }
                        }

                        void scrollToFocus (Context &ctx) const;

                private:
                        T &widget; // Wrapped widget
                };

                template <typename T, Focus focusIndexV, Selection radioIndexV, Coordinate yV>
                void Widget<T, focusIndexV, radioIndexV, yV>::scrollToFocus (Context &ctx) const
                {
                        auto h = getHeight ();
                        if (!detail::heightsOverlap (getY (), getHeight (), ctx.currentScroll, ctx.dimensions.height)) {
                                if (ctx.currentFocus == getFocusIndex ()) {
                                        if (getY () < ctx.currentScroll) {
                                                ctx.currentScroll = getY ();
                                        }
                                        else {
                                                ctx.currentScroll = getY () - ctx.dimensions.height + 1;
                                        }
                                }
                        }
                }

                /**
                 * Base for widgets in the augument:: namespace that can contain other widgets.
                 */
                template <typename ConcreteClass, typename Decor> struct ContainerWidget {
                        Visibility operator() (auto &d, Context const &ctx) const
                        {
                                if (!detail::heightsOverlap (ConcreteClass::getY (), ConcreteClass::getHeight (), ctx.currentScroll,
                                                             ctx.dimensions.height)) {
                                        return Visibility::outside;
                                }

                                auto l = [&d, &ctx] (auto &itself, auto const &child, auto const &...children) {
                                        if (child (d, ctx) == Visibility::visible) {
                                                constexpr bool lastWidgetInLayout = (sizeof...(children) == 0);

                                                if (!lastWidgetInLayout) {
                                                        Decor::after (ctx);
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

                        Visibility operator() (auto &d) const
                        {
                                Iteration iter{};
                                return operator() (d, d.context);
                        }

                        void input (auto &d, Context const &ctx, char c)
                        {
                                auto l = [&d, &ctx, c] (auto &itself, auto &child, auto &...children) {
                                        child.input (d, ctx, c);

                                        if constexpr (sizeof...(children) > 0) {
                                                itself (itself, children...);
                                        }
                                };

                                std::apply ([&l] (auto &...children) { l (l, children...); }, static_cast<ConcreteClass *> (this)->children);
                        }

                        void input (auto &d, char c) { input (d, d.context, c); }

                        void incrementFocus (Context &ctx) const
                        {
                                if (ctx.currentFocus < ConcreteClass::Wrapped::focusableWidgetCount - 1) {
                                        ++ctx.currentFocus;
                                        scrollToFocus (ctx);
                                }
                        }

                        void decrementFocus (Context &ctx) const
                        {
                                if (ctx.currentFocus > 0) {
                                        --ctx.currentFocus;
                                        scrollToFocus (ctx);
                                }
                        }

                        void scrollToFocus (Context &ctx) const
                        {
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

                // TODO Parent has to be another Layout or void (use concept for ensuring that)
                template <typename T, typename WidgetTuple, Focus focusIndexV,
                          Coordinate yV = 0> // TODO remove default parameter initializers if possible
                class Layout : public ContainerWidget<Layout<T, WidgetTuple, focusIndexV, yV>, typename T::DecoratorType> {
                public:
                        using Wrapped = T;
                        constexpr Layout (Wrapped &t, WidgetTuple const &c) : widget{t}, children{c} {}

                        static constexpr Dimension getHeight () { return Wrapped::template Decorator<WidgetTuple>::height; }
                        static constexpr Selection getRadioIndex () { return 0; }
                        static constexpr Focus getFocusIndex () { return focusIndexV; }
                        static constexpr Coordinate getY () { return yV; }

                private:
                        friend ContainerWidget<Layout<T, WidgetTuple, focusIndexV, yV>, typename T::DecoratorType>;
                        Wrapped &widget;
                        WidgetTuple children;
                };

                // TODO Parent has to be a Layout (use concept for ensuring that)
                template <typename T, typename WidgetTuple, Focus focusIndexV, Coordinate yV, Dimension heightV, typename Decor>
                class Group : public ContainerWidget<Group<T, WidgetTuple, focusIndexV, yV, heightV, Decor>, Decor> {
                public:
                        using Wrapped = T;
                        constexpr Group (Wrapped &t, WidgetTuple const &c) : widget{t}, children{c} {}

                        static constexpr Dimension getHeight () { return heightV; }
                        static constexpr Selection getRadioIndex () { return 0; }
                        static constexpr Focus getFocusIndex () { return focusIndexV; }
                        static constexpr Coordinate getY () { return yV; }

                private:
                        friend ContainerWidget<Group<T, WidgetTuple, focusIndexV, yV, heightV, Decor>, Decor>;
                        Wrapped &widget;
                        WidgetTuple children;
                };

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

        } // namespace augument

        template <Focus f, Coordinate y, typename GrandParent, typename Parent, typename ChildrenTuple>
        constexpr auto transform (ChildrenTuple &tuple);

        // Wrapper for ordinary widgets
        template <typename T, typename GrandParent, typename Parent, Focus f, Selection r, Coordinate y,
                  template <typename Wtu> typename Decor = DefaultDecor, typename WidgetsTuple = Empty>
        struct Wrap {
                using WidgetType = T;
                static auto wrap (WidgetType &t) { return augument::Widget<WidgetType, f, r, y>{t}; }
        };

        // Wrapper for layouts
        template <typename GrandParent, typename Parent, Focus f, Selection r, Coordinate y, template <typename Wtu> typename Decor,
                  typename WidgetsTuple>
        struct Wrap<Layout<Decor, WidgetsTuple>, GrandParent, Parent, f, r, y> {
                using WidgetType = Layout<Decor, WidgetsTuple>;

                static auto wrap (WidgetType &t)
                {
                        return augument::Layout<WidgetType, decltype (transform<f, y, Parent, WidgetType> (t.widgets)), f, y>{
                                t, transform<f, y, Parent, WidgetType> (t.widgets)};
                }
        };

        // Wrapper for groups
        template <typename GrandParent, typename Parent, Focus f, Selection r, Coordinate y, typename Callback, typename WidgetsTuple>
        struct Wrap<Group<Callback, WidgetsTuple>, GrandParent, Parent, f, r, y> {
                using WidgetType = Group<Callback, WidgetsTuple>;

                static auto wrap (WidgetType &t)
                {
                        return augument::Group<WidgetType, decltype (transform<f, y, Parent, WidgetType> (t.widgets)), f, y,
                                               Parent::template Decorator<typename WidgetType::Children>::height,
                                               typename Parent::DecoratorType>{t, transform<f, y, Parent, WidgetType> (t.widgets)};
                }
        };

        template <typename T, Focus f = 0, Selection r = 0, Coordinate y = 0> auto wrap (T &t)
        {
                using WidgetType = T;
                return Wrap<WidgetType, void, void, f, r, y>::wrap (t);
        }

        template <Focus f, Selection r, Coordinate y, typename GrandParent, typename Parent, typename Tuple, typename T, typename... Ts>
        constexpr auto transformImpl (Tuple &&prev, T &t, Ts &...ts)
        {
                using WidgetType = std::remove_cvref_t<T>;
                using Wrapper = Wrap<WidgetType, GrandParent, Parent, f, r, y>;
                using Wrapped = decltype (Wrapper::wrap (t));
                auto a = std::make_tuple (Wrapper::wrap (t));

                if constexpr (sizeof...(Ts) > 0) { // Parent(Layout)::Decor::filter -> hbox return 0 : vbox return height
                        constexpr auto childHeight = Wrapped::getHeight ();
                        constexpr auto childHeightIncrement = augument::getHeightIncrement<GrandParent, Parent> (childHeight);

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

template <typename T> void log (T const &t, int indent = 0)
{
        auto l = [indent] (auto &itself, auto const &w, auto const &...ws) {
                // std::string used only for debug.
                std::cout << std::string (indent, ' ') << "focusIndex: " << w.getFocusIndex () << ", radioIndex: " << int (w.getRadioIndex ())
                          << ", y: " << w.getY () << ", height: " << w.getHeight () << ", " << typeid (w.widget).name () << std::endl;

                if constexpr (requires (decltype (w) x) { x.children; }) {
                        log (w.children, indent + 2);
                }

                if constexpr (sizeof...(ws) > 0) {
                        itself (itself, ws...);
                }
        };

        std::apply ([&l] (auto const &...widgets) { l (l, widgets...); }, std::tuple{t});
}

/**
 * Container for other widgtes.
 */
template <template <typename Wtu> typename Decor, typename WidgetsTuple> struct Layout {

        static constexpr uint16_t focusableWidgetCount = detail::Sum<WidgetsTuple, detail::FocusableWidgetCountField>::value;

        template <typename Wtu> using Decorator = Decor<Wtu>;
        using Children = WidgetsTuple;
        using DecoratorType = Decor<WidgetsTuple>;

        explicit Layout (WidgetsTuple w) : widgets{std::move (w)} /* , augumentedWidgets{detail::transform (widgets)} */ {}

        // private:
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

/**
 *
 */
template <uint16_t ox, uint16_t oy, uint16_t widthV, uint16_t heightV, typename Child> struct Window {

        explicit Window (Child const &c) : child{c} {}

        Visibility operator() (auto &d) const
        {
                Iteration iter{};
                return operator() (d, d.context, iter);
        }

        // Visibility operator() (auto &d, Context &ctx, Iteration const &iter = {}) const
        // {
        //         d.cursorX = ox;
        //         d.cursorY = oy;
        //         return child (d, context, iter);
        // }

        // TODO This always prints the frame. There should be option for a windows without one.
        Visibility operator() (auto &d, Context &ctx, Iteration &iter) const
        {
                // TODO move to separate function. Code duplication.
                // if (!detail::heightsOverlap (Base::y, height, ctx.currentScroll, ctx.dimensions.height)) {
                //         return Visibility::outside;
                // }

                d.setCursorX (ox);
                d.setCursorY (oy);

                d.print ("┌"); // TODO print does not move cursor, but line does. Inconsistent.
                d.move (1, 0);
                detail::line (d, width - 2);
                d.print ("┐");

                for (int i = 0; i < height - 2; ++i) {
                        d.setCursorX (ox);
                        d.move (0, 1);
                        d.print ("│");
                        // d.move (width - 1, 0);
                        d.move (1, 0);
                        detail::line (d, width - 2, " ");
                        d.print ("│");
                }

                d.setCursorX (ox);
                d.move (0, 1);
                d.print ("└");
                d.move (1, 0);
                detail::line (d, width - 2);
                d.print ("┘");

                d.setCursorX (ox + 1);
                d.setCursorY (oy + 1);

                if (context.cursor == nullptr) { // TODO I don't like this approach
                        context.cursor = ctx.cursor;
                }

                return child (d, context, iter);
        }

        void input (auto &d, char c)
        {
                Iteration iter{};
                input (d, context, iter, c);
        }

        void input (auto &d, Context & /* ctx */, Iteration &iter, char c) { child.input (d, context, iter, c); }
        void scrollToFocus (Context & /* ctx */, Iteration &iter) const { child.scrollToFocus (context, iter); }

        void incrementFocus (Context & /* ctx */) const
        {
                if (context.currentFocus < focusableWidgetCount - 1) {
                        ++context.currentFocus;
                        Iteration iter{};
                        scrollToFocus (context, iter);
                }
        }

        void decrementFocus (Context & /* ctx */) const
        {
                if (context.currentFocus > 0) {
                        --context.currentFocus;
                        Iteration iter{};
                        scrollToFocus (context, iter);
                }
        }

        void calculatePositions () { child.calculatePositions (); }

        static constexpr Dimension width = widthV;   // Dimensions in charcters
        static constexpr Dimension height = heightV; // Dimensions in charcters
        static constexpr uint16_t focusableWidgetCount = Child::focusableWidgetCount;
        mutable Context context{nullptr, {ox + 1, oy + 1}, {width - 2, height - 2}};
        Child child;
};

template <uint16_t ox, uint16_t oy, uint16_t widthV, uint16_t heightV> auto window (auto &&c)
{
        return Window<ox, oy, widthV, heightV, std::remove_reference_t<decltype (c)>> (std::forward<decltype (c)> (c));
}

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

        // auto vb = vbox (
        //         vbox (label ("Combo"), Combo (Options (option (0, "red"), option (1, "green"), option (1, "blue")), [] (auto const &o) {})),
        //         line<10>, //
        //         vbox (label ("New radio"),
        //               Radio2 (OptionsRad (radio (0, " red"), radio (1, " green"), radio (1, " blue")), [] (auto const &o) {})),
        //         line<10>, //
        //         hbox (Radio2 (OptionsRad (radio (0, " R "), radio (1, " G "), radio (1, " B ")), [] (auto const &o) {})),
        //         line<10>,                                                                                           //
        //         vbox (label ("Old radio"), radio (0, " a "), radio (0, " b "), radio (0, " c "), radio (0, " d ")), //
        //         line<10>,                                                                                           //
        //         vbox (label ("Checkbox"), check (" 1 "), check (" 2 "), check (" 3 "), check (" 4 ")),              //
        //         line<10>,                                                                                           //
        //         vbox (check (" 5 "), check (" 6 "), check (" 7 "), check (" 8 ")),                                  //
        //         line<10>,                                                                                           //
        //         vbox (hbox (check (" 1 "), check (" 2 "), check (" 3 "), check (" 4 ")),
        //               hbox (check (" 5 "), check (" 6 "), check (" 7 "), check (" 8 ")),
        //               hbox (check (" 9 "), check (" A "), check (" B "), check (" C ")),
        //               hbox (check (" D "), check (" E "), check (" F "), check (" G ")),
        //               hbox (check (" H "), check (" I "), check (" J "), check (" K ")),
        //               hbox (check (" L "), check (" 1 "), check (" M "), check (" N ")),
        //               hbox (check (" O "), check (" P "), check (" Q "), check (" R ")),
        //               hbox (check (" S "), check (" T "), check (" U "), check (" V ")),
        //               hbox (check (" W "), check (" X "), check (" Y "), check (" Z ")),
        //               hbox (check (" 1 "), check (" 2 "), check (" 3 "), check (" 4 ")),
        //               hbox (check (" 5 "), check (" 6 "), check (" 7 "), check (" 8 "))));

        // auto vb = vbox (group ([] (auto const &o) {}, radio (0, " R "), radio (1, " G "), radio (1, " B "), radio (1, " A ")),        //
        //                 hbox (group ([] (auto const &o) {}, radio (0, " R "), radio (1, " G "), radio (1, " B "), radio (1, " A "))), //
        //                 check (" 1"), check (" 2"), check (" 3"), check (" 4"));

        // auto vb = vbox (label ("Combo"), Combo (Options (option (0, "red"), option (1, "green"), option (1, "blue")), [] (auto const &o) {}),
        //                 hbox (Radio2 (OptionsRad (radio (0, " R "), radio (1, " G "), radio (1, " B ")), [] (auto const &o) {})), check (" 5
        //                 "), check (" 6 "), check (" 7 "), check (" 8 "), check (" 5 "), check (" 6 "), check (" 7 "), check (" 8 "));

        auto vb = vbox (hbox (label ("Hej "), check (" 1 "), check (" 2 ")),                                                   //
                        hbox (label ("ho  "), check (" 5 "), check (" 6 ")),                                                   //
                        line<18>,                                                                                              //
                        group ([] (auto const &o) {}, radio (0, " R "), radio (1, " G "), radio (1, " B "), radio (1, " A ")), //
                        line<18>,                                                                                              //
                        radio (0, " R "), line<18>,                                                                            //
                        Combo (Options (option (0, "red"), option (1, "green"), option (1, "blue")), [] (auto const &o) {}),   //
                        line<18>,                                                                                              //
                        hbox (button ("Aaa", [] {}), hspace<1>, button ("Bbb", [] {}), hspace<1>, button ("Ccc", [] {})),      //
                        line<18>,                                                                                              //
                        check (" 1 "), check (" 2 "), check (" 3 "), check (" 4 "), check (" 5 "), check (" 6 "), check (" 7 "), check (" 8 "),
                        check (" 9 "), check (" 10 "), check (" 11 "), check (" 12 "), check (" 13 "), check (" 14 "), check (" 15 "));

        auto x = detail::wrap (vb);
        // log (x);

        bool showDialog{};

        // auto dialog = window<4, 1, 10, 5> (vbox (label ("  Token"), label (" 123456"), button ("  [OK]", [&showDialog] { showDialog = false;
        // }),
        //                                          check (" dialg5"), check (" 6 "), check (" 7 "), check (" 8 ")));

        // TODO simplify this mess to a few lines. Minimal verbosity.
        while (true) {
                d1.clear ();
                // vb (d1);
                x (d1);

                // if (showDialog) {
                //         Iteration iter{}; // TODO mess
                //         dialog (d1, d1.context, iter);
                // }

                d1.refresh ();
                int ch = getch ();

                if (ch == 'q') {
                        break;
                }

                switch (ch) {
                case KEY_DOWN:
                        if (showDialog) {
                                // dialog.incrementFocus (d1.context);
                        }
                        else {
                                x.incrementFocus (d1.context);
                        }
                        break;

                case KEY_UP:
                        if (showDialog) {
                                // dialog.decrementFocus (d1.context);
                        }
                        else {
                                x.decrementFocus (d1.context);
                        }
                        break;

                case 'd':
                        showDialog = true;
                        break;

                default:
                        // d1.input (vb, char (ch));
                        if (showDialog) {
                                // dialog.input (d1, char (ch));
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
