/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#pragma once
#include "oledgui.h"
#include "sys/printk.h"
#include <type_traits>
#include <zephyr/display/cfb.h>
#include <zephyr/zephyr.h>

#define debugMacro printk

namespace og::zephyr::cfb {

/**
 * Ncurses backend.
 */
template <Dimension widthV, Dimension heightV, typename Child = Empty>
class Display : public og::Display<Display<widthV, heightV, Child>, widthV, heightV, Child> {
public:
        using Base = og::Display<Display<widthV, heightV, Child>, widthV, heightV, Child>;
        using Base::cursor;
        using Base::width, Base::height;

        Display (device const *disp, Child c = {});

        template <typename String>
        requires requires (String s)
        {
                {
                        s.data ()
                        } -> std::convertible_to<const char *>;
        }

        void print (String const &str)
        {
                if (cfb_print (display, const_cast<char *> (static_cast<const char *> (str.data ())), cursor ().x () * 7, cursor ().y () * 8)
                    != 0) {
                        printk ("cfb_print is unable to print\r\n");
                }

                if (color_ == 2) {
                        if (int err = cfb_invert_area (display, cursor ().x () * 7, cursor ().y () * 8, str.size () * 7, 8); err != 0) {
                                printk ("Could not invert (err %d)\n", err);
                        }
                }

                // printk ("print (%s), %d,%d\r\n", const_cast<char *> (static_cast<const char *> (str.data ())), cursor ().x () * 7,
                //         cursor ().y () * 8);
        }

        void clear ()
        {
                if (int err = cfb_framebuffer_clear (display, true); err) {
                        printk ("Could not clear framebuffer (err %d)\n", err);
                }

                cursor ().x () = 0;
                cursor ().y () = 0;
                // printk ("clear\r\n");
        }

        void color (Color color)
        {
                color_ = color;
                // printk ("color %d,%d\r\n", cursor ().x () * 7, cursor ().y () * 8);
        }

        void refresh ()
        {
                if (int err = cfb_framebuffer_finalize (display); err) {
                        printk ("Could not finalize framebuffer (err %d)\n", err);
                }
                // printk ("refresh\r\n");
        }

private:
        using Base::child;
        device const *display{};
        Color color_{};
};

template <Dimension widthV, Dimension heightV, typename Child>
Display<widthV, heightV, Child>::Display (device const *disp, Child c) : Base (c), display{disp}
{
        if (!device_is_ready (display)) {
                printk ("Display device not ready\n");
                return;
        }
}

template <Dimension widthV, Dimension heightV> auto zephyrCfb (auto &&child)
{
        return Display<widthV, heightV, std::remove_reference_t<decltype (child)>>{std::forward<decltype (child)> (child)};
}

} // namespace og::zephyr::cfb
