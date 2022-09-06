/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

// See README.md for a brief code overview
namespace og {
enum class Visibility {
        visible,    // Widget drew something on the screen.
        outside,    // Widget is invisible because is outside the active region.
        nonDrawable // Widget is does not draw anything by itself.
};

struct Empty {};

using Coordinate = int16_t;
using Dimension = uint16_t;
using Focus = uint16_t;
using Selection = uint8_t;
using Color = uint16_t;
using LineOffset = int;

enum class CanFocus { no, yes };

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
        constexpr Dimensions (Dimension w = 0, Dimension h = 0) : width{w}, height{h} {}

        Dimension width{};
        Dimension height{};
};

/**
 * Runtime context for all the recursive loops.
 */
struct Context {
        Point const origin{};
        Dimensions const dimensions{};
        Focus currentFocus{};
        Coordinate currentScroll{};
};

enum class Key { unknown, incrementFocus, decrementFocus, select };

struct EmptyDisplay;

namespace c {

        /**
         * Layer 1 widget.
         */
        template <typename T>
        concept widget = requires (T const t, EmptyDisplay &d, Context const &c)
        {
                {
                        T::height
                        } -> std::same_as<Dimension const &>;
                {
                        t.template operator()<int> (d, c)
                        } -> std::same_as<Visibility>;

                // t.template input<int> (d, c, 'a'); // input is not required. Some widget has it, some hasn't
        };

        template <typename S>
        concept string = std::copyable<S> && requires (S s)
        {
                {
                        s.size ()
                        } -> std::convertible_to<std::size_t>;
        };

        template <typename S>
        concept text_buffer = string<S> && requires (S s)
        {
                typename S::value_type;

                {
                        s.cbegin ()
                        } -> std::input_iterator;

                {
                        s.cend ()
                        } -> std::input_iterator;
        };

} // namespace c

/**
 * A helper.
 */
template <typename... T> struct First {
        using type = std::tuple_element_t<0, std::tuple<T...>>;
};

template <typename... T> using First_t = typename First<T...>::type;

struct IDisplay {
        IDisplay () = default;
        IDisplay (IDisplay const &) = default;
        IDisplay &operator= (IDisplay const &) = default;
        IDisplay (IDisplay &&) = default;
        IDisplay &operator= (IDisplay &&) = default;
        virtual ~IDisplay () = default;

        virtual void print (std::span<const char> const &str) = 0;
        virtual void clear () = 0;
        virtual void color (Color clr) = 0;
        virtual void refresh () = 0;

        constexpr virtual Dimension width () const = 0;
        constexpr virtual Dimension height () const = 0;

        virtual Point &cursor () = 0;
        virtual Point const &cursor () const = 0;
};

namespace detail {

        // TODO export this mess to a named concept, and use a placeholder.
        template <typename String>
        requires requires (String str)
        {
                {
                        // TODO not acurate
                        str.data ()
                        } -> std::convertible_to<const char *>;
        }
        void print (IDisplay &disp, String const &str) { disp.print (std::span<const char> (str.begin (), str.end ())); }

} // namespace detail

/**
 * Abstract
 */
template <typename ConcreteClass, Dimension widthV, Dimension heightV> class AbstractDisplay : public IDisplay {
public:
        static constexpr Dimension width_ = widthV;   // Dimensions in characters
        static constexpr Dimension height_ = heightV; // Dimensions in characters

        constexpr Dimension width () const final { return width_; }
        constexpr Dimension height () const final { return height_; }

        Point &cursor () final { return cursor_; }
        Point const &cursor () const final { return cursor_; }

private:
        Point cursor_{};
};

struct EmptyDisplay : public AbstractDisplay<EmptyDisplay, 0, 0> {

        void print (std::span<const char> const &str) final {}
        void clear () final {}
        void color (Color clr) final {}
        void refresh () final {}
};

/****************************************************************************/

namespace detail {

        // Warning it moves the cursor!
        template <typename String = std::string_view> // No concept, no requirements
        void line (auto &d, uint16_t len, String const &ch = std::string_view ("-"))
        {
                for (uint16_t i = 0; i < len; ++i) {
                        detail::print (d, ch);
                        d.cursor ().x () += 1;
                }
        }

} // namespace detail

namespace detail {
        template <std::signed_integral C, std::unsigned_integral D> constexpr bool heightsOverlap (C y1, D height1, C y2, D height2)
        {
                auto y1d = y1 + height1 - 1;
                auto y2d = y2 + height2 - 1;
                return y1 <= y2d && y2 <= y1d;
        }

        static_assert (!heightsOverlap (-1, 1U, 0, 2U));
        static_assert (heightsOverlap (0, 1U, 0, 2U));
        static_assert (heightsOverlap (1, 1U, 0, 2U));
        static_assert (!heightsOverlap (2, 1U, 0, 2U));

} // namespace detail

struct Focusable {
        static constexpr CanFocus canFocus = CanFocus::yes;
};

/****************************************************************************/

template <Dimension len, char c> struct Line {
        static constexpr Dimension height = 1;
        static constexpr Dimension width = len;

        template <typename> Visibility operator() (auto &d, Context const & /* ctx */) const
        {
                detail::line (d, len); // TODO make character customizable
                return Visibility::visible;
        }
};

template <uint16_t len> Line<len, '-'> const line;

template <Dimension widthV, Dimension heightV> struct Space {
        static constexpr Dimension height = heightV;
        static constexpr Dimension width = widthV;

        template <typename> Visibility operator() (auto &d, Context const & /* ctx */) const
        {
                d.cursor () += {width, std::max (height - 1, 0)};
                return Visibility::visible;
        }
};

template <Dimension widthV, Dimension heightV> Space<widthV, heightV> const space;
template <Dimension widthV> Space<widthV, 1> const hspace;
template <Dimension heightV> Space<1, heightV> const vspace;

/****************************************************************************/
/* Check                                                                    */
/****************************************************************************/

template <typename Callback, typename ChkT, typename String>
requires c::string<std::remove_reference_t<String>>
class Check : public Focusable {
public:
        static constexpr Dimension height = 1;

        constexpr Check (Callback clb, ChkT chk, String const &lbl) : label_{lbl}, checked_{std::move (chk)}, callback{std::move (clb)} {}

        template <typename Wrapper> Visibility operator() (auto &disp, Context const &ctx) const;
        template <typename Wrapper> void input (auto & /* d */, Context const & /* ctx */, Key key)
        {
                if (key == Key::select) {
                        // TODO there's a problem here. When checked_ has std::reference_wrapper <bool> type it has to be casted as below. But if
                        // it's an int tha cast will fail. It shouldnt be, a concept should be more rigorous.
                        static_cast<bool &> (checked_) = !static_cast<bool &> (checked_);
                        // checked_ = !static_cast<bool> (checked_); // This won't work either when checked_ is std::reference_wrapper <bool>
                        callback (checked_);
                }
        }

        bool &checked () { return checked_; }
        bool const &checked () const { return checked_; }

        String const &label () { return label_; }
        String &label () const { return label_; }

private:
        String label_;
        ChkT checked_{};
        Callback callback;
};

/*--------------------------------------------------------------------------*/

template <typename Callback, typename ChkT, typename String>
requires c::string<std::remove_reference_t<String>>
template <typename Wrapper> Visibility Check<Callback, ChkT, String>::operator() (auto &d, Context const &ctx) const
{
        using namespace std::string_view_literals;

        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                d.color (2);
        }

        if (checked_) {
                detail::print (d, "☑"sv);
        }
        else {
                detail::print (d, "☐"sv);
        }

        d.cursor () += {1, 0}; // Customizable
        detail::print (d, label_);

        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                d.color (1);
        }

        d.cursor () += {Coordinate (label_.size ()), 0};
        return Visibility::visible;
}

/*--------------------------------------------------------------------------*/

template <typename T> struct EmptyUnaryInvocable {
        void operator() (T) {}
};

template <std::invocable<bool> Callback, c::string String>
Check (Callback clb, String const &lbl) -> Check<std::remove_cvref_t<Callback>, bool, std::unwrap_ref_decay_t<String>>;

template <std::invocable<bool> Callback, c::string String> constexpr auto check (Callback &&clb, String &&str)
{
        return Check<std::remove_cvref_t<Callback>, bool, std::unwrap_ref_decay_t<String>> (std::forward<Callback> (clb), {},
                                                                                            std::forward<String> (str));
}

template <std::convertible_to<bool> ChkT, c::string String>
requires (!std::invocable<ChkT, bool>) constexpr auto check (ChkT &&chk, String &&str)
{
        return Check<EmptyUnaryInvocable<bool>, std::remove_reference_t<ChkT>, std::unwrap_ref_decay_t<String>> ({}, std::forward<ChkT> (chk),
                                                                                                                 std::forward<String> (str));
}

// template <std::invocable<bool> Callback, std::convertible_to<bool> ChkT, c::string String>
// Check (Callback clb, ChkT chk, String const &str)
//         -> Check<std::remove_cvref_t<Callback>, std::remove_reference_t<ChkT>, std::unwrap_ref_decay_t<String>>;

// TODO change ChkT to ValueT - make this naming consistent in every widget
template <std::invocable<bool> Callback, std::convertible_to<bool> ChkT, c::string String>
constexpr auto check (Callback &&clb, ChkT &&chk, String &&str)
{
        return Check<std::remove_cvref_t<Callback>, std::remove_reference_t<ChkT>, std::unwrap_ref_decay_t<String>> (
                std::forward<Callback> (clb), std::forward<ChkT> (chk), std::forward<String> (str));
}

/****************************************************************************/
/* Radio                                                                    */
/****************************************************************************/

template <typename String, std::regular ValueT, bool radioVisible = true>
requires c::string<std::remove_reference_t<String>>
class Radio : public Focusable {
public:
        static constexpr Dimension height = 1;
        using Value = ValueT;

        constexpr Radio (Value value, String const &lbl) : id_{std::move (value)}, label_{lbl} {}
        template <typename Wrapper> Visibility operator() (auto &disp, Context const &ctx, ValueT const &value = {}) const;
        template <typename Wrapper, typename Callback> void input (auto &disp, Context const &ctx, Key key, Callback &clb, ValueT &value);

        // Value &id () { return id_; }
        Value const &id () const { return id_; }

        // String &label () { return label_; }
        String const &label () const { return label_; }

private:
        Value id_;
        String label_;
};

/*--------------------------------------------------------------------------*/

template <typename String, std::regular ValueT, bool radioVisible>
requires c::string<std::remove_reference_t<String>>
template <typename Wrapper>
Visibility Radio<String, ValueT, radioVisible>::operator() (auto &disp, Context const &ctx, ValueT const &value) const
{
        using namespace std::string_view_literals;

        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                disp.color (2);
        }

        if constexpr (radioVisible) {
                if (id () == value) {
                        detail::print (disp, "◉"sv);
                }
                else {
                        detail::print (disp, "○"sv);
                }

                disp.cursor () += {1, 0}; // TODO make customizable
        }

        detail::print (disp, label_);

        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                disp.color (1);
        }

        disp.cursor () += {Coordinate (label_.size ()), 0};
        return Visibility::visible;
}

template <typename String, std::regular ValueT, bool radioVisible>
requires c::string<std::remove_reference_t<String>>
template <typename Wrapper, typename Callback>
void Radio<String, ValueT, radioVisible>::input (auto & /* disp */, Context const & /* ctx */, Key key, Callback &clb, ValueT &value)
{
        if (key == Key::select) {
                value = id ();
                clb (id ());
        }
}

template <typename String, std::regular Id> constexpr auto radio (Id &&cid, String &&label)
{
        return Radio<std::unwrap_ref_decay_t<String>, std::decay_t<Id>, true> (std::forward<Id> (cid), std::forward<String> (label));
}

template <typename String, std::regular Id> constexpr auto item (Id &&cid, String &&label)
{
        return Radio<std::unwrap_ref_decay_t<String>, std::decay_t<Id>, false> (std::forward<Id> (cid), std::forward<String> (label));
}

template <typename T> struct is_radio : public std::bool_constant<false> {
};

template <typename String, typename Id, bool radioVisible> struct is_radio<Radio<String, Id, radioVisible>> : public std::bool_constant<true> {
};

/****************************************************************************/
/* Text                                                                     */
/****************************************************************************/

/**
 * Multiline text
 * TODO do not allow scrolling so the text inside is not visible. Stop scrolling when the last line is reached.
 */
template <Dimension widthV, Dimension heightV, typename Buffer>
requires c::text_buffer<std::remove_reference_t<Buffer>>
class Text {
public:
        using BufferType = std::remove_reference_t<Buffer>;
        static constexpr Dimension height = heightV;
        static constexpr Dimension width = widthV;

        /// Tip use std::ref
        constexpr explicit Text (Buffer const &b) : buffer_{b} {}

        template <typename /* Wrapper */> Visibility operator() (auto &d, Context const & /* ctx */) const;

        typename BufferType::const_iterator skipToLine (LineOffset line)
        {
                size_t charactersToSkip = line * widthV;
                return std::next (buffer_.cbegin (), std::min (charactersToSkip, buffer_.size ()));
        }

        LineOffset setStartLine (LineOffset line)
        {
                startLine = std::max (line, 0);

                size_t charNoInLine{};
                LineOffset linesCounted{};
                start = std::find_if (buffer_.cbegin (), buffer_.cend (), [&charNoInLine, &linesCounted, this] (auto c) {
                        if (startLine == linesCounted) {
                                return true;
                        }

                        if (c != '\n') {
                                ++charNoInLine;
                        }

                        if (c == '\n' || charNoInLine >= width) {
                                ++linesCounted;
                                charNoInLine = 0;
                        }

                        return false;
                });

                if (start == buffer_.cend ()) {
                        startLine = linesCounted;
                }

                return startLine;
        }

        Buffer const &buffer () { return buffer_; }
        Buffer &buffer () const { return buffer_; }

private:
        Buffer buffer_;
        typename BufferType::const_iterator start = buffer_.cbegin (); // start points to the start of what's visible
        LineOffset startLine{};
        bool scrollToBottom{};
};

template <Dimension widthV, Dimension heightV, typename Buffer>
requires c::text_buffer<std::remove_reference_t<Buffer>>
template <typename Wrapper> Visibility Text<widthV, heightV, Buffer>::operator() (auto &d, Context const &ctx) const
{
        size_t widgetScroll = std::max (ctx.currentScroll - Wrapper::getY (), 0);
        size_t heightToPrint = heightV - widgetScroll;

        Dimension len = buffer_.size ();
        std::array<char, widthV + 1> lineA;
        std::string_view line{lineA.data (), lineA.size ()};
        size_t totalCharactersCopied{};
        size_t linesPrinted{};
        size_t charactersToSkip = widgetScroll * widthV;
        auto iter = std::next (start, std::min (charactersToSkip, buffer_.size ()));

        while (totalCharactersCopied < len && linesPrinted++ < heightToPrint) {
                if (linesPrinted > 1) {
                        d.cursor () += {0, 1}; // New line
                }

                // Depending on the iterator category (input or random), distance can perform an iteration or simple subtraction respectively.
                auto lineCharactersToCopy = std::min (size_t (std::distance (iter, buffer_.cend ())), size_t (widthV));

                // We're looking for \n-s not from the buffer start, but from the visible contents start
                bool newLine{};
                auto end = std::next (iter, lineCharactersToCopy + 1); // 1 past the end
                if (auto lineEndIter = std::find (iter, end, '\n'); lineEndIter != end) {
                        lineCharactersToCopy = std::distance (iter, lineEndIter);
                        newLine = true;
                }

                auto i = std::copy_n (iter, lineCharactersToCopy, lineA.begin ());
                *i = '\0';
                std::advance (iter, lineCharactersToCopy + int (newLine)); // Skip the '\n' if necessary

                totalCharactersCopied += lineCharactersToCopy + int (newLine);
                detail::print (d, line);
        }

        d.cursor ().x () += widthV;
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
template <typename String>
requires c::string<std::remove_reference_t<String>>
class Label {
public:
        static constexpr Dimension height = 1;

        constexpr explicit Label (String const &l) : label_{l} {}

        template <typename /* Wrapper */> Visibility operator() (auto &d, Context const & /* ctx */) const
        {
                detail::print (d, label_);
                d.cursor () += {Coordinate (label_.size ()), 0};
                return Visibility::visible;
        }

        String const &label () { return label_; }
        String &label () const { return label_; }

private:
        String label_;
};

template <typename String> auto label (String &&str) { return Label<std::unwrap_ref_decay_t<String>> (std::forward<String> (str)); }

/****************************************************************************/
/* Button                                                                   */
/****************************************************************************/

/**
 *
 */
template <typename String, typename Callback>
requires c::string<std::remove_reference_t<String>>
class Button : public Focusable {
public:
        static constexpr Dimension height = 1;

        constexpr explicit Button (String const &lbl, Callback clb) : label_{lbl}, callback{std::move (clb)} {}

        template <typename Wrapper> Visibility operator() (auto &disp, Context const &ctx) const;

        template <typename> void input (auto & /* d */, Context const & /* ctx */, Key key)
        {
                if (key == Key::select) {
                        callback ();
                }
        }

        String const &label () { return label_; }
        String &label () const { return label_; }

private:
        String label_;
        Callback callback;
};

template <typename String, typename Callback>
requires c::string<std::remove_reference_t<String>>
template <typename Wrapper> Visibility Button<String, Callback>::operator() (auto &disp, Context const &ctx) const
{
        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                disp.color (2);
        }

        detail::print (disp, label_);
        disp.cursor () += {Coordinate (label_.size ()), 0};

        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                disp.color (1);
        }

        return Visibility::visible;
}

// TODO switch parameters wit echa other. This is to be consistent with the resto of the API
template <typename String, typename Callback> auto button (String &&str, Callback &&clb)
{
        return Button<std::unwrap_ref_decay_t<String>, std::decay_t<Callback>> (std::forward<String> (str), std::forward<Callback> (clb));
}

/****************************************************************************/
/* Combo                                                                    */
/****************************************************************************/

/**
 * Single combo option.
 */
template <std::regular ValueT, c::string String> struct Option {
public:
        using Value = ValueT;

        Option (Value val, String const &lbl) : value_{std::move (val)}, label_{lbl} {}

        Value &value () { return value_; }
        Value const &value () const { return value_; }

        String &label () { return label_; }
        String const &label () const { return label_; }

private:
        Value value_;
        String label_;
};

template <std::regular Value, c::string String> auto option (Value &&val, String &&label)
{
        return Option<std::remove_reference_t<Value>, std::unwrap_ref_decay_t<String>> (std::forward<Value> (val), std::forward<String> (label));
}

/**
 * A container for options.
 */
template <std::regular ValueT, size_t Num, c::string String> struct Options {
public:
        using OptionType = Option<ValueT, String>;
        using Value = ValueT;
        using ContainerType = std::array<OptionType, Num>;
        using SelectionIndex = typename ContainerType::size_type; // std::array::at accepts this

        template <typename... J> constexpr Options (Option<J, String> &&...elms) : elms{std::forward<Option<J, String>> (elms)...} {}

        SelectionIndex toIndex (Value const &cid) const;

        auto &getOptionByValue (Value const &cid) { return getOptionByIndex (toIndex (cid)); }
        auto const &getOptionByValue (Value const &cid) const { return getOptionByIndex (toIndex (cid)); }

        auto &getOptionByIndex (SelectionIndex idx) { return elms.at (idx); }
        auto const &getOptionByIndex (SelectionIndex idx) const { return elms.at (idx); }

        size_t size () const { return elms.size (); }

private:
        ContainerType elms;
};

template <typename String, typename... J> Options (Option<J, String> &&...elms) -> Options<First_t<J...>, sizeof...(J), String>;

/*--------------------------------------------------------------------------*/

template <std::regular I, size_t Num, c::string String>
typename Options<I, Num, String>::SelectionIndex Options<I, Num, String>::toIndex (Value const &cid) const
{
        size_t ret{};

        // This is brute-force. Can be improved.
        for (auto const &opt : elms) {
                if (opt.value () == cid) {
                        return ret;
                }

                ++ret;
        }

        return 0;
}

/**
 *
 */
template <typename Callback, typename ValueContainerT, CanFocus canFocusV, typename OptionCollection>
requires std::invocable<Callback, typename OptionCollection::Value>
class Combo {
public:
        static constexpr CanFocus canFocus = canFocusV;
        static constexpr Dimension height = 1;
        using Option = typename OptionCollection::OptionType;
        using Value = typename OptionCollection::Value;
        using ValueContainer = ValueContainerT;
        using SelectionIndex = typename OptionCollection::SelectionIndex;

        constexpr Combo (Callback clb, ValueContainer cid, OptionCollection const &opts)
            : options{opts}, valueContainer (std::move (cid)), callback{std::move (clb)}
        {
        }

        template <typename Wrapper> Visibility operator() (auto &disp, Context const &ctx) const;
        template <typename Wrapper> void input (auto &disp, Context const &ctx, Key key);

        ValueContainer &value () { return valueContainer; }
        ValueContainer const &value () const { return valueContainer; }

        SelectionIndex index () const { return index_; };

private:
        Value &toValue () { return static_cast<Value &> (valueContainer); }
        Value const &toValue () const { return static_cast<Value const &> (valueContainer); }

        OptionCollection options;
        ValueContainer valueContainer; // Can be int, can be std::ref (int)
        mutable SelectionIndex index_{};
        Callback callback;
};

/*--------------------------------------------------------------------------*/

template <typename Callback, typename ValueContainer, CanFocus canFocusV, typename OptionCollection>
requires std::invocable<Callback, typename OptionCollection::Value>
template <typename Wrapper>
Visibility Combo<Callback, ValueContainer, canFocusV, OptionCollection>::operator() (auto &disp, Context const &ctx) const
{
        if constexpr (canFocusV == CanFocus::yes) {
                if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                        disp.color (2);
                }
        }

        index_ = options.toIndex (value ());
        auto &label = options.getOptionByIndex (index_).label ();
        detail::print (disp, label);

        if constexpr (canFocusV == CanFocus::yes) {
                if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                        disp.color (1);
                }
        }

        disp.cursor () += {Coordinate (label.size ()), 0};
        return Visibility::visible;
}

/*--------------------------------------------------------------------------*/

template <typename Callback, typename ValueContainer, CanFocus canFocusV, typename OptionCollection>
requires std::invocable<Callback, typename OptionCollection::Value>
template <typename Wrapper>
void Combo<Callback, ValueContainer, canFocusV, OptionCollection>::input (auto & /* disp */, Context const &ctx, Key key)
{
        if constexpr (canFocusV == CanFocus::yes) {
                if (ctx.currentFocus == Wrapper::getFocusIndex () && key == Key::select) {
                        ++index_;
                        index_ %= options.size ();
                        toValue () = options.getOptionByIndex (index_).value ();
                        callback (toValue ());
                }
        }
}

/*--------------------------------------------------------------------------*/

template <typename CallbackT, typename... Opts>
requires std::invocable<CallbackT, typename First_t<Opts...>::Value>
auto combo (CallbackT &&clb, Opts &&...opts)
{
        using Value = typename First_t<Opts...>::Value;
        using Callback = std::remove_reference_t<CallbackT>;
        using OptionCollection = decltype (Options (std::forward<Opts> (opts)...));
        return Combo<Callback, Value, CanFocus::yes, OptionCollection> (std::forward<Callback> (clb), Value{},
                                                                        Options (std::forward<Opts> (opts)...));
}

template <typename CallbackT, typename ValueContainerT, typename... Opts>
requires std::invocable<CallbackT, typename First_t<Opts...>::Value> &&           //
        std::convertible_to<ValueContainerT, typename First_t<Opts...>::Value> && //
        (!std::invocable<ValueContainerT, typename First_t<Opts...>::Value>)      //

        auto combo (CallbackT &&clb, ValueContainerT &&value, Opts &&...opts)
{
        using ValueContainer = std::remove_reference_t<ValueContainerT>;
        using Callback = std::remove_reference_t<CallbackT>;
        using OptionCollection = decltype (Options (std::forward<Opts> (opts)...));

        return Combo<Callback, ValueContainer, CanFocus::yes, OptionCollection> (
                std::forward<CallbackT> (clb), std::forward<ValueContainerT> (value), Options (std::forward<Opts> (opts)...));
}

template <CanFocus canFocusV = CanFocus::yes, typename ValueContainerT, typename... Opts>
requires std::convertible_to<ValueContainerT, typename First_t<Opts...>::Value> &&(
        !std::invocable<ValueContainerT, typename First_t<Opts...>::Value>)auto combo (ValueContainerT &&cid, Opts &&...opts)
{
        using Value = typename First_t<Opts...>::Value;
        using Callback = EmptyUnaryInvocable<Value>;
        using ValueContainer = std::remove_reference_t<ValueContainerT>;
        using OptionCollection = decltype (Options (std::forward<Opts> (opts)...));

        return Combo<Callback, ValueContainer, canFocusV, OptionCollection> ({}, std::forward<ValueContainerT> (cid),
                                                                             Options (std::forward<Opts> (opts)...));
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
                                                   } -> std::convertible_to<CanFocus>;
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

        template <typename T> constexpr Dimension getWidth ()
        {
                if constexpr (requires { T::getWidth (); }) {
                        return T::getWidth ();
                }
                else if constexpr (requires { T::width; }) {
                        return T::width;
                }
                else {
                        return 0; // default
                }
        };

        template <typename T> struct FocusableWidgetCountField {
                static constexpr auto value = getFocusableWidgetCount<std::remove_reference_t<std::unwrap_ref_decay_t<T>>> ();
        };

        template <typename T> struct WidgetHeightField {
                static constexpr auto value = getHeight<std::remove_reference_t<std::unwrap_ref_decay_t<T>>> ();
        };

        template <typename T> struct WidgetWidthField {
                static constexpr auto value = getWidth<std::remove_reference_t<std::unwrap_ref_decay_t<T>>> ();
        };

        /*--------------------------------------------------------------------------*/

        template <typename Tuple, template <typename T> typename Field, int elemIdx = static_cast<int> (std::tuple_size_v<Tuple> - 1)>
        struct Sum {
                static constexpr auto value = Field<std::tuple_element_t<elemIdx, Tuple>>::value + Sum<Tuple, Field, elemIdx - 1>::value;
        };

        template <typename Tuple, template <typename T> typename Field> struct Sum<Tuple, Field, 0> {
                static constexpr auto value = Field<std::tuple_element_t<0, Tuple>>::value;
        };

        // Partial specialization for empty children list
        template <typename Tuple, template <typename T> typename Field> struct Sum<Tuple, Field, -1> {
                static constexpr int value = 0;
        };

        template <typename Tuple, template <typename T> typename Field, int elemIdx = static_cast<int> (std::tuple_size_v<Tuple> - 1)>
        struct Max {
                static constexpr auto value
                        = std::max (Field<std::tuple_element_t<elemIdx, Tuple>>::value, Max<Tuple, Field, elemIdx - 1>::value);
        };

        template <typename Tuple, template <typename T> typename Field> struct Max<Tuple, Field, 0> {
                static constexpr auto value = Field<std::tuple_element_t<0, Tuple>>::value;
        };

        // Partial specialization for empty children list
        template <typename Tuple, template <typename T> typename Field> struct Max<Tuple, Field, -1> {
                static constexpr int value = 0;
        };

        /*--------------------------------------------------------------------------*/
        /* Decorators for vertical and horizontal layouts.                          */
        /*--------------------------------------------------------------------------*/

        template <typename WidgetsTuple> struct VBoxDecoration {
                static constexpr Dimension width = detail::Max<WidgetsTuple, detail::WidgetWidthField>::value;
                static constexpr Dimension height = detail::Sum<WidgetsTuple, detail::WidgetHeightField>::value;

                static void after (auto &display, Context const &ctx, Point const &layoutOrigin)
                {
                        display.cursor ().y () += 1;
                        display.cursor ().x () = ctx.origin.x () + layoutOrigin.x ();
                }
                static constexpr Dimension getWidthIncrement (Dimension /* d */) { return 0; }
                static constexpr Dimension getHeightIncrement (Dimension disp) { return disp; }
        };

        template <typename WidgetsTuple> struct HBoxDecoration {
                static constexpr Dimension width = detail::Sum<WidgetsTuple, detail::WidgetWidthField>::value;
                static constexpr Dimension height = detail::Max<WidgetsTuple, detail::WidgetHeightField>::value;

                static void after (auto &display, Context const &ctx, Point const &layoutOrigin)
                {
                        display.cursor ().y () = std::max (ctx.origin.y () + layoutOrigin.y () - ctx.currentScroll, 0);
                }
                static constexpr Dimension getWidthIncrement (Dimension disp) { return disp; }
                static constexpr Dimension getHeightIncrement (Dimension /* d */) { return 0; }
        };

        struct NoDecoration {
                static constexpr Dimension height = 0;
                static constexpr void after (auto &display, Context const &ctx, Point const &layoutOrigin) {}
                static constexpr Dimension getWidthIncrement (Dimension /* d */) { return 0; }
                static constexpr Dimension getHeightIncrement (Dimension /* d */) { return 0; }
        };

} // namespace detail

/****************************************************************************/

/**
 * Radio group.
 */
template <typename Callback, typename ValueContainerT, typename WidgetTuple> class Group {
public:
        using ValueContainer = ValueContainerT;

        Group (Callback const &clb, ValueContainer value, WidgetTuple radios)
            : widgets_{std::move (radios)}, value_ (std::move (value)), callback_{clb}
        {
        }

        static constexpr Focus focusableWidgetCount = detail::Sum<WidgetTuple, detail::FocusableWidgetCountField>::value;

        using Children = WidgetTuple;

        Children &widgets () { return widgets_; }
        Children const &widgets () const { return widgets_; }

        ValueContainer &value () { return value_; }
        ValueContainer const &value () const { return value_; }

        Callback &callback () { return callback_; }
        Callback const &callback () const { return callback_; }

private:
        WidgetTuple widgets_;
        ValueContainer value_; // T, ref_wrap <T> etc
        Callback callback_;
};

/*--------------------------------------------------------------------------*/

template <typename T> struct is_group : public std::bool_constant<false> {
};

template <typename Callback, typename ValueContainerT, typename WidgetTuple>
struct is_group<Group<Callback, ValueContainerT, WidgetTuple>> : public std::bool_constant<true> {
};

namespace c {
        template <typename T>
        concept group = is_group<T>::value;
} // namespace c

/*--------------------------------------------------------------------------*/

template <typename CallbackT, typename... W>
requires std::invocable<CallbackT, typename First_t<W...>::Value> && std::conjunction_v<is_radio<W>...> // TODO requires that all the radio
                                                                                                        // values have the same type
auto group (CallbackT &&clb, W &&...widgets)
{
        using Callback = std::remove_reference_t<CallbackT>;
        using RadioCollection = decltype (std::tuple{std::forward<W> (widgets)...});
        using ValueContainer = typename First_t<W...>::Value;

        return Group<Callback, ValueContainer, RadioCollection>{std::forward<CallbackT> (clb), {}, std::tuple{std::forward<W> (widgets)...}};
}

template <typename ValueContainerT, typename... W>
requires (!std::invocable<ValueContainerT, typename First_t<W...>::Value>) &&  //
        std::convertible_to<ValueContainerT, typename First_t<W...>::Value> && //
        std::conjunction_v<is_radio<W>...>                                     //

        auto group (ValueContainerT &&val, W &&...widgets)
{
        using ValueContainer = std::remove_reference_t<ValueContainerT>;
        using RadioCollection = decltype (std::tuple{std::forward<W> (widgets)...});
        using Value = typename First_t<W...>::Value;

        return Group<EmptyUnaryInvocable<Value>, ValueContainer, RadioCollection>{
                {}, std::forward<ValueContainerT> (val), std::tuple{std::forward<W> (widgets)...}};
}

template <typename CallbackT, typename ValueContainerT, typename... W>
requires std::invocable<CallbackT, typename First_t<W...>::Value> &&           //
        (!std::invocable<ValueContainerT, typename First_t<W...>::Value>)&&    //
        std::convertible_to<ValueContainerT, typename First_t<W...>::Value> && //
        std::conjunction_v<is_radio<W>...>                                     //

        auto group (CallbackT &&clb, ValueContainerT &&val, W &&...widgets)
{
        using Callback = std::remove_reference_t<CallbackT>;
        using Value = std::remove_reference_t<ValueContainerT>;
        using RadioCollection = decltype (std::tuple{std::forward<W> (widgets)...});

        return Group<Callback, Value, RadioCollection>{std::forward<CallbackT> (clb), std::forward<ValueContainerT> (val),
                                                       std::tuple{std::forward<W> (widgets)...}};
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
        explicit Window (ChildT wgt) : child_{std::move (wgt)} {} // ChildT can be a widget (like Label) or reference_wrapper <Label>

        template <typename Wrapper> Visibility operator() (auto &disp, Context & /* ctx */) const // TODO Move outide the class
        {
                using namespace std::string_view_literals;
                disp.cursor () = {ox, oy};

                if constexpr (frame) {
                        detail::print (disp, "┌"sv); // TODO characters customizable
                        disp.cursor () += {1, 0};
                        detail::line (disp, width - F::cut);
                        detail::print (disp, "┐"sv);

                        for (int i = 0; i < height - F::cut; ++i) {
                                disp.cursor ().x () = ox;
                                disp.cursor () += {0, 1};
                                detail::print (disp, "│"sv);
                                disp.cursor () += {1, 0};
                                detail::line (disp, width - F::cut, " "sv);
                                detail::print (disp, "│"sv);
                        }

                        disp.cursor ().x () = ox;
                        disp.cursor () += {0, 1};
                        detail::print (disp, "└"sv);
                        disp.cursor () += {1, 0};
                        detail::line (disp, width - F::cut);
                        detail::print (disp, "┘"sv);

                        disp.cursor () = {ox + F::offset, oy + F::offset};
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
        template <Coordinate ox, Coordinate oy, Dimension widthV, Dimension heightV, bool frame = false> auto windowRaw (auto &&wgt)
        {
                return Window<ox, oy, widthV, heightV, frame, std::decay_t<decltype (wgt)>> (std::forward<decltype (wgt)> (wgt));
        }
} // namespace detail

/****************************************************************************/

// Forward declaration for is_layout
template <template <typename Wtu> typename Decor, typename WidgetsTuple, Dimension widthV = 0> class Layout;

template <typename Wtu> class DefaultDecor {
};

/// Type trait for discovering Layouts
template <typename T, template <typename Wtu> typename Decor = DefaultDecor, typename WidgetsTuple = Empty, Dimension widthV = 0>
struct is_layout : public std::bool_constant<false> {
};

template <template <typename Wtu> typename Decor, typename WidgetsTuple, Dimension widthV>
struct is_layout<Layout<Decor, WidgetsTuple, widthV>> : public std::bool_constant<true> {
};

namespace c {
        // template <typename T>
        // concept layout = is_layout<T>::value;

        template <typename L>
        concept layout = requires
        {
                // TODO is widget etc, the rest of stuff required.
                typename L::DecoratorType;
        };
} // namespace c

namespace detail {

        namespace augment {

                /**
                 * Additional information for all the widget contained in the Layout.
                 */
                template <typename T, Focus focusIndexV, Selection radioIndexV, Coordinate xV, Coordinate yV>
                requires c::widget<std::remove_reference_t<T>> // T can be for example Label or Label &. Only these 2 options.
                class Widget {
                public:
                        using Wrapped = std::remove_reference_t<T>;

                        constexpr Widget (T const &wgt) : widget{wgt} {}

                        static constexpr Dimension getWidth ();                             /// Width in characters.
                        static constexpr Dimension getHeight () { return Wrapped::height; } /// Height in characters.
                        static constexpr Coordinate getX () { return xV; }                  /// Position relative to the top left corner.
                        static constexpr Coordinate getY () { return yV; }                  /// Position relative to the top left corner.

                        /// Consecutive number in a radio group.
                        static constexpr Selection getRadioIndex () { return radioIndexV; } // TODO is this used?

                        /// Consecutive number (starting from 0) assigned for every focusable widget.
                        static constexpr Focus getFocusIndex () { return focusIndexV; }

                        Visibility operator() (auto &disp, Context const *ctx) const
                        {
                                if (!detail::heightsOverlap (getY (), getHeight (), ctx->currentScroll, ctx->dimensions.height)) {
                                        return Visibility::outside;
                                }

                                return widget.template operator()<Widget> (disp, *ctx);
                        }

                        template <typename ValueT> Visibility operator() (auto &disp, Context const *ctx, ValueT const &value) const
                        {
                                if (!detail::heightsOverlap (getY (), getHeight (), ctx->currentScroll, ctx->dimensions.height)) {
                                        return Visibility::outside;
                                }

                                return widget.template operator()<Widget> (disp, *ctx, value);
                        }

                        void input (auto &disp, Context const &ctx, Key key)
                        {
                                if (ctx.currentFocus == getFocusIndex ()) {
                                        if constexpr (requires { widget.template input<Widget> (disp, ctx, key); }) {
                                                widget.template input<Widget> (disp, ctx, key);
                                        }
                                }
                        }

                        template <typename Callback, typename ValueT>
                        void input (auto &disp, Context const &ctx, Key key, Callback &clb, ValueT &value)
                        {
                                if (ctx.currentFocus == getFocusIndex ()) {
                                        if constexpr (requires { widget.template input<Widget> (disp, ctx, key, clb, value); }) {
                                                widget.template input<Widget> (disp, ctx, key, clb, value);
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

                template <typename T, Focus focusIndexV, Selection radioIndexV, Coordinate xV, Coordinate yV>
                class is_widget_wrapper<Widget<T, focusIndexV, radioIndexV, xV, yV>> : public std::bool_constant<true> {
                };

                template <typename T>
                concept widget_wrapper = is_widget_wrapper<T>::value;

                template <typename T, Focus focusIndexV, Selection radioIndexV, Coordinate xV, Coordinate yV>
                requires c::widget<std::remove_reference_t<T>>
                constexpr Dimension Widget<T, focusIndexV, radioIndexV, xV, yV>::getWidth ()
                {
                        if constexpr (requires {
                                              {
                                                      Wrapped::width
                                                      } -> std::convertible_to<Dimension>;
                                      }) {
                                return Wrapped::width;
                        }
                        else {
                                return 0;
                        }
                }

                template <typename T, Focus focusIndexV, Selection radioIndexV, Coordinate xV, Coordinate yV>
                requires c::widget<std::remove_reference_t<T>>
                void Widget<T, focusIndexV, radioIndexV, xV, yV>::scrollToFocus (Context *ctx) const
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
                        Visibility operator() (auto &disp, Context *ctx) const // TODO move method body out
                        {
                                auto tmpCursor = disp.cursor ();

                                // Some container widgets have their own display method (operator ()). For instance og::Window
                                if constexpr (requires {
                                                      static_cast<ConcreteClass const *> (this)->widget.template operator()<ConcreteClass> (
                                                              disp, *ctx);
                                              }) {
                                        static_cast<ConcreteClass const *> (this)->widget.template operator()<ConcreteClass> (disp, *ctx);
                                }

                                // This iterates over the children
                                auto lbd = [&disp, &ctx, concrete = static_cast<ConcreteClass const *> (this)] (auto &itself, auto const &child,
                                                                                                                auto const &...children) {
                                        Visibility status{};

                                        if constexpr (requires { concrete->widget.value (); }) {
                                                status = child (disp, ctx, concrete->widget.value ());
                                        }
                                        else {
                                                status = child (disp, ctx);
                                        }

                                        if (status == Visibility::visible) {
                                                constexpr bool lastWidgetInLayout = (sizeof...(children) == 0);

                                                if (!lastWidgetInLayout) {
                                                        Decor::after (disp, *ctx, Point (ConcreteClass::getX (), ConcreteClass::getY ()));
                                                }
                                        }

                                        if constexpr (sizeof...(children) > 0) {
                                                itself (itself, children...);
                                        }
                                };

                                auto &concreteChildren = static_cast<ConcreteClass const *> (this)->children;

                                std::apply (
                                        [&lbd] (auto const &...children) {
                                                // In case of empty children list for example when vbox() was called (no args)
                                                if constexpr (sizeof...(children) > 0) {
                                                        lbd (lbd, children...);
                                                }
                                        },
                                        concreteChildren);

                                /**
                                 * Move the cursor to the bottom right corner, buut only if width of the container widget
                                 * is defined. This is kind of a hack which works when somebody either defines the layout
                                 * width explicitly using template argument (example : vbox <4>(xxx)), or when Layout's
                                 * contents have the width defined which is usually not the case. Most widgets display dynamic
                                 * contents like std::string and thus cannot figure out the width at compile time.
                                 */
                                if (ConcreteClass::getWidth ()) {
                                        disp.cursor () = tmpCursor;
                                        disp.cursor () += {ConcreteClass::getWidth (), std::max (ConcreteClass::getHeight () - 1, 0)};
                                }

                                return Visibility::visible;
                        }

                        void input (auto &disp, Context &ctx, Key key)
                        {
                                auto lbd = [&disp, &ctx, key, concrete = static_cast<ConcreteClass *> (this)] (auto &itself, auto &child,
                                                                                                               auto &...children) {
                                        if constexpr (requires {
                                                              concrete->widget.callback ();
                                                              concrete->widget.value ();
                                                      }) {

                                                child.input (disp, ctx, key, concrete->widget.callback (), concrete->widget.value ());
                                        }
                                        else {
                                                child.input (disp, ctx, key);
                                        }

                                        if constexpr (sizeof...(children) > 0) {
                                                itself (itself, children...);
                                        }
                                };

                                std::apply (
                                        [&lbd] (auto &...children) {
                                                if constexpr (sizeof...(children) > 0) {
                                                        lbd (lbd, children...);
                                                }
                                        },
                                        static_cast<ConcreteClass *> (this)->children);
                        }

                        void scrollToFocus (Context *ctx) const
                        {
                                // ctx = changeContext (ctx);

                                auto lbd = [&ctx] (auto &itself, auto const &child, auto const &...children) {
                                        child.scrollToFocus (ctx);

                                        if constexpr (sizeof...(children) > 0) {
                                                itself (itself, children...);
                                        }
                                };

                                std::apply (
                                        [&lbd] (auto const &...children) {
                                                if constexpr (sizeof...(children) > 0) {
                                                        lbd (lbd, children...);
                                                }
                                        },
                                        static_cast<ConcreteClass const *> (this)->children);
                        }
                };

                /**
                 * Layout
                 */
                template <typename T, typename WidgetTuple, Coordinate xV, Coordinate yV>
                requires c::layout<std::remove_reference_t<T>>
                class Layout : public ContainerWidget<Layout<T, WidgetTuple, xV, yV>, typename std::remove_reference_t<T>::DecoratorType> {
                public:
                        using Wrapped = std::remove_reference_t<T>;
                        constexpr Layout (T const &t, WidgetTuple c) : widget{t}, children{std::move (c)} {}

                        static constexpr Dimension getWidth ()
                        {
                                return std::max (Wrapped::template Decorator<WidgetTuple>::width, Wrapped::width);
                        }
                        static constexpr Dimension getHeight () { return Wrapped::template Decorator<WidgetTuple>::height; }
                        static constexpr Coordinate getX () { return xV; }
                        static constexpr Coordinate getY () { return yV; }

                        Visibility operator() (auto &d, Context *ctx) const
                        {
                                if (!detail::heightsOverlap (getY (), getHeight (), ctx->currentScroll, ctx->dimensions.height)) {
                                        return Visibility::outside;
                                }

                                return BaseClass::operator() (d, ctx);
                        }

                private:
                        using BaseClass = ContainerWidget<Layout<T, WidgetTuple, xV, yV>, typename std::remove_reference_t<T>::DecoratorType>;
                        template <typename X> friend void log (X const &, int);
                        template <typename CC, typename D> friend class ContainerWidget;

                        T widget;
                        WidgetTuple children;
                };

                template <typename T> struct is_layout_wrapper : public std::bool_constant<false> {
                };

                template <typename T, typename WidgetTuple, Coordinate xV, Coordinate yV>
                class is_layout_wrapper<Layout<T, WidgetTuple, xV, yV>> : public std::bool_constant<true> {
                };

                template <typename T>
                concept layout_wrapper = is_layout_wrapper<T>::value;

                /**
                 * Interface for dynamic polypomphism (see docs).
                 */
                struct IWindow {
                        virtual ~IWindow () = default;

                        virtual Visibility operator() (IDisplay &disp) const = 0;
                        virtual void input (IDisplay &disp, Key key) = 0;
                        virtual void incrementFocus (IDisplay &disp) const = 0;
                        virtual void decrementFocus (IDisplay &disp) const = 0;
                };

                /**
                 * Window
                 */
                template <typename T, typename Child> class Window : public IWindow, public ContainerWidget<Window<T, Child>, NoDecoration> {
                public:
                        using Wrapped = std::remove_reference_t<T>;
                        constexpr explicit Window (T const &t, Child c) : widget{t}, children{std::move (c)} {}

                        static constexpr Dimension getWidth () { return Wrapped::width; }
                        static constexpr Dimension getHeight () { return Wrapped::height; }
                        static constexpr Coordinate getX () { return Wrapped::x; }
                        static constexpr Coordinate getY () { return Wrapped::y; }

                        Visibility operator() (IDisplay &disp) const override { return BaseClass::operator() (disp, &context); }

                        void input (IDisplay &disp, Key key) override { BaseClass::input (disp, context, key); }

                        void incrementFocus (IDisplay & /* disp */) const override
                        {
                                if (context.currentFocus < Wrapped::focusableWidgetCount - 1) {
                                        ++context.currentFocus;
                                        scrollToFocus (&context);
                                }
                        }

                        void decrementFocus (IDisplay & /* disp */) const override
                        {
                                if (context.currentFocus > 0) {
                                        --context.currentFocus;
                                        scrollToFocus (&context);
                                }
                        }

                        mutable Context context{/* nullptr, */ {Wrapped::x + F::offset, Wrapped::y + F::offset},
                                                {Wrapped::width - F::cut, Wrapped::height - F::cut}};

                private:
                        template <typename X> friend void log (X const &, int);
                        template <typename CC, typename D> friend class ContainerWidget;

                        T widget;
                        std::tuple<Child> children;
                        friend ContainerWidget<Window, NoDecoration>;
                        using F = typename Wrapped::F;
                        // mutable Context context{/* nullptr, */ {Wrapped::x + F::offset, Wrapped::y + F::offset},
                        //                         {Wrapped::width - F::cut, Wrapped::height - F::cut}};

                        using BaseClass = ContainerWidget<Window<T, Child>, NoDecoration>;
                        using BaseClass::scrollToFocus, BaseClass::input, BaseClass::operator();
                };

                template <typename T> struct is_window_wrapper : public std::bool_constant<false> {
                };

                template <typename T, typename Child> class is_window_wrapper<Window<T, Child>> : public std::bool_constant<true> {
                };

                // template <typename T>
                // concept window_wrapper = is_window_wrapper<T>::value;

                template <typename T>
                concept window_wrapper = requires (T type, EmptyDisplay &disp, Key key)
                {
                        type.incrementFocus (disp);
                        type.decrementFocus (disp);
                        type.input (disp, key);

                        {
                                type.operator() (disp)
                                } -> std::same_as<Visibility>;
                };

                /****************************************************************************/

                /**
                 * Group
                 */
                template <typename T, typename WidgetTuple, Coordinate xV, Coordinate yV, Dimension widthV, Dimension heightV, typename Decor>
                class Group : public ContainerWidget<Group<T, WidgetTuple, xV, yV, widthV, heightV, Decor>, Decor> {
                public:
                        using Wrapped = std::remove_reference_t<T>;
                        constexpr Group (T const &t, WidgetTuple c) : widget{t}, children{std::move (c)} {}

                        static constexpr Dimension getWidth () { return widthV; }
                        static constexpr Dimension getHeight () { return heightV; }
                        static constexpr Coordinate getX () { return xV; }
                        static constexpr Coordinate getY () { return yV; }

                        Visibility operator() (auto &d, Context *ctx) const
                        {
                                if (!detail::heightsOverlap (getY (), getHeight (), ctx->currentScroll, ctx->dimensions.height)) {
                                        return Visibility::outside;
                                }

                                return BaseClass::operator() (d, ctx);
                        }

                private:
                        using BaseClass = ContainerWidget<Group<T, WidgetTuple, xV, yV, widthV, heightV, Decor>, Decor>;
                        template <typename X> friend void log (X const &, int);
                        template <typename CC, typename D> friend class ContainerWidget;

                        T widget;
                        WidgetTuple children;
                };

                template <typename T> struct is_group_wrapper : public std::bool_constant<false> {
                };

                template <typename T, typename WidgetTuple, Coordinate xV, Coordinate yV, Dimension widthV, Dimension heightV, typename Decor>
                class is_group_wrapper<Group<T, WidgetTuple, xV, yV, widthV, heightV, Decor>> : public std::bool_constant<true> {
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

                template <typename GrandParent, typename Parent> constexpr Dimension getWidthIncrement (Dimension d)
                {
                        // Parent : og::Layout or og::Group
                        if constexpr (is_layout<Parent>::value) {
                                return Parent::DecoratorType::getWidthIncrement (d);
                        }
                        else {
                                return GrandParent::DecoratorType::getWidthIncrement (d);
                        }
                }
        } // namespace augment

        template <Focus f, Coordinate x, Coordinate y, typename GrandParent, typename Parent, typename ChildrenTuple>
        constexpr auto transform (ChildrenTuple &tuple);

        // Wrapper for ordinary widgets
        // TODO can I replace x and y with 1 parameter of type Point?
        template <typename T, typename Parent, Focus f, Selection r, Coordinate x, Coordinate y> struct Wrap {

                template <typename W> static auto wrap (W &&t)
                {
                        return augment::Widget<std::unwrap_ref_decay_t<W>, f, r, x, y>{std::forward<W> (t)};
                }
        };

        // Wrapper for layouts
        template <c::layout T, typename Parent, Focus f, Selection r, Coordinate x, Coordinate y>
        requires c::layout<Parent> || c::window<Parent> || std::same_as<Parent, void>
        struct Wrap<T, Parent, f, r, x, y> {

                template <typename W> static auto wrap (W &&t)
                {
                        return augment::Layout<std::unwrap_ref_decay_t<W>,
                                               decltype (transform<f, x, y, Parent, T> (static_cast<T> (t).widgets ())), x, y>{
                                std::forward<W> (t), transform<f, x, y, Parent, T> (static_cast<T> (t).widgets ())};
                }
        };

        // Wrapper for groups
        template <c::group T, c::layout Parent, Focus f, Selection r, Coordinate x, Coordinate y> struct Wrap<T, Parent, f, r, x, y> {

                template <typename W> static auto wrap (W &&t)
                {
                        return augment::Group<std::unwrap_ref_decay_t<W>,
                                              decltype (transform<f, x, y, Parent, T> (static_cast<T> (t).widgets ())), x, y,
                                              Parent::template Decorator<typename T::Children>::width,
                                              Parent::template Decorator<typename T::Children>::height, typename Parent::DecoratorType>{
                                std::forward<W> (t), transform<f, x, y, Parent, T> (static_cast<T> (t).widgets ())};
                }
        };

        template <typename Parent = void, Focus f = 0, Selection r = 0, Coordinate x = 0, Coordinate y = 0> auto wrap (auto &&t);

        // Partial specialization for Windows
        template <c::window T, typename Parent, Focus f, Selection r, Coordinate x, Coordinate y>
        requires std::same_as<Parent, void> // Means that windows are always top level
        struct Wrap<T, Parent, f, r, x, y> {

                template <typename W> static auto wrap (W &&t)
                {
                        return augment::Window<std::unwrap_ref_decay_t<W>, decltype (og::detail::wrap<T, f, r, x, y> (t.child ()))> (
                                std::forward<W> (t), og::detail::wrap<T, f, r, x, y> (t.child ()));
                }
        };

        /// Default values for template arguments are in the forward declaration above
        template <typename Parent, Focus f, Selection r, Coordinate x, Coordinate y> auto wrap (auto &&t)
        {
                // RawType is for selecting the Wrapper partial specialization.
                using RawType = std::remove_reference_t<std::unwrap_ref_decay_t<decltype (t)>>;
                return Wrap<RawType, Parent, f, r, x, y>::wrap (std::forward<decltype (t)> (t));
        }

        template <Focus f, Selection r, Coordinate x, Coordinate y, typename GrandParent, typename Parent, typename Tuple, typename T,
                  typename... Ts>
        constexpr auto transformImpl (Tuple &&prev, T &t, Ts &...ts)
        {
                auto a = std::make_tuple (og::detail::wrap<Parent, f, r, x, y> (t));
                using Wrapped = std::tuple_element_t<0, decltype (a)>;

                if constexpr (sizeof...(Ts) > 0) { // Parent(Layout)::Decor::filter -> hbox return 0 : vbox return height
                        constexpr auto childWidthIncrement = augment::getWidthIncrement<GrandParent, Parent> (Wrapped::getWidth ());
                        constexpr auto childHeightIncrement = augment::getHeightIncrement<GrandParent, Parent> (Wrapped::getHeight ());

                        using WidgetType = std::remove_reference_t<std::unwrap_ref_decay_t<T>>;
                        return transformImpl<f + getFocusableWidgetCount<WidgetType> (), r + 1, x + childWidthIncrement,
                                             y + childHeightIncrement, GrandParent, Parent> (std::tuple_cat (prev, a), ts...);
                }
                else {
                        return std::tuple_cat (prev, a);
                }
        }

        template <Focus f, Coordinate x, Coordinate y, typename GrandParent, typename Parent, typename ChildrenTuple>
        constexpr auto transform (ChildrenTuple &tuple) // TODO use concept instead of "Children" prefix.
        {
                if constexpr (std::tuple_size<ChildrenTuple>::value == 0) {
                        return std::tuple{};
                }
                else {
                        return std::apply (
                                [] (auto &...element) {
                                        std::tuple dummy{};
                                        return transformImpl<f, 0, x, y, GrandParent, Parent> (dummy, element...);
                                },
                                tuple);
                }
        }

} // namespace detail

Visibility draw (auto &display, detail::augment::window_wrapper auto const &...window)
{
        display.clear ();
        Visibility ret = (window (display), ...);
        display.refresh ();
        return ret;
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

/**
 * Common interface for easy polymorphism.
 * TODO constrain KeyT with some concept, like std::integral but allowing enums.
 */
template <typename KeyT> class ISuite {
public:
        KeyT &current () { return current_; }
        KeyT const &current () const { return current_; }

private:
        KeyT current_{};
};

/**
 *
 */
template <typename KeyT, typename WinTuple>
// requires detail::augment::window_wrapper<std::remove_reference_t<WinTuple>> // TODO move to the factory method.
class Element {
public:
        using WinType = WinTuple;
        Element (KeyT key, WinTuple win) : key_{key}, win_{std::move (win)} {}

        KeyT key () const { return key_; };
        WinTuple const &win () const { return win_; };
        // WinTuple &win () { return win_; };

        auto &last () { return std::get<size_ - 1> (win ()); }
        auto const &last () const { return std::get<size_ - 1> (win ()); }

private:
        KeyT key_{};
        WinTuple win_{};
        static constexpr auto size_ = std::tuple_size_v<WinType>;
};

/**
 * Facotry method
 */
template <typename KeyT, typename... Win> auto element (KeyT key, Win &&...win)
{
        return Element{key, std::make_tuple (std::forward<Win> (win)...)};
}

/**
 * Suite of windows. Allows you to switch currently displayed window by setting a single
 * integer. Thanks to common interface class ISuite, you can easilly pass pointers to it
 * to your handlers.
 */
template <typename KeyT, typename WindowElementTuple> class WindowSuite : public detail::augment::IWindow, public ISuite<KeyT> {
public:
        using ISuite<KeyT>::current;

        explicit WindowSuite (WindowElementTuple wins) : windows{std::move (wins)} {}

        /// Returns the status of the last one
        Visibility operator() (IDisplay &display) const override
        {
                Visibility ret{};

                applyForOne ([&display, &ret] (auto const &elem) {
                        ret = std::apply ([&display] (auto const &...win) { return og::draw (display, win...); }, elem.win ());
                });

                return ret;
        }

        void input (IDisplay &display, Key key) override
        {
                applyForOne ([&display, key]<typename Elm> (Elm const &elem) {
                        constexpr auto i = std::tuple_size_v<typename Elm::WinType>;
                        std::get<i - 1> (elem.win ()).input (display, key);
                });
        }

        void incrementFocus (IDisplay &display) const override
        {
                applyForOne ([&display]<typename Elm> (Elm &elem) { elem.last ().incrementFocus (display); });
        }

        void decrementFocus (IDisplay &display) const override
        {
                applyForOne ([&display]<typename Elm> (Elm &elem) { elem.last ().decrementFocus (display); });
        }

private:
        template <typename Callback> void applyForOne (Callback const &clb) const;

        WindowElementTuple windows;
};

/*--------------------------------------------------------------------------*/

template <typename KeyT, typename WindowElementTuple>
template <typename Callback>
void WindowSuite<KeyT, WindowElementTuple>::applyForOne (Callback const &clb) const
{
        auto condition = [&clb, currentKey = current ()] (auto const &elem) -> bool {
                if (elem.key () == currentKey) {
                        clb (elem);
                        return true;
                }

                return false;
        };

        std::apply ([&condition] (auto const &...elms) { (condition (elms) || ...); }, windows);
}

/**
 * Facotry method
 */
template <typename KeyT, typename... Win> auto suite (Element<KeyT, Win> &&...el)
{
        using Tuple = decltype (std::tuple{std::forward<Element<KeyT, Win>> (el)...});

        return WindowSuite<KeyT, Tuple>{std::tuple{std::forward<Element<KeyT, Win>> (el)...}};
}

/****************************************************************************/

/**
 * Container for other widgets.
 */
template <template <typename Wtu> typename Decor, typename WidgetsTuple, Dimension widthV> class Layout {
public:
        static constexpr Focus focusableWidgetCount = detail::Sum<WidgetsTuple, detail::FocusableWidgetCountField>::value;
        template <typename Wtu> using Decorator = Decor<Wtu>;
        using DecoratorType = Decor<WidgetsTuple>;
        static constexpr Dimension width = widthV;

        explicit Layout (WidgetsTuple wgts) : widgets_{std::move (wgts)} {}

        WidgetsTuple &widgets () { return widgets_; }
        WidgetsTuple const &widgets () const { return widgets_; }

private:
        WidgetsTuple widgets_;
};

/*--------------------------------------------------------------------------*/

template <Dimension width = 0, typename... W> auto vbox (W &&...widgets)
{
        using WidgetsTuple = decltype (std::tuple{std::forward<W> (widgets)...});
        auto vbox = Layout<detail::VBoxDecoration, WidgetsTuple, width>{WidgetsTuple{std::forward<W> (widgets)...}};
        return vbox;
}

template <Dimension width = 0, typename... W> auto hbox (W &&...widgets)
{
        using WidgetsTuple = decltype (std::tuple{std::forward<W> (widgets)...});
        auto hbox = Layout<detail::HBoxDecoration, WidgetsTuple, width>{WidgetsTuple{std::forward<W> (widgets)...}};
        return hbox;
}

/****************************************************************************/

/// window factory function has special meaning that is also wraps.
template <Coordinate ox, Coordinate oy, Dimension widthV, Dimension heightV, bool frame = false, typename W = void> auto window (W &&c)
{
        return detail::wrap (detail::windowRaw<ox, oy, widthV, heightV, frame> (std::forward<W> (c)));
}

} // namespace og
