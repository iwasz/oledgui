/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "ncurses.h"
#include "oledgui.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
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
        return;
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

/*--------------------------------------------------------------------------*/

/**
 * Base class.
 * widgetCountV : number of focusable widgets. Some examples : for a label whihc is
 *                not focusable, use 0. For a button use 1. For containers return the
 *                number of focusable children insdide.
 * heightV : height of a widget in characters. For a mere (single line) label it would
 *           equal to 1, for a container it would be sum of all children heights.
 */
template <typename ConcreteClass, uint16_t widgetCountV = 0, uint16_t heightV = 0> struct Widget {

        void scrollToFocus (Context &ctx, Iteration const &iter) const;
        void input (auto &d, Context const &ctx, Iteration const &iter, char c) {}

        void incrementFocus (Context &ctx) const
        {
                if (ctx.currentFocus < ConcreteClass::widgetCount - 1) {
                        ++ctx.currentFocus;
                        Iteration iter{}; // TODO I don't like this
                        static_cast<ConcreteClass const *> (this)->scrollToFocus (ctx, iter);
                }
        }

        void decrementFocus (Context &ctx) const
        {
                if (ctx.currentFocus > 0) {
                        --ctx.currentFocus;
                        Iteration iter{};
                        static_cast<ConcreteClass const *> (this)->scrollToFocus (ctx, iter);
                }
        }

        static constexpr Dimension getHeight () { return heightV; }

        uint16_t y{};
        static constexpr uint16_t widgetCount = widgetCountV;
        static constexpr bool canFocus = widgetCount > 0;
        // static constexpr uint16_t height = heightV;
};

template <typename ConcreteClass, uint16_t widgetCountV, uint16_t heightV>
void Widget<ConcreteClass, widgetCountV, heightV>::scrollToFocus (Context &ctx, Iteration const &iter) const
{
        auto h = ConcreteClass::getHeight ();
        if (!detail::heightsOverlap (y, h, ctx.currentScroll, ctx.dimensions.height)) {
                if (ctx.currentFocus == iter.focusIndex) {
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

template <uint16_t len> struct Line : public Widget<Line<len>, 0, 1> {

        using Base = Widget<Line<len>, 0, 1>;
        using Base::y, Base::getHeight;

        Visibility operator() (auto &d, Context const &ctx, Iteration const & /* iter */) const
        {
                if (!detail::heightsOverlap (y, getHeight (), ctx.currentScroll, ctx.dimensions.height)) {
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

class Check : public Widget<Check, 1, 1> {
public:
        constexpr Check (const char *s, bool c) : label{s}, checked{c} {}

        // TODO move outside the class.
        // TODO remove radioIndex and groupSelection. Not needed here.
        Visibility operator() (auto &d, Context const &ctx, Iteration const &iter) const
        {
                if (!detail::heightsOverlap (y, getHeight (), ctx.currentScroll, ctx.dimensions.height)) {
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

        void input (auto & /* d */, Context const &ctx, Iteration const &iter, char c)
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

template <std::integral Id> // TODO less restrictive concept for Id
class Radio : public Widget<Radio<Id>, 1, 1> {
public:
        using Base = Widget<Radio<Id>, 1, 1>;
        using Base::y, Base::getHeight;

        constexpr Radio (Id const &i, const char *l) : id{i}, label{l} {}
        Visibility operator() (auto &d, Context const &ctx, Iteration const &iter) const;
        void input (auto &d, Context const &ctx, Iteration const &iter, char c);

private:
        Id id;
        const char *label;
};

template <std::integral Id> Visibility Radio<Id>::operator() (auto &d, Context const &ctx, Iteration const &iter) const
{
        if (!detail::heightsOverlap (y, getHeight (), ctx.currentScroll, ctx.dimensions.height)) {
                return Visibility::outside;
        }

        if (ctx.currentFocus == iter.focusIndex) {
                d.color (2);
        }

        if (ctx.radioSelection != nullptr && iter.radioIndex == *ctx.radioSelection) {
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

template <std::integral Id> void Radio<Id>::input (auto & /* d */, Context const &ctx, Iteration const &iter, char c)
{
        // TODO character must be customizable (compile time)
        if (ctx.radioSelection != nullptr && ctx.currentFocus == iter.focusIndex && c == ' ') {
                *ctx.radioSelection = iter.radioIndex;
        }
}

template <std::integral Id> constexpr auto radio (Id &&id, const char *label) { return Radio<Id> (std::forward<Id> (id), label); }

/**
 * A container for radios.
 */
template <std::integral I, size_t Num> struct OptionsRad {

        using OptionType = Radio<I>;
        using Id = I;
        using ContainerType = std::array<Radio<I>, Num>;
        using SelectionIndex = typename ContainerType::size_type; // std::array::at accepts this

        template <typename... J> constexpr OptionsRad (Radio<J> &&...e) : elms{std::forward<Radio<J>> (e)...} {}

        ContainerType elms;
};

template <typename... J> OptionsRad (Radio<J> &&...e) -> OptionsRad<First_t<J...>, sizeof...(J)>;

/****************************************************************************/
/* Label                                                                    */
/****************************************************************************/

/**
 * Single line (?)
 */
class Label : public Widget<Label, 0, 1> {
public:
        constexpr Label (const char *l) : label{l} {}
        Visibility operator() (auto &d, Context const &ctx, Iteration const & /* iter */) const
        {
                if (!detail::heightsOverlap (y, getHeight (), ctx.currentScroll, ctx.dimensions.height)) { // TODO Move from here, duplication.
                        return Visibility::outside;
                }

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
template <typename Callback> class Button : public Widget<Button<Callback>, 1, 1> {
public:
        using Base = Widget<Button<Callback>, 1, 1>;
        using Base::y, Base::getHeight;

        constexpr Button (const char *l, Callback const &c) : label{l}, callback{c} {}

        Visibility operator() (auto &d, Context const &ctx, Iteration const &iter) const;

        void input (auto & /* d */, Context const &ctx, Iteration const &iter, char c)
        {
                if (ctx.currentFocus == iter.focusIndex && c == ' ') {
                        callback ();
                }
        }

private:
        const char *label;
        Callback callback;
};

template <typename Callback> Visibility Button<Callback>::operator() (auto &d, Context const &ctx, Iteration const &iter) const
{
        if (!detail::heightsOverlap (y, getHeight (), ctx.currentScroll, ctx.dimensions.height)) { // TODO Move from here, duplication.
                return Visibility::outside;
        }

        if (ctx.currentFocus == iter.focusIndex) {
                d.color (2);
        }

        d.print (label);
        d.move (strlen (label), 0);

        if (ctx.currentFocus == iter.focusIndex) {
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
class Combo : public Widget<Combo<OptionCollection, Callback>, 1, 1> {
public:
        using Option = typename OptionCollection::OptionType;
        using Id = typename OptionCollection::Id;
        using Base = Widget<Combo<OptionCollection, Callback>, 1, 1>;
        using Base::y, Base::height;
        using SelectionIndex = typename OptionCollection::SelectionIndex;

        constexpr Combo (OptionCollection const &o, Callback c) : options{o}, callback{c} {}

        Visibility operator() (auto &d, Context const &ctx, Iteration const &iter) const;
        void input (auto &d, Context const &ctx, Iteration const &iter, char c);

private:
        OptionCollection options;
        SelectionIndex currentSelection{};
        Callback callback;
};

/*--------------------------------------------------------------------------*/

template <typename OptionCollection, typename Callback>
Visibility Combo<OptionCollection, Callback>::operator() (auto &d, Context const &ctx, Iteration const &iter) const
{
        if (!detail::heightsOverlap (y, height, ctx.currentScroll, ctx.dimensions.height)) {
                return Visibility::outside;
        }

        if (ctx.currentFocus == iter.focusIndex) {
                d.color (2);
        }

        const char *label = options.elms.at (currentSelection).label;
        d.print (label);

        if (ctx.currentFocus == iter.focusIndex) {
                d.color (1);
        }

        d.move (strlen (label), 0);
        return Visibility::visible;
}

/*--------------------------------------------------------------------------*/

template <typename OptionCollection, typename Callback>
void Combo<OptionCollection, Callback>::input (auto & /* d */, Context const &ctx, Iteration const &iter, char c)
{
        if (ctx.currentFocus == iter.focusIndex && c == ' ') { // TODO character must be customizable (compile time)
                ++currentSelection;
                currentSelection %= options.elms.size ();
        }
}

/****************************************************************************/
/* Radio group                                                              */
/****************************************************************************/

/**
 * Copy & paste from Combo
 */
template <typename OptionCollection, typename Callback>
requires std::invocable<Callback, typename OptionCollection::Id>
class Radio2 /* : public Widget<Radio2<OptionCollection, Callback>> */ {
public:
        using Option = typename OptionCollection::OptionType;
        using Id = typename OptionCollection::Id;
        using Base = Widget<Radio2<OptionCollection, Callback>>;
        // using Base::y;
        // using SelectionIndex = typename OptionCollection::SelectionIndex;

        static constexpr uint16_t widgetCount = std::tuple_size<typename OptionCollection::ContainerType>::value;
        static_assert (widgetCount <= 255);
        // static constexpr uint16_t height = widgetCount; // TODO assumes that all radiobuttons are 1 in height (which is correct)

        constexpr Radio2 (OptionCollection const &o, Callback c) : options{o}, callback{c} {}

        // Visibility operator() (auto &d, Context const &ctx, Iteration const &iter) const;
        // void input (auto &d, Context const &ctx, Iteration iter, char c);

        auto const &getElements () const { return options.elms; }
        auto &getElements () { return options.elms; }

        Coordinate y{};

        mutable Selection currentSelection{}; // TODO private
private:
        OptionCollection options;
        Callback callback;
};

/****************************************************************************/
/* Layouts                                                                  */
/****************************************************************************/

namespace detail {

        template <typename T> struct WidgetCountField {
                static constexpr auto value = T::widgetCount;
        };

        template <typename T> struct WidgetHeightField {

                static constexpr auto value = T::getHeight ();
        };

        /*--------------------------------------------------------------------------*/

        template <typename E, template <typename T> typename Field> constexpr auto getValue ()
        {
                if constexpr (requires (E t) { t.getElements (); }) {
                        return Field<typename E::Option>::value * E::widgetCount; // TODO remove if Radio2 not needed
                }
                else {
                        return Field<E>::value;
                }
        }

        template <typename E, template <typename T> typename Field> constexpr auto getValueMax ()
        {
                if constexpr (requires (E t) { t.getElements (); }) {
                        return Field<typename E::Option>::value; // All radios in a group have the same type which which imples same height
                }
                else {
                        return Field<E>::value;
                }
        }

        template <typename Tuple, template <typename T> typename Field, size_t n = std::tuple_size_v<Tuple> - 1> struct Sum {
                static constexpr auto value = getValue<std::tuple_element_t<n, Tuple>, Field> () + Sum<Tuple, Field, n - 1>::value;
        };

        template <typename Tuple, template <typename T> typename Field> struct Sum<Tuple, Field, 0> {
                static constexpr auto value = getValue<std::tuple_element_t<0, Tuple>, Field> ();
        };

        template <typename Tuple, template <typename T> typename Field, size_t n = std::tuple_size_v<Tuple> - 1> struct Max {
                static constexpr auto value = std::max (getValueMax<std::tuple_element_t<n, Tuple>, Field> (), Max<Tuple, Field, n - 1>::value);
        };

        template <typename Tuple, template <typename T> typename Field> struct Max<Tuple, Field, 0> {
                static constexpr auto value = getValueMax<std::tuple_element_t<0, Tuple>, Field> ();
        };

        /*--------------------------------------------------------------------------*/
        /* Decorators for vertical and horizontal layouts.                          */
        /*--------------------------------------------------------------------------*/

        // Common implementation for both
        // TODO vertical can be a template parameter - but what is the CPU and size impact? Make a decision after tests.
        template <typename WidgetType> static void calculatePosition (WidgetType &widget, Coordinate &bottomY, bool vertical)
        {
                if constexpr (requires (WidgetType w) { w.getElements (); }) {
                        for (auto &o : widget.getElements ()) {
                                o.y = bottomY;

                                if (vertical) {
                                        bottomY += o.height;
                                }

                                // std::cerr << "o.y = " << o.y << ", o.height = " << o.height << std::endl;
                        }
                }
                else {
                        widget.y = bottomY;

                        if (vertical) {
                                bottomY += widget.getHeight ();
                        }

                        // std::cerr << "w.y = " << widget.y << ", w.height = " << widget.height << ", typeid = " << typeid (widget).name ()
                        //           << std::endl;

                        if constexpr (requires (decltype (widget) w) { widget.calculatePositions (); }) {
                                widget.calculatePositions (); // widget.y is already set and Layout::calculatePositions() uses it.
                        }
                }
        }

        template <typename WidgetsTuple> struct VBoxDecoration {
                static constexpr Dimension height = detail::Sum<WidgetsTuple, detail::WidgetHeightField>::value;

                static void after (Context const &ctx)
                {
                        ctx.cursor->y += 1;
                        ctx.cursor->x = ctx.origin.x;
                }

                template <typename WidgetType> static void calculatePosition (WidgetType &widget, Coordinate &bottomY)
                {
                        detail::calculatePosition (widget, bottomY, true);
                }
        };

        template <typename WidgetsTuple> struct HBoxDecoration {
                static constexpr Dimension height = detail::Max<WidgetsTuple, detail::WidgetHeightField>::value;

                static void after (Context const &ctx) {}

                template <typename WidgetType> static void calculatePosition (WidgetType &widget, Coordinate &bottomY)
                {
                        detail::calculatePosition (widget, bottomY, false);
                }
        };

        /**
         * Iterate over Layout's widgets.
         */
        template <typename Callback, typename W, typename... Ws>
        void iterate (Callback const &callback, Context &ctx, Iteration &iter, W &widget, Ws &...widgets)
        {
                using WidgetType = W;

                constexpr bool lastWidgetInLayout = (sizeof...(widgets) == 0);

                // This tests if a widget has getElements method. Its like a reflection.
                // Such a widget is called a "composite widget".
                if constexpr (requires (WidgetType w) { w.getElements (); }) { // TODO can it be extracted to a concept/function?

                        static_assert (std::is_reference_v<decltype (widget.getElements ())>,
                                       "Ensure that your composite widget type has getElements method that returns a reference.");

                        iter.radioIndex = 0;
                        ctx.radioSelection = &widget.currentSelection;

                        for (auto const &last = widget.getElements ().back (); auto &o : widget.getElements ()) {
                                std::cerr << "O. focusIndex: " << iter.focusIndex << ", radioIndex: " << int (iter.radioIndex) << std::endl;
                                callback (o, iter, lastWidgetInLayout && &o == &last);
                                ++iter.focusIndex;
                                ++iter.radioIndex;
                        }

                        iter.focusIndex -= widget.widgetCount; // TODO absolute mess
                }
                // This branch is for widgets that don't have getElements method
                else {
                        std::cerr << "W. focusIndex: " << iter.focusIndex << ", radioIndex: " << int (iter.radioIndex)
                                  << ", name: " << typeid (widget).name () << ", wc: " << widget.widgetCount << std::endl;

                        callback (widget, iter, lastWidgetInLayout);
                }
                // iter.focusIndex += widget.widgetCount;

                if constexpr (!lastWidgetInLayout) {
                        // TODO I don'tlike the fact that the whole Iter POD object is created again ana again even if one of its members is not
                        // used.
                        Iteration iii{uint16_t (iter.focusIndex + widget.widgetCount), 0 /* TODO not used */};
                        iterate (callback, ctx, iii, widgets...);
                        // iterate (callback, iter, widgets...);
                }
        }

} // namespace detail

/****************************************************************************/

/**
 * Radio group. Take 3.
 */
template <typename Callback, typename WidgetTuple> class Group : public Widget<Group<Callback, WidgetTuple>> {
public:
        Group (Callback const &c, WidgetTuple const &wt) : widgets{wt}, callback{c} {}

        static constexpr uint16_t widgetCount = detail::Sum<WidgetTuple, detail::WidgetCountField>::value;
        static constexpr bool canFocus = false;

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

        // /// Type trait for discovering Layouts
        // template <typename T, template <typename Wtu> typename Decor = DefaultDecor, typename WidgetsTuple = Empty>
        // struct is_layout : public std::bool_constant<false> {
        // };

        // template <template <typename Wtu> typename Decor, typename WidgetsTuple>
        // struct is_layout<Layout<Decor, WidgetsTuple>> : public std::bool_constant<true> {
        // };

        // /// Quick "unit test"
        // static_assert (is_layout<Layout<detail::VBoxDecoration, decltype (std::tuple{check ("")})>>::value);
        // static_assert (!is_layout<decltype (check (""))>::value);

        namespace augument {
                /**
                 * Additional information for all the widgetc contained in the Layout.
                 */
                template <typename T, Focus focusIndexV, Selection radioIndexV = 0, Coordinate yV = 0> struct Widget {

                        constexpr Widget (T &t) : widget{t} /* , focusIndex{f} */ {}
                        // constexpr bool operator== (Widget const &other) const { return other.t == t && other.focusIndex == focusIndex; }

                        /// Height in characters.
                        static constexpr Dimension getHeight () { return T::getHeight (); }

                        /// Consecutiive number in a radio group.
                        static constexpr Selection getRadioIndex () { return radioIndexV; }

                        /// Consecutive number (starting from 0) assigned for every focusable widget.
                        static constexpr Focus getFocusIndex () { return focusIndexV; }

                        /// Position starting from the top.
                        static constexpr Coordinate getY () { return yV; }

                        // Wrapped widget
                        T &widget;
                };

                template <typename T, typename WidgetTuple, template <typename Wgts> typename Decor, Focus focusIndexV, Coordinate yV = 0>
                struct Layout {

                        constexpr Layout (T &t, WidgetTuple const &c) : widget{t}, children{c} {}

                        static constexpr Dimension getHeight () { return Decor<WidgetTuple>::height; }
                        static constexpr Selection getRadioIndex () { return 0; }
                        static constexpr Focus getFocusIndex () { return focusIndexV; }
                        static constexpr Coordinate getY () { return yV; }

                        T &widget;
                        WidgetTuple children;
                };

                template <typename T, typename WidgetTuple, Focus focusIndexV, Coordinate yV = 0, Dimension heightV = 0> struct Group {

                        constexpr Group (T &t, WidgetTuple const &c) : widget{t}, children{c} {}

                        static constexpr Dimension getHeight () { return heightV; }
                        static constexpr Selection getRadioIndex () { return 0; }
                        static constexpr Focus getFocusIndex () { return focusIndexV; }
                        static constexpr Coordinate getY () { return yV; }

                        T &widget;
                        WidgetTuple children;
                };

        } // namespace augument

        template <Focus f, Coordinate y, typename Parent, typename ChildrenTuple> constexpr auto transform (ChildrenTuple &tuple);

        // Wrapper for ordinary widgets
        template <typename T, typename Parent, Focus f, Selection r, Coordinate y, template <typename Wtu> typename Decor = DefaultDecor,
                  typename WidgetsTuple = Empty>
        struct Wrap {
                using WidgetType = T;
                static auto wrap (WidgetType &t) { return augument::Widget<WidgetType, f, r, y>{t}; }
        };

        // Wrapper for layouts
        template <typename Parent, Focus f, Selection r, Coordinate y, template <typename Wtu> typename Decor, typename WidgetsTuple>
        struct Wrap<Layout<Decor, WidgetsTuple>, Parent, f, r, y> {
                using WidgetType = Layout<Decor, WidgetsTuple>;

                static auto wrap (WidgetType &t)
                {
                        return augument::Layout<WidgetType, decltype (transform<f, y, WidgetType> (t.widgets)), Decor, f, y>{
                                t, transform<f, y, WidgetType> (t.widgets)};
                }
        };

        // Wrapper for groups
        template <typename Parent, Focus f, Selection r, Coordinate y, typename Callback, typename WidgetsTuple>
        struct Wrap<Group<Callback, WidgetsTuple>, Parent, f, r, y> {
                using WidgetType = Group<Callback, WidgetsTuple>;

                static auto wrap (WidgetType &t)
                {
                        return augument::Group<WidgetType, decltype (transform<f, y, WidgetType> (t.widgets)), f, y,
                                               Parent::template Decorator<typename WidgetType::Children>::height>{
                                t, transform<f, y, WidgetType> (t.widgets)};
                }
        };

        template <typename T, Focus f = 0, Selection r = 0, Coordinate y = 0> auto wrap (T &t)
        {
                using WidgetType = T;
                return Wrap<WidgetType, void, f, r, y>::wrap (t);
        }

        template <Focus f, Selection r, Coordinate y, typename Parent, typename Tuple, typename T, typename... Ts>
        constexpr auto transformImpl (Tuple &&prev, T &t, Ts &...ts)
        {
                using WidgetType = std::remove_cvref_t<T>;
                using Wrapper = Wrap<WidgetType, Parent, f, r, y>;
                using Wrapped = decltype (Wrapper::wrap (t));
                auto a = std::make_tuple (Wrapper::wrap (t));

                if constexpr (sizeof...(Ts) > 0) { // Parent(Layout)::Decor::filter -> hbox return 0 : vbox return height
                        return transformImpl<f + WidgetType::widgetCount, r + 1, y + Wrapped::getHeight (), Parent> (std::tuple_cat (prev, a),
                                                                                                                     ts...);
                }
                else {
                        return std::tuple_cat (prev, a);
                }
        }

        template <Focus f, Coordinate y, typename Parent, typename ChildrenTuple>
        constexpr auto transform (ChildrenTuple &tuple) // TODO use concept instead of "Children" prefix.
        {
                return std::apply (
                        [] (auto &...element) {
                                std::tuple dummy{};
                                return transformImpl<f, 0, y, Parent> (dummy, element...);
                        },
                        tuple);
        }

} // namespace detail

/*--------------------------------------------------------------------------*/

template <typename T> void log (T const &t, int indent = 0)
{
        auto l = [indent] (auto &itself, auto const &w, auto const &...ws) {
                // std::string used only for debug.
                // TODO consider removing std::string
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
template <template <typename Wtu> typename Decor, typename WidgetsTuple> struct Layout : public Widget<Layout<Decor, WidgetsTuple>> {

        using Base = Widget<Layout<Decor, WidgetsTuple>>;
        static constexpr uint16_t widgetCount = detail::Sum<WidgetsTuple, detail::WidgetCountField>::value;
        static constexpr bool canFocus = false;
        // static constexpr Dimension height = detail::Sum<WidgetsTuple, detail::WidgetHeightField>::value;
        // static constexpr Dimension height = Decor<WidgetsTuple>::height;

        template <typename Wtu> using Decorator = Decor<Wtu>;
        using Children = WidgetsTuple;

        explicit Layout (WidgetsTuple w) : widgets{std::move (w)} /* , augumentedWidgets{detail::transform (widgets)} */ {}

        Visibility operator() (auto &d) const
        {
                Iteration iter{};
                return operator() (d, d.context, iter);
        }

        Visibility operator() (auto &d, Context &ctx, Iteration &iter) const
        {
                // TODO move to separate function. Code duplication.
                // if (!detail::heightsOverlap (y, height, ctx.currentScroll, ctx.dimensions.height)) {
                //         return Visibility::outside;
                // }

                std::cerr << "operator ()" << std::endl;

                std::apply (
                        [&d, &ctx, &iter] (auto const &...widgets) {
                                detail::iterate (
                                        // This lambda get called for every widget in tne Layout (whether composite or not).
                                        [&d, &ctx] (auto const &widget, Iteration &iter, bool lastWidgetInLayout) {
                                                // It calls the operator () which is for drawing the widget on the screen.
                                                if (widget (d, ctx, iter) == Visibility::visible) {
                                                        if (!lastWidgetInLayout) {
                                                                Decor<WidgetsTuple>::after (ctx);
                                                        }
                                                }
                                        },
                                        ctx, iter, widgets...);
                        },
                        widgets);

                return Visibility::visible;
        }

        void scrollToFocus (Context &ctx, Iteration &iter) const
        {
                std::cerr << "scrollToFocus ()" << std::endl;

                std::apply (
                        [&ctx, &iter] (auto const &...widgets) {
                                detail::iterate ([&ctx] (auto const &widget, Iteration &iter, bool) { widget.scrollToFocus (ctx, iter); }, ctx,
                                                 iter, widgets...);
                        },
                        widgets);
        }

        void input (auto &d, char c)
        {
                Iteration iter{};
                input (d, d.context, iter, c);
        }

        void input (auto &d, Context &ctx, Iteration &iter, char c)
        {
                std::cerr << "input ()" << std::endl;

                std::apply (
                        [&d, &ctx, &iter, c] (auto &...widgets) {
                                detail::iterate ([&d, &ctx, c] (auto &widget, Iteration &iter, bool) { widget.input (d, ctx, iter, c); }, ctx,
                                                 iter, widgets...);
                        },
                        widgets);
        }

        void calculatePositions ()
        {
                Coordinate bottomY = y;
                std::apply ([this, &bottomY] (auto &...widgets) { (Decor<WidgetsTuple>::calculatePosition (widgets, bottomY), ...); }, widgets);
        }

        using Base::y;

        // private:
        WidgetsTuple widgets;
        // decltype (detail::transform (widgets)) augumentedWidgets; // TODO Can I ommit the widgets field, and leave only augumentedWidgets?
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
template <uint16_t ox, uint16_t oy, uint16_t widthV, uint16_t heightV, typename Child>
struct Window : public Widget<Window<ox, oy, widthV, heightV, Child>> {

        using Base = Widget<Window<ox, oy, widthV, heightV, Child>>;

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
                if (!detail::heightsOverlap (Base::y, height, ctx.currentScroll, ctx.dimensions.height)) {
                        return Visibility::outside;
                }

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
                if (context.currentFocus < widgetCount - 1) {
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

        static constexpr uint16_t width = widthV;   // Dimensions in charcters
        static constexpr uint16_t height = heightV; // Dimensions in charcters
        static constexpr uint16_t widgetCount = Child::widgetCount;
        static constexpr bool canFocus = false;
        mutable Context context{nullptr, {ox + 1, oy + 1}, {width - 2, height - 2}};
        Child child;
};

template <uint16_t ox, uint16_t oy, uint16_t widthV, uint16_t heightV> auto window (auto &&c)
{
        return Window<ox, oy, widthV, heightV, std::remove_reference_t<decltype (c)>> (std::forward<decltype (c)> (c));
}

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
        auto vb = ncurses<18, 7> (vbox (vbox (radio (0, " A "), radio (1, " B "), radio (2, " C "), radio (3, " d ")), //
                                        line<10>,                                                                      //
                                        vbox (check (" 1 "), check (" 2 "), check (" 3 "), check (" 4 ")),             //
                                        line<10>,                                                                      //
                                        vbox (radio (0, " a "), radio (0, " b "), radio (0, " c "), radio (0, " d ")), //
                                        line<10>,                                                                      //
                                        vbox (check (" 5 "), check (" 6 "), check (" 7 "), check (" 8 ")),             //
                                        line<10>,                                                                      //
                                        vbox (radio (0, " E "), radio (0, " F "), radio (0, " G "), radio (0, " H "))  //
                                        ));                                                                            //

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

        // auto vb = vbox (hbox (check (" A"), check (" B"), check (" C")), //
        //                 check (" A"),                                    //
        //                 check (" B"),                                    //
        //                 check (" C"));

        auto vb = hbox (check (" A"), check (" B"), check (" C"));

        // auto vb = vbox (group ([] (auto const &o) {}, radio (0, " R "), radio (1, " G "), radio (1, " B "), radio (1, " A ")), //
        //                 group ([] (auto const &o) {}, radio (0, " R "), radio (1, " G "), radio (1, " B "), radio (1, " A ")), //
        //                 check (" 1"), check (" 2"), check (" 3"), check (" 4"));

        // auto vb = vbox (label ("Combo"), Combo (Options (option (0, "red"), option (1, "green"), option (1, "blue")), [] (auto const &o) {}),
        //                 hbox (Radio2 (OptionsRad (radio (0, " R "), radio (1, " G "), radio (1, " B ")), [] (auto const &o) {})), check (" 5
        //                 "), check (" 6 "), check (" 7 "), check (" 8 "), check (" 5 "), check (" 6 "), check (" 7 "), check (" 8 "));

        // auto vb = vbox (hbox (label (""), check (" 1 "), check (" 2 ")),  //
        //                 hbox (label (""), check (" 5 "), check (" 6 "))); //
        // vb.print ();

        auto x = detail::wrap (vb);
        log (x);

        vb.calculatePositions (); // TODO get rid of this additional call
        bool showDialog{};

        auto dialog = window<4, 1, 10, 5> (vbox (label ("  Token"), label (" 123456"), button ("  [OK]", [&showDialog] { showDialog = false; }),
                                                 check (" dialg5"), check (" 6 "), check (" 7 "), check (" 8 ")));
        dialog.calculatePositions ();

        // TODO simplify this mess to a few lines. Minimal verbosity.
        while (true) {
                d1.clear ();
                vb (d1);

                if (showDialog) {
                        Iteration iter{}; // TODO mess
                        dialog (d1, d1.context, iter);
                }

                d1.refresh ();
                int ch = getch ();

                if (ch == 'q') {
                        break;
                }

                switch (ch) {
                case KEY_DOWN:
                        if (showDialog) {
                                dialog.incrementFocus (d1.context);
                        }
                        else {
                                vb.incrementFocus (d1.context);
                        }
                        break;

                case KEY_UP:
                        if (showDialog) {
                                dialog.decrementFocus (d1.context);
                        }
                        else {
                                vb.decrementFocus (d1.context);
                        }
                        break;

                case 'd':
                        showDialog = true;
                        break;

                default:
                        // d1.input (vb, char (ch));
                        if (showDialog) {
                                dialog.input (d1, char (ch));
                        }
                        else {
                                vb.input (d1, char (ch));
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
