/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#pragma once
#include "oledgui.h"
#include <type_traits>
#include <zephyr/display/cfb.h>
#include <zephyr/sys/printk.h>

// #define debugMacro printk

namespace og::zephyr::cfb {

/**
 * Zephyr backend
 * TODO make character width and height customizable (template param)
 * TODO optimize this "old-school" font because now there are 2 pixel vertical spaces between the glyphs.
 */
template <Dimension widthV, Dimension heightV> class Display : public AbstractDisplay<Display<widthV, heightV>, widthV, heightV> {
public:
        using Base = og::AbstractDisplay<Display<widthV, heightV>, widthV, heightV>;
        using Base::cursor;
        using Base::width, Base::height;

        Display (device const *disp);

        void print (std::span<const char> const &str) override
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
        }

        void clear () override
        {
                if (int err = cfb_framebuffer_clear (display, true); err) {
                        printk ("Could not clear framebuffer (err %d)\n", err);
                }

                cursor ().x () = 0;
                cursor ().y () = 0;
                // printk ("clear\r\n");
        }

        void style (Style stl) override
        {
                style_ = stl;
                // printk ("style %d,%d\r\n", cursor ().x () * 7, cursor ().y () * 8);
        }

        void refresh () override
        {
                if (int err = cfb_framebuffer_finalize (display); err) {
                        printk ("Could not finalize framebuffer (err %d)\n", err);
                }
                // printk ("refresh\r\n");
        }

private:
        device const *display{};
        Style style_{};
};

/****************************************************************************/

template <Dimension widthV, Dimension heightV> Display<widthV, heightV>::Display (device const *disp) : display{disp}
{
        if (!device_is_ready (display)) {
                printk ("Display device not ready\n");
                return;
        }
}

/****************************************************************************/

template <Dimension widthV, Dimension heightV> auto zephyrCfb () { return Display<widthV, heightV>{}; }

} // namespace og::zephyr::cfb
