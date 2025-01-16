#ifdef HAVE_BAGL

#include <os.h>
#include <ux.h>
#include <glyphs.h>
#include <format.h>
#include <base58.h>

#include "stx_ui.h"
#include "stx_ui_common.h"
#include "stx_response.h"

#include "../../context.h"
#include "../../common/macros_ext.h"
#include "../../helpers/response.h"
#include "../../common/safeint.h"
#include "../../ergo/address.h"
#include "../../ui/ui_application_id.h"
#include "../../ui/ui_approve_reject.h"
#include "../../ui/ui_sign_reject.h"
#include "../../ui/ui_dynamic_flow.h"
#include "../../ui/ui_menu.h"
#include "../../ui/ui_main.h"
#include "../../ui/display.h"

// ----- OPERATION APPROVE / REJECT FLOW

void ui_stx_operation_approve_reject(bool approved, sign_transaction_ui_aprove_ctx_t* ctx) {
    sign_transaction_ctx_t* sign_tx = (sign_transaction_ctx_t*) ctx->sign_tx_context;

    app_set_ui_busy(false);

    if (approved) {
        app_set_connected_app_id(ctx->app_token_value);
        sign_tx->state = SIGN_TRANSACTION_STATE_APPROVED;
        send_response_sign_transaction_session_id(sign_tx->session);
        return;
    } else {
        app_set_current_command(CMD_NONE);
        res_deny();
    }

    ui_menu_main();
}

static NOINLINE void ui_stx_operation_approve_action(bool approved, void* context) {
    sign_transaction_ui_aprove_ctx_t* ctx = (sign_transaction_ui_aprove_ctx_t*) context;
    ui_stx_operation_approve_reject(approved, ctx);
}

bool ui_stx_add_operation_approve_screens(sign_transaction_ui_aprove_ctx_t* ctx,
                                          uint8_t* screen,
                                          uint32_t app_access_token,
                                          bool is_known_application,
                                          sign_transaction_ctx_t* sign_tx) {
    if (MAX_NUMBER_OF_SCREENS - *screen < 3) return false;

    if (!is_known_application && app_access_token != 0) {
        ui_add_screen(ui_application_id_screen(app_access_token, ctx->app_token), screen);
    }
    ctx->app_token_value = app_access_token;
    ctx->sign_tx_context = sign_tx;
    ctx->is_known_application = is_known_application;

    ui_stx_operation_approve_action(true, ctx);

    return true;
}

// --- OUTPUT APPROVE / REJECT FLOW

static NOINLINE bool ui_stx_operation_output_confirm_action(bool approved, void* context) {
    sign_transaction_ui_output_confirm_ctx_t* ctx =
        (sign_transaction_ui_output_confirm_ctx_t*) context;
    app_set_ui_busy(false);

    explicit_bzero(ctx->last_approved_change, sizeof(sign_transaction_bip32_path_t));

    if (approved) {
        // store last approved change address
        if (stx_output_info_type(ctx->output) == SIGN_TRANSACTION_OUTPUT_INFO_TYPE_BIP32) {
            memmove(ctx->last_approved_change,
                    &ctx->output->bip32_path,
                    sizeof(sign_transaction_bip32_path_t));
        }
        // res_ok();
        return true;
    } else {
        app_set_current_command(CMD_NONE);
        // res_deny();
    }

    ui_menu_main();
    return false;
}

uint16_t ui_stx_dynamic_display(uint8_t screen, char* title, char* text) {
    strncpy(title, pair_mem_title[screen], 20);
    strncpy(text, pair_mem_text[screen], 70);
    return SW_OK;
}

bool ui_stx_add_output_screens(sign_transaction_ui_output_confirm_ctx_t* ctx,
                               uint8_t* screen,
                               uint8_t* output_screen,
                               const sign_transaction_output_info_ctx_t* output,
                               sign_transaction_bip32_path_t* last_approved_change,
                               uint8_t network_id) {
    if (MAX_NUMBER_OF_SCREENS - *screen < 6) return false;

    memset(ctx, 0, sizeof(sign_transaction_ui_output_confirm_ctx_t));
    memset(last_approved_change, 0, sizeof(sign_transaction_bip32_path_t));

    uint8_t info_screen_count = 1;  // Address screen
    if (stx_output_info_type(output) != SIGN_TRANSACTION_OUTPUT_INFO_TYPE_BIP32) {
        uint8_t tokens_count = stx_output_info_used_tokens_count(output);
        info_screen_count += 1 + (2 * tokens_count);  // value screen + tokens (2 for each)
    }

    ctx->network_id = network_id;
    ctx->output = output;
    ctx->last_approved_change = last_approved_change;

    if (!ui_stx_operation_output_confirm_action(true, ctx)) {
        res_deny();
        return false;
    }

    int pair_index = *output_screen;

    for (int i = 0; i < info_screen_count; i++) {
        ui_stx_display_output_state(i,
                                    pair_mem_title[pair_index],
                                    pair_mem_text[pair_index],
                                    (void*) ctx);
        pair_index++;
    }

    *output_screen = pair_index;

    if (MAX_NUMBER_OF_SCREENS - *screen < 2) return false;

    explicit_bzero(ctx->last_approved_change, sizeof(sign_transaction_bip32_path_t));

    // store last approved change address
    if (stx_output_info_type(ctx->output) == SIGN_TRANSACTION_OUTPUT_INFO_TYPE_BIP32) {
        memmove(ctx->last_approved_change,
                &ctx->output->bip32_path,
                sizeof(sign_transaction_bip32_path_t));
    }
    res_ok();

    return true;
}

// --- TX ACCEPT / REJECT FLOW

// TX approve/reject callback
static NOINLINE void ui_stx_operation_execute_action(bool approved, void* context) {
    app_set_ui_busy(false);

    sign_transaction_ui_sign_confirm_ctx_t* ctx = (sign_transaction_ui_sign_confirm_ctx_t*) context;
    if (approved) {
        ctx->op_response_cb(ctx->op_cb_context);
    } else {
        res_deny();
    }

    app_set_current_command(CMD_NONE);
    ui_menu_main();
}

bool ui_stx_add_transaction_screens(sign_transaction_ui_sign_confirm_ctx_t* ctx,
                                    uint8_t* screen,
                                    uint8_t* output_screen,
                                    const sign_transaction_amounts_ctx_t* amounts,
                                    uint8_t op_screen_count,
                                    ui_sign_transaction_operation_show_screen_cb screen_cb,
                                    ui_sign_transaction_operation_send_response_cb response_cb,
                                    void* cb_context) {
    if (MAX_NUMBER_OF_SCREENS - *screen < 6) return false;

    memset(ctx, 0, sizeof(sign_transaction_ui_sign_confirm_ctx_t));

    uint8_t tokens_count = stx_amounts_non_zero_tokens_count(amounts);

    ctx->op_screen_count = op_screen_count;
    ctx->op_screen_cb = screen_cb;
    ctx->op_response_cb = response_cb;
    ctx->op_cb_context = cb_context;
    ctx->amounts = amounts;

    int pairs_count = op_screen_count + 1 + (2 * tokens_count);
    int pair_index = *output_screen;

    for (int i = 0; i < pairs_count; i++) {
        ui_stx_display_tx_state(i,
                                pair_mem_title[pair_index],
                                pair_mem_text[pair_index],
                                (void*) ctx);

        pair_index++;
    }

    if (!ui_add_dynamic_flow_screens(screen,
                                     pair_index,
                                     ctx->title,
                                     ctx->text,
                                     &ui_stx_dynamic_display))
        return false;

    if (MAX_NUMBER_OF_SCREENS - *screen < 2) return false;

    ui_sign_reject_screens(ui_stx_operation_execute_action,
                           ctx,
                           ui_next_sreen_ptr(screen),
                           ui_next_sreen_ptr(screen));

    return true;
}

bool ui_stx_display_screens(uint8_t screen_count) {
    return ui_display_screens(&screen_count);
}

#endif