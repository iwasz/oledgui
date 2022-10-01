/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#pragma once
#include <cstddef>
#include <oledgui/zephyrCfb.h>
#include <string_view>
#include <type_traits>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>
#include <zephyr/zephyr.h>

namespace zephyr {

/**
 * Single key (button) implemented using an ISR.
 * OnPress and OnLongPress callbacks are run in an ISR context!
 */
template <std::invocable OnPress, std::invocable OnLongPress> class Key {
public:
        Key (gpio_dt_spec const &devTr, OnPress onp, OnLongPress olp);
        ~Key ();

        bool isPressed () const { return gpio_pin_get_dt (&deviceTree); }

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
        bool lastPressedStatus{};
        k_timer debounceTimer{};

        enum class State { idle, down };
        State state{};

        OnPress onPress;
        OnLongPress onLongPress;
};

/****************************************************************************/

template <std::invocable OnPress, std::invocable OnLongPress>
Key<OnPress, OnLongPress>::Key (gpio_dt_spec const &devTr, OnPress onp, OnLongPress olp)
    : deviceTree{devTr}, onPress{std::move (onp)}, onLongPress{std::move (olp)}
{
        if (!device_is_ready (deviceTree.port)) {
                printk ("Key not ready");
                return;
        }

        k_timer_init (&debounceTimer, onDebounceElapsed, NULL);

        int ret = gpio_pin_configure_dt (&deviceTree, GPIO_INPUT | deviceTree.dt_flags);

        if (ret != 0) {
                printk ("Error %d: failed to configure %s pin %d\n", ret, deviceTree.port->name, deviceTree.pin);
                return;
        }

        ret = gpio_pin_interrupt_configure_dt (&deviceTree, GPIO_INT_EDGE_BOTH);
        if (ret != 0) {
                printk ("Error %d: failed to configure interrupt on %s pin %d\n", ret, deviceTree.port->name, deviceTree.pin);
                return;
        }

        gpio_init_callback (&data.buttonCb, onEdge, BIT (deviceTree.pin));
        gpio_add_callback (deviceTree.port, &data.buttonCb);
}

/****************************************************************************/

template <std::invocable OnPress, std::invocable OnLongPress> Key<OnPress, OnLongPress>::~Key ()
{
        gpio_remove_callback (deviceTree.port, &data.buttonCb);
}

/****************************************************************************/

template <std::invocable OnPress, std::invocable OnLongPress>
void Key<OnPress, OnLongPress>::onEdge (const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
        Key::Envelope *data = CONTAINER_OF (cb, Key::Envelope, buttonCb);

        if (data && data->that) {
                data->that->lastPressedStatus = data->that->isPressed ();
                k_timer_user_data_set (&data->that->debounceTimer, data->that);
                k_timer_start (&data->that->debounceTimer, K_MSEC (32), K_NO_WAIT);
        }

        // LOG_INF ("onPressed");
}

/****************************************************************************/

template <std::invocable OnPress, std::invocable OnLongPress> void Key<OnPress, OnLongPress>::onDebounceElapsed (struct k_timer *timer_id)
{
        auto *key = reinterpret_cast<Key *> (k_timer_user_data_get (timer_id));

        if (key->isPressed () != key->lastPressedStatus) {
                return;
        }

        // LOG_INF ("id: %d, state: %d", int (key->value ()), int (key->lastPressedStatus));

        if (key->state == State::idle && key->lastPressedStatus) {
                key->state = State::down;
                k_timer_start (&key->debounceTimer, K_MSEC (1000), K_NO_WAIT);
        }
        else if (key->state == State::down && key->lastPressedStatus) {
                key->state = State::idle;
                key->onLongPress ();
        }
        else if (key->state == State::down && !key->lastPressedStatus) {
                key->state = State::idle;
                key->onPress ();
        }
}

} // namespace zephyr
