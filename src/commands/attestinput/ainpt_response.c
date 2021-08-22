#include "ainpt_response.h"
#include "ainpt_context.h"
#include "ainpt_sw.h"
#include "../../globals.h"
#include "../../helpers/response.h"
#include "../../helpers/input_frame.h"
#include "../../helpers/io.h"

static inline uint8_t get_frames_count(uint8_t tokens_count) {
    uint8_t frames_count = (tokens_count + (FRAME_MAX_TOKENS_COUNT - 1)) / FRAME_MAX_TOKENS_COUNT;
    return frames_count == 0 ? 1 : frames_count;
}

int send_response_attested_input_frame(attest_input_ctx_t *ctx,
                                       uint8_t session_key[static SESSION_KEY_LEN],
                                       uint8_t index) {
    uint8_t frames_count = get_frames_count(ctx->box.tokens_count);
    if (index >= frames_count) {
        return res_error(SW_ATTEST_UTXO_BAD_FRAME_INDEX);
    }

    // Hack for stack overflow. Writing directly to the IO buffer.
    // Not elegant but works.
    BUFFER_FROM_ARRAY_EMPTY(output, G_io_apdu_buffer, IO_APDU_BUFFER_SIZE - 2);

    if (!buffer_write_bytes(&output, ctx->box_id, BOX_ID_LEN)) {
        return res_error(SW_ATTEST_UTXO_BUFFER_ERROR);
    }
    if (!buffer_write_u8(&output, frames_count)) {
        return res_error(SW_ATTEST_UTXO_BUFFER_ERROR);
    }
    if (!buffer_write_u8(&output, index)) {
        return res_error(SW_ATTEST_UTXO_BUFFER_ERROR);
    }
    if (!buffer_write_u8(&output, ctx->box.tokens_count)) {
        return res_error(SW_ATTEST_UTXO_BUFFER_ERROR);
    }
    if (!buffer_write_u64(&output, ctx->box.ctx.value, BE)) {
        return res_error(SW_ATTEST_UTXO_BUFFER_ERROR);
    }

    uint8_t offset = index * FRAME_MAX_TOKENS_COUNT;
    uint8_t non_empty = 0;
    for (uint8_t i = 0; i < ctx->tokens_table.count; i++) {
        if (ctx->token_amounts[i] > 0) non_empty++;
        if (ctx->token_amounts[i] > 0 && non_empty >= offset) {
            if (!buffer_write_bytes(&output, ctx->tokens_table.tokens[i], TOKEN_ID_LEN)) {
                return res_error(SW_ATTEST_UTXO_BUFFER_ERROR);
            }
            if (!buffer_write_u64(&output, ctx->token_amounts[i], BE)) {
                return res_error(SW_ATTEST_UTXO_BUFFER_ERROR);
            }
            if (non_empty - offset == FRAME_MAX_TOKENS_COUNT - 1) break;
        }
    }
    cx_hmac_sha256_t hmac;
    if (cx_hmac_sha256_init_no_throw(&hmac, session_key, SESSION_KEY_LEN) != 0) {
        return res_error(SW_ATTEST_UTXO_HMAC_ERROR);
    }
    if (cx_hmac_no_throw((cx_hmac_t *) &hmac,
                         CX_LAST,
                         buffer_read_ptr(&output),
                         buffer_data_len(&output),
                         buffer_write_ptr(&output),
                         buffer_empty_space_len(&output)) != 0) {
        return res_error(SW_ATTEST_UTXO_HMAC_ERROR);
    }

    if (!buffer_seek_write_cur(&output, INPUT_FRAME_SIGNATURE_LEN)) {
        return res_error(SW_ATTEST_UTXO_BUFFER_ERROR);
    }

    return res_ok_data(&output);
}

int send_response_attested_input_frame_count(uint8_t tokens_count) {
    uint8_t frames_count = get_frames_count(tokens_count);
    BUFFER_FROM_VAR_FULL(buf, frames_count);
    return res_ok_data(&buf);
}

int send_response_attested_input_session_id(uint8_t session_id) {
    BUFFER_FROM_VAR_FULL(buf, session_id);
    return res_ok_data(&buf);
}
