//
// Created by awalol on 2026/5/4.
//

#include "cmd.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "bt.h"
#include "button_shortcut.h"
#include "config.h"
#include "device/usbd.h"
#include "pico/time.h"
#include "audio.h"
#include "usb.h"
#include "utils.h"

extern bool spk_active;
extern USBGetStateData interrupt_in_data;

bool is_pico_cmd(uint8_t report_id) {
    if (report_id == 0xf6 ||
        report_id == 0xf7 ||
        report_id == 0xf8 ||
        report_id == 0xf9 ||
        report_id == 0xfa ||
        report_id == 0xfb
    ) {
        return true;
    }
    return false;
}

uint16_t pico_cmd_get(uint8_t report_id, uint8_t *buffer, uint16_t reqlen) {
    if (report_id == 0xf7) {
        printf("[HID] Receive 0xf7 getting config\n");
        if (sizeof(Config_body) > reqlen) {
            printf("[Config] Warning: Config_body overflow\n");
        }
        const auto len = std::min(sizeof(Config_body),static_cast<size_t>(reqlen));
        memcpy(buffer,&get_config(),len);
        return len;
    }
    if (report_id == 0xf8) {
        printf("[HID] Receive 0xf8 getting firmware version\n");
        const auto len = std::min(strlen(PICO_PROGRAM_VERSION_STRING), static_cast<size_t>(reqlen));
        memcpy(buffer, PICO_PROGRAM_VERSION_STRING, len);
        return len;
    }
    if (report_id == 0xf9) {
        // [-128,0]
        int8_t rssi = 0;
        bt_get_signal_strength(&rssi);
        if (reqlen == 0) {
            return 0;
        }
        buffer[0] = rssi;
        // byte 1: real audio gating state, for the config UI to display.
        //   bit7 = valid marker (firmware without this byte leaves it 0)
        //   bit0 = controller mic actually streaming (host opened it AND mic_select != disable)
        //   bit1 = controller speaker actually driven (host opened it AND speaker_select != disable)
#if ENABLE_VERBOSE
        printf("[HID] 0xf9 RSSI=%d raw=0x%02X\n", rssi, buffer[0]);
#endif
        if (reqlen < 2) {
            return 1;
        }
        uint8_t flags = 0x80;
        if (audio_mic_active() && get_config().mic_select != 3) flags |= 0x01;
        if (spk_active && get_config().speaker_select != 3) flags |= 0x02;
        // bit2 = controller connected over BT (MiraLink extension)
        const bool connected = bt_is_connected();
        if (connected) flags |= 0x04;
        buffer[1] = flags;
        if (reqlen < 3) {
            return 2;
        }
        // byte 2 (MiraLink extension): battery. High nibble = PowerState,
        // low nibble = PowerPercent (0..10, x10 %). 0xFF when not connected.
        buffer[2] = connected
                        ? static_cast<uint8_t>((interrupt_in_data.Power << 4) |
                                               (interrupt_in_data.PowerPercent & 0x0F))
                        : 0xFF;
        return 3;
    }
    if (report_id == 0xfa) {
        printf("[HID] Receive 0xfa getting button remaps\n");
        const auto len = std::min(static_cast<size_t>(BUTTON_REMAP_COUNT),
                                  static_cast<size_t>(reqlen));
        memcpy(buffer, get_button().button_remap, len);
        return len;
    }
    if (report_id == 0xfb) {
        printf("[HID] Receive 0xfb getting shortcut slots\n");
        const auto len = std::min(sizeof(Button::shortcuts), static_cast<size_t>(reqlen));
        memcpy(buffer, get_button().shortcuts, len);
        return len;
    }
    return 0;
}

void pico_cmd_set(uint8_t report_id, uint8_t const *buffer, uint16_t bufsize) {
    if (bufsize == 0) {
        return;
    }

    if (report_id == 0xfa) {
        printf("[HID] Receive 0xfa setting button remaps\n");
        set_button_remap(buffer, bufsize);
        return;
    }

    if (report_id == 0xfb) {
        printf("[HID] Receive 0xfb setting shortcut slots\n");
        set_shortcut(buffer, bufsize);
        return;
    }

    // 0x01 update config in variable
    // 0x02 write config to flash
    // 0x03 reconnect tinyusb device;
    if (buffer[0] == 0x01) {
#if ENABLE_VERBOSE
        printf("[CMD] Enter config set func\n");
#endif
        set_config(buffer + 1, bufsize - 1);
    }
    if (buffer[0] == PICO_CMD_SAVE_TO_FLASH) {
        printf("[CMD] Enter config save func\n");
        config_save();
    }
    if (buffer[0] == PICO_CMD_RECONNECT_USB) {
        printf("[CMD] Enter tud reconnect func\n");
        usb_reconnect(usb_keyboard_only);
    }
    if (buffer[0] == PICO_CMD_SHORTCUT_LEARNING) {
        // The config UI starts listening for a controller trigger; pause the
        // shortcut state machine so the captured press does not also fire.
        // Only starting is supported: a window always ends on its deadline.
        if (bufsize >= 2 && buffer[1]) {
            printf("[CMD] Shortcut learning window\n");
            button_shortcut_start_learning();
        }
    }
}
