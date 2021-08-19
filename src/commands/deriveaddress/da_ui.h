#pragma once

#include <stdint.h>   // uint*
#include <stdbool.h>  // bool
#include "../../constants.h"

// /**
//  * Callback to reuse action with approve/reject in step FLOW.
//  */
// typedef void (*action_validate_cb)(bool);

/**
 * Display account on the device and ask confirmation to export.
 *
 * @return 0 if success, negative integer otherwise.
 *
 */
int ui_display_address(bool send,
                       uint8_t network_id,
                       uint32_t app_access_token,
                       uint32_t *bip32_path,
                       uint8_t bip32_path_len,
                       uint8_t raw_pub_key[static PUBLIC_KEY_LEN]);

/**
 * Action for public key validation and export.
 *
 * @param[in] choice
 *   User choice (either approved or rejected).
 *
 */
void ui_action_derive_address(bool choice);