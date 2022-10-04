/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#pragma once
#include "oledgui.h"

namespace og {

/****************************************************************************/
/* Progress bar                                                             */
/****************************************************************************/

// constexpr int numCharactersPerSegment = 2;
// constexpr std::array segmentCharacters = {'.', ':'};
constexpr std::array segmentCharacters = {std::string_view{"."}, std::string_view{":"}};

/**
 * A progress bar (not focusable, not editable).
 * Template (and runtime) params:
 * - Width in characters
 * - Minimum value
 * - Maximum value
 *
 * CSS-style parameters
 * - Number of unique characters for displaying progress among one segment
 * - These characters (an std::array?)
 */
template <typename ValueT,                              /// Value of this exact type is stored inside this class. For example int or int &.
          Dimension widthV,                             /// Width in characters.
          typename std::remove_reference_t<ValueT> min, /// Minimum value, the bar is empty when this value is set
          typename std::remove_reference_t<ValueT> max> /// Maximum value, the bar is full when this value is set
requires std::integral<typename std::remove_reference_t<ValueT>>
class Progress {
public:
        static constexpr Dimension height = 1;
        static constexpr Dimension width = widthV;
        using Value = std::remove_reference_t<ValueT>; // std::integral or a reference like int or int &

        constexpr explicit Progress (ValueT const &cid) : valueContainer (cid) {}
        template <typename Wrapper> Visibility operator() (auto &disp, Context const &ctx) const;

        Value &value () { return valueContainer; }
        Value const &value () const { return valueContainer; }

private:
        static constexpr Value span = std::abs (max - min);
        static constexpr Value unit = span / width;
        static_assert (unit != 0, "(max - min) / width equals 0 (integer math). Use floating point types if you want fractional increments.");

        ValueT valueContainer; // Can be int, can be int &. Or any other std::integral and its reference
};

/*--------------------------------------------------------------------------*/

template <typename ValueT, Dimension widthV, typename std::remove_reference_t<ValueT> min, typename std::remove_reference_t<ValueT> max>
requires std::integral<typename std::remove_reference_t<ValueT>>
template <typename Wrapper> Visibility Progress<ValueT, widthV, min, max>::operator() (auto &disp, Context const & /* ctx */) const
{
        auto tmpX = disp.cursor ().x ();
        Value const barLen = valueContainer / unit;

        for (int i = 0; i < barLen; ++i) {
                detail::print (disp, segmentCharacters.back ());
                ++disp.cursor ().x ();
        }

        Value const fraction = valueContainer % unit;

        if (fraction > 0) {
                size_t character = (fraction - 1) * segmentCharacters.size () / unit;
                detail::print (disp, segmentCharacters.at (character));
                ++disp.cursor ().x ();
        }

        disp.cursor ().x () = tmpX + width;
        return Visibility::visible;
}

/*--------------------------------------------------------------------------*/

template <auto widthV, auto min, auto max, typename ValueT>
requires std::integral<typename std::remove_reference_t<std::unwrap_ref_decay_t<ValueT>>>
auto progress (ValueT &&val) { return Progress<std::unwrap_ref_decay_t<ValueT>, widthV, min, max>{std::forward<ValueT> (val)}; }

// template <auto widthV, auto min, auto max, typename ValueT>
// requires std::integral<typename std::remove_reference_t<std::unwrap_ref_decay_t<ValueT>>> Progress (ValueT const &)
// ->Progress<std::unwrap_ref_decay_t<ValueT>, widthV, min, max>;

} // namespace og