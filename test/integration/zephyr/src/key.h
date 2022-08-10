/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#pragma once
#include <cstddef>
#include <drivers/gpio.h>
#include <oledgui/zephyrCfb.h>
#include <string_view>
#include <type_traits>
#include <zephyr.h>
#include <zephyr/types.h>
#include <zephyr/zephyr.h>

namespace zephyr {

class Key;

class Keyboard {
public:
        Keyboard ()
        { /* k_timer_init (&debounceTimer, onDebounceElapsed, NULL); */
        }

        // void onPressed (auto const);

        friend class Key;

private:
        // k_timer debounceTimer{};
};

/****************************************************************************/

/* template <auto Code>  */ class Key {
public:
        explicit Key (gpio_dt_spec const &devTr, og::Key val); // TODO val -> template
        ~Key ();

        bool isPressed () const { return gpio_pin_get_dt (&deviceTree); }
        og::Key value () const { return value_; };

private:
        static void onEdge (const struct device *dev, struct gpio_callback *cb, uint32_t pins);
        static void onDebounceElapsed (struct k_timer *timer_id);

        gpio_dt_spec deviceTree;

        struct Envelope {
                gpio_callback buttonCb{};
                Key *that{};
        };

        static_assert (std::is_standard_layout_v<Key::Envelope>);

        Envelope data{{}, this};
        // Keyboard *keyboard{};
        og::Key value_{};
        bool lastPressedStatus{};
        k_timer debounceTimer{};

        enum class State { idle, down, up };
        State state{};
};

} // namespace zephyr

/****************************************************************************/

og::Key getKey ();