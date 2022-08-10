/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#pragma once
#include "oledgui.h"
#include <iostream>

namespace og {

/**
 * Simplest dumb terminal backend.
 */
template <Dimension widthV, Dimension heightV, typename Child = Empty>
class TerminalDisplay : public Display<TerminalDisplay<widthV, heightV, Child>, widthV, heightV, Child> {
public:
        using Base = Display<TerminalDisplay<widthV, heightV, Child>, widthV, heightV, Child>;
        using Base::cursor;
        using Base::width, Base::height;

        template <typename String>
        requires requires (String s) { std::cout << s; }
        void print (String const &str) { std::cout << "\033[" << cursor ().y () + 1 << ";" << cursor ().x () + 1 << "H" << str << std::flush; }

        void clear ()
        {
                cursor () = {0, 0};
                color (0);

                for (int y = 0; y < heightV; ++y) {
                        std::cout << "\033[" << y + 1 << ";0H" << std::flush;
                        for (int x = 0; x < widthV; ++x) {
                                std::cout.put (' ');
                        }
                }
        }

        void color (Color c)
        {
                if (c == 0) {
                        std::cout << "\033[44m" << std::flush;
                }
                else if (c == 1) {
                        std::cout << "\033[47m" << std::endl;
                }
        }

        void refresh () { std::cout << "\033[0m" << std::flush; }

private:
        using Base::child;
};

og::Key getKey (int ch)
{
        // TODO characters must be customizable (compile time)
        switch (ch) {
        case 's':
                return Key::incrementFocus;

        case 'w':
                return Key::decrementFocus;

        case int (' '):
                return Key::select;

        default:
                return Key::unknown;
        }
}

og::Key getKey () { return getKey (std::cin.get ()); }

} // namespace og
