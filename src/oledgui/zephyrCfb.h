/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "oledgui.h"
#include <type_traits>
#include <zephyr/display/cfb.h>
#include <zephyr/zephyr.h>

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
                cfb_print (display, const_cast<char *> (static_cast<const char *> (str.data ())), cursor ().x () * 7, cursor ().y () * 8);
                // cfb_print (display, const_cast<char *> (static_cast<const char *> (str.data ())), cursor ().x (), cursor ().y ());
        }

        void clear ()
        {
                if (int err = cfb_framebuffer_clear (display, true); err) {
                        printk ("Could not clear framebuffer (err %d)\n", err);
                }

                cursor ().x () = 0;
                cursor ().y () = 0;
        }

        void color (Color c)
        {
                if (c == 2) {
                        // cfb_invert_area (display, cursor ().x (), cursor ().y (), 7, 0);
                }
        }

        void refresh ()
        {
                if (int err = cfb_framebuffer_finalize (display); err) {
                        printk ("Could not finalize framebuffer (err %d)\n", err);
                }
        }

private:
        using Base::child;
        device const *display{};
};

template <Dimension widthV, Dimension heightV, typename Child>
Display<widthV, heightV, Child>::Display (device const *disp, Child c) : Base (c), display{disp}
{
        if (!device_is_ready (display)) {
                printk ("Display device not ready\n");
                return;
        }

        // if (display_set_pixel_format (display, PIXEL_FORMAT_MONO10) != 0) {
        //         printk ("Failed to set required pixel format\n");
        //         return;
        // }

        // if (int err = cfb_framebuffer_init (display); err) {
        //         printk ("Could not initialize framebuffer (err %d)\n", err);
        // }
}

template <Dimension widthV, Dimension heightV> auto zephyrCfb (auto &&child)
{
        return Display<widthV, heightV, std::remove_reference_t<decltype (child)>>{std::forward<decltype (child)> (child)};
}

} // namespace og::zephyr::cfb
