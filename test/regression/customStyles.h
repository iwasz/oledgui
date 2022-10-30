/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#pragma once
#include <oledgui/oledgui.h>

namespace og::style {

template <> struct Style<window> {
        static constexpr std::string_view nwcorner{"┌"};
        static constexpr std::string_view necorner{"┐"};
        static constexpr std::string_view swcorner{"└"};
        static constexpr std::string_view secorner{"┘"};
        static constexpr std::string_view vline{"│"};
        static constexpr std::string_view hline{"─"};
        // static constexpr bool frame = true;
};

// template <> struct Style<radio> {
//         static constexpr std::string_view checked{"◉"};
//         static constexpr std::string_view unchecked{"○"};
// };

} // namespace og::style

struct WindowsWithFrame {
        static constexpr bool frame = true;
};
