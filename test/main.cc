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

namespace detail {

        struct IDecoration {
                virtual void after (Context const &ctx) const = 0;
        };
} // namespace detail

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
        detail::IDecoration const *decoration{};
};

/// These classes are for simplyfying the Widget API. Instead of n function arguments we have those 2 aggreagtes.
struct Iteration {
        uint16_t focusIndex{};
        Selection radioIndex{};
};

template <typename ConcreteClass, uint16_t widthV, uint16_t heightV, typename Child = Empty> class Display {
public:
        Display (Child const &c = {}) : child{c} {}

        Visibility operator() ()
        {
                static_cast<ConcreteClass *> (this)->clear (); // How cool.
                auto v = child (*static_cast<ConcreteClass *> (this), context);
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

/*--------------------------------------------------------------------------*/

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

        void reFocus (Context &ctx, uint16_t focusIndex = 0) const;
        void input (auto &d, Context const &ctx, Iteration iter, char c) {}
        // TODO remove
        void calculatePositions (uint16_t) {}

        void incrementFocus (Context &ctx) const
        {
                if (ctx.currentFocus < ConcreteClass::widgetCount - 1) {
                        ++ctx.currentFocus;
                        static_cast<ConcreteClass const *> (this)->reFocus (ctx);
                }
        }

        void decrementFocus (Context &ctx) const
        {
                if (ctx.currentFocus > 0) {
                        --ctx.currentFocus;
                        static_cast<ConcreteClass const *> (this)->reFocus (ctx);
                }
        }

        uint16_t y{};
        static constexpr uint16_t widgetCount = widgetCountV;
        static constexpr uint16_t height = heightV;
};

template <typename ConcreteClass, uint16_t widgetCountV, uint16_t heightV>
void Widget<ConcreteClass, widgetCountV, heightV>::reFocus (
        Context &ctx, uint16_t focusIndex) const // TODO the name of tyhis function is misleading. It should be like scrollToFocus otr sth
{
        auto h = ConcreteClass::height;
        if (!detail::heightsOverlap (y, h, ctx.currentScroll, ctx.dimensions.height)) {
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

template <uint16_t len> struct Line : public Widget<Line<len>, 0, 1> {

        using Widget<Line<len>, 0, 1>::y, Widget<Line<len>, 0, 1>::height;

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

class Check : public Widget<Check, 1, 1> {
public:
        constexpr Check (const char *s, bool c) : label{s}, checked{c} {}

        // TODO move outside the class.
        // TODO remove radioIndex and groupSelection. Not needed here.
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

        void input (auto & /* d */, Context const &ctx, Iteration iter, char c)
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

class Radio : public Widget<Radio, 1, 1> {
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

        if (iter.radioIndex == *ctx.radioSelection) {
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

void Radio::input (auto & /* d */, Context const &ctx, Iteration iter, char c)
{
        if (ctx.currentFocus == iter.focusIndex && c == ' ') { // TODO character must be customizable (compile time)
                *ctx.radioSelection = iter.radioIndex;
        }
}

constexpr Radio radio (const char *label) { return {label}; }

/****************************************************************************/
/* Label                                                                    */
/****************************************************************************/

/**
 * Single line (?)
 */
class Label : public Widget<Radio, 0, 1> {
public:
        constexpr Label (const char *l) : label{l} {}
        Visibility operator() (auto &d, Context const &ctx, Iteration /* iter */) const
        {
                if (!detail::heightsOverlap (y, height, ctx.currentScroll, ctx.dimensions.height)) { // TODO Move from here, duplication.
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
template <typename Callback> class Button : public Widget<Radio, 1, 1> {
public:
        constexpr Button (const char *l, Callback const &c) : label{l}, callback{c} {}

        Visibility operator() (auto &d, Context const &ctx, Iteration iter) const;

        void input (auto & /* d */, Context const &ctx, Iteration iter, char c)
        {
                if (ctx.currentFocus == iter.focusIndex && c == ' ') {
                        callback ();
                }
        }

private:
        const char *label;
        Callback callback;
};

template <typename Callback> Visibility Button<Callback>::operator() (auto &d, Context const &ctx, Iteration iter) const
{
        if (!detail::heightsOverlap (y, height, ctx.currentScroll, ctx.dimensions.height)) { // TODO Move from here, duplication.
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

template <typename... T> struct First {
        using type = std::tuple_element_t<0, std::tuple<T...>>;
};

template <typename... T> using First_t = typename First<T...>::type;

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

        Visibility operator() (auto &d, Context const &ctx, Iteration iter) const;
        void input (auto &d, Context const &ctx, Iteration iter, char c);

private:
        OptionCollection options;
        SelectionIndex currentSelection{};
        Callback callback;
};

/*--------------------------------------------------------------------------*/

template <typename OptionCollection, typename Callback>
Visibility Combo<OptionCollection, Callback>::operator() (auto &d, Context const &ctx, Iteration iter) const
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
void Combo<OptionCollection, Callback>::input (auto & /* d */, Context const &ctx, Iteration iter, char c)
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
class Radio2 : public Widget<Radio2<OptionCollection, Callback>> {
public:
        using Option = typename OptionCollection::OptionType;
        using Id = typename OptionCollection::Id;
        using Base = Widget<Radio2<OptionCollection, Callback>>;
        using Base::y;
        using SelectionIndex = typename OptionCollection::SelectionIndex;

        static constexpr uint16_t widgetCount = std::tuple_size<typename OptionCollection::ContainerType>::value;
        static constexpr uint16_t height = widgetCount;

        constexpr Radio2 (OptionCollection const &o, Callback c) : options{o}, callback{c} {}

        Visibility operator() (auto &d, Context const &ctx, Iteration iter) const;
        void input (auto &d, Context const &ctx, Iteration iter, char c);

private:
        OptionCollection options;
        SelectionIndex currentSelection{};
        Callback callback;
};

/*--------------------------------------------------------------------------*/

template <typename OptionCollection, typename Callback>
Visibility Radio2<OptionCollection, Callback>::operator() (auto &d, Context const &ctx, Iteration iter) const
{
        if (!detail::heightsOverlap (y, height, ctx.currentScroll, ctx.dimensions.height)) {
                return Visibility::outside;
        }

        for (SelectionIndex cnt = 0; auto const &o : options.elms) {

                if (ctx.currentFocus == iter.focusIndex + cnt) {
                        d.color (2);
                }
                if (cnt == currentSelection) {
                        d.print ("◉");
                }
                else {
                        d.print ("○");
                }
                d.move (1, 0);
                d.print (o.label);

                if (ctx.decoration) {
                        ctx.decoration->after (ctx); // TODO extract method
                }

                if (ctx.currentFocus == iter.focusIndex + cnt) {
                        d.color (1);
                }

                ++cnt;
        }

        // d.move (strlen (label), 0);
        return Visibility::visible;
}

/*--------------------------------------------------------------------------*/

template <typename OptionCollection, typename Callback>
void Radio2<OptionCollection, Callback>::input (auto & /* d */, Context const &ctx, Iteration iter, char c)
{
        if (ctx.currentFocus == iter.focusIndex && c == ' ') { // TODO character must be customizable (compile time)
                ++currentSelection;
                currentSelection %= options.elms.size ();
        }
}

/****************************************************************************/
/* Layouts                                                                  */
/****************************************************************************/
namespace detail {

        struct VBoxDecoration : public IDecoration {
                void after (Context const &ctx) const override
                {
                        ctx.cursor->y += 1;
                        ctx.cursor->x = ctx.origin.x;
                }
        };

        struct HBoxDecoration : public IDecoration {
                void after (Context const &ctx) const override
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

template <typename Decor, typename WidgetsTuple> struct Layout : public Widget<Layout<Decor, WidgetsTuple>> {

        using Base = Widget<Layout<Decor, WidgetsTuple>>;
        static constexpr uint16_t widgetCount = detail::Sum<WidgetsTuple, detail::WidgetCountField>::value;
        static constexpr uint16_t height = detail::Sum<WidgetsTuple, detail::WidgetHeightField>::value;

        // TODO this is westeful, every Layout in the hierarchy will fire this methiod, while only the most external one should.
        explicit Layout (WidgetsTuple w) : widgets (std::move (w)) { calculatePositions (); }

        Visibility operator() (auto &d) const { return operator() (d, d.context, {}); }
        Visibility operator() (auto &d, Context &ctx, Iteration iter = {}) const
        {
                ctx.decoration = &decoration;

                // TODO move to separate function. Code duplication.
                if (!detail::heightsOverlap (y, height, ctx.currentScroll, ctx.dimensions.height)) {
                        return Visibility::outside;
                }
                ctx.radioSelection = &radioSelection;

                auto l = [&d, &ctx, this] (auto &itself, Iteration iter, auto const &widget, auto const &...widgets) {
                        if (widget (d, ctx, iter) == Visibility::visible) {
                                decoration.after (ctx);
                        }

                        if constexpr (sizeof...(widgets) > 0) {
                                itself (itself, {uint16_t (iter.focusIndex + widget.widgetCount), Selection (iter.radioIndex + 1)}, widgets...);
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
                                itself (itself, {uint16_t (iter.focusIndex + widget.widgetCount), Selection (iter.radioIndex + 1)}, widgets...);
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

        // uint16_t y{}; // TODO constexpr
        using Base::y;

private:
        mutable uint8_t radioSelection{};
        WidgetsTuple widgets;
        Decor decoration{};
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
template <uint16_t ox, uint16_t oy, uint16_t widthV, uint16_t heightV, typename Child>
struct Window : public Widget<Window<ox, oy, widthV, heightV, Child>> {

        using Base = Widget<Window<ox, oy, widthV, heightV, Child>>;

        explicit Window (Child const &c) : child{c} {}

        Visibility operator() (auto &d) const { return operator() (d, d.context, {}); }
        // Visibility operator() (auto &d, Context &ctx, Iteration iter = {}) const
        // {
        //         d.cursorX = ox;
        //         d.cursorY = oy;
        //         return child (d, context, iter);
        // }

        // TODO This always prints the frame. There should be option for a windows without one.
        Visibility operator() (auto &d, Context &ctx, Iteration iter = {}) const
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

        void input (auto &d, char c) { input (d, context, {}, c); }
        void input (auto &d, Context & /* ctx */, Iteration iter, char c) { child.input (d, context, iter, c); }
        void reFocus (Context & /* ctx */, uint16_t focusIndex = 0) const { child.reFocus (context, focusIndex); }

        void incrementFocus (Context & /* ctx */) const
        {
                if (context.currentFocus < widgetCount - 1) {
                        ++context.currentFocus;
                        reFocus (context);
                }
        }

        void decrementFocus (Context & /* ctx */) const
        {
                if (context.currentFocus > 0) {
                        --context.currentFocus;
                        reFocus (context);
                }
        }

        static constexpr uint16_t width = widthV;   // Dimensions in charcters
        static constexpr uint16_t height = heightV; // Dimensions in charcters
        static constexpr uint16_t widgetCount = Child::widgetCount;
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

        auto vb = vbox (
                vbox (label ("Combo"), Combo (Options (option (0, "red"), option (1, "green"), option (1, "blue")), [] (auto const &o) {})), //
                line<10>,                                                                                                                    //
                vbox (label ("New radio"),
                      Radio2 (Options (option (0, " red"), option (1, " green"), option (1, " blue")), [] (auto const &o) {})),
                line<10>,                                                                               //
                vbox (label ("Old radio"), radio (" a "), radio (" b "), radio (" c "), radio (" d ")), //
                line<10>,                                                                               //
                vbox (label ("Checkbox"), check (" 1 "), check (" 2 "), check (" 3 "), check (" 4 ")),  //
                line<10>,                                                                               //
                vbox (check (" 5 "), check (" 6 "), check (" 7 "), check (" 8 ")),                      //
                line<10>,                                                                               //
                vbox (vbox (radio (" x "), radio (" y "), radio (" z "), radio (" ź ")),                //
                      vbox (radio (" ż "), radio (" ą "), radio (" ę "), radio (" ł ")))                //
        );                                                                                              //

        bool showDialog{};

        auto dialog = window<4, 1, 10, 5> (vbox (label ("  Token"), label (" 123456"), button ("  [OK]", [&showDialog] { showDialog = false; }),
                                                 check (" dialg5"), check (" 6 "), check (" 7 "), check (" 8 ")));

        // TODO simplify this mess to a few lines. Minimal verbosity.
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
        test1 ();
}
