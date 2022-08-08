/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "oledgui.h"
#include <iomanip>
#include <iostream>

// This file include potantially memory hungry bits fo STD C++ library.
namespace og {

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
#if defined(__cpp_rtti)
                        std::string name (typeid (w.widget).name ());
#else
                        std::string name ("unknown");
#endif
                        name.resize (std::min<std::string::size_type> (32UL, name.size ()));
                        std::cout << ", x: " << w.getX () << ", y: " << w.getY () << ", w: " << w.getWidth () << ", h: " << w.getHeight ()
                                  << ", " << name << std::endl;

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

} // namespace og
