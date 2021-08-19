//
//  tx_ser_full.c
//  ErgoTxSerializer
//
//  Created by Yehor Popovych on 18.08.2021.
//

#include "tx_ser_full.h"
#include "../common/varint.h"
#include "../common/int_ops.h"
#include <string.h>

static inline ergo_tx_serializer_full_result_e map_table_result(
    ergo_tx_serializer_table_result_e res) {
    switch (res) {
        case ERGO_TX_SERIALIZER_TABLE_RES_OK:
            return ERGO_TX_SERIALIZER_FULL_RES_OK;
        case ERGO_TX_SERIALIZER_TABLE_RES_MORE_DATA:
            return ERGO_TX_SERIALIZER_FULL_RES_MORE_DATA;
        case ERGO_TX_SERIALIZER_TABLE_RES_ERR_BAD_TOKEN_ID:
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_TOKEN_ID;
        case ERGO_TX_SERIALIZER_TABLE_RES_ERR_TOO_MANY_TOKENS:
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_TOO_MANY_TOKENS;
        case ERGO_TX_SERIALIZER_TABLE_RES_ERR_HASHER:
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_HASHER;
        case ERGO_TX_SERIALIZER_TABLE_RES_ERR_BUFFER:
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_BUFFER;
    }
}

static inline ergo_tx_serializer_full_result_e map_box_result(ergo_tx_serializer_box_result_e res) {
    switch (res) {
        case ERGO_TX_SERIALIZER_BOX_RES_OK:
            return ERGO_TX_SERIALIZER_FULL_RES_OK;
        case ERGO_TX_SERIALIZER_BOX_RES_MORE_DATA:
            return ERGO_TX_SERIALIZER_FULL_RES_MORE_DATA;
        case ERGO_TX_SERIALIZER_BOX_RES_ERR_BAD_TOKEN_INDEX:
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_TOKEN_INDEX;
        case ERGO_TX_SERIALIZER_BOX_RES_ERR_BAD_TOKEN_VALUE:
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_TOKEN_VALUE;
        case ERGO_TX_SERIALIZER_BOX_RES_ERR_TOO_MANY_TOKENS:
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_TOO_MANY_TOKENS;
        case ERGO_TX_SERIALIZER_BOX_RES_ERR_TOO_MANY_REGISTERS:
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_TOO_MANY_REGISTERS;
        case ERGO_TX_SERIALIZER_BOX_RES_ERR_TOO_MUCH_DATA:
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_TOO_MUCH_DATA;
        case ERGO_TX_SERIALIZER_BOX_RES_ERR_HASHER:
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_HASHER;
        case ERGO_TX_SERIALIZER_BOX_RES_ERR_BUFFER:
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_BUFFER;
        case ERGO_TX_SERIALIZER_BOX_RES_ERR_U64_OVERFLOW:
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_U64_OVERFLOW;
        case ERGO_TX_SERIALIZER_BOX_RES_ERR_BAD_STATE:
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
}

static inline bool hash_u16(cx_blake2b_t* hash, uint16_t u16) {
    BUFFER_NEW_LOCAL_EMPTY(buffer, 10);
    if (gve_put_u16(&buffer, u16) != GVE_OK) {
        return false;
    }
    return blake2b_update(hash, buffer_read_ptr(&buffer), buffer_data_len(&buffer));
}

static ergo_tx_serializer_full_result_e add_token_table_and_output_count(
    ergo_tx_serializer_full_context_t* context) {
    ergo_tx_serializer_full_result_e res =
        map_table_result(ergo_tx_serializer_table_hash(&context->table_ctx, &context->hash));
    if (res != ERGO_TX_SERIALIZER_FULL_RES_OK) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return res;
    }
    if (!hash_u16(&context->hash, context->outputs_count)) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_HASHER;
    }
    context->state = ERGO_TX_SERIALIZER_FULL_STATE_OUTPUTS_STARTED;
    return ERGO_TX_SERIALIZER_FULL_RES_OK;
}

ergo_tx_serializer_full_result_e ergo_tx_serializer_full_init(
    ergo_tx_serializer_full_context_t* context,
    uint16_t inputs_count,
    uint16_t data_inputs_count,
    uint16_t outputs_count,
    uint8_t tokens_count) {
    memset(context, 0, sizeof(ergo_tx_serializer_full_context_t));
    if (inputs_count == 0) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_INPUT_COUNT;
    }
    if (outputs_count == 0) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_OUTPUT_COUNT;
    }
    if (!blake2b_256_init(&context->hash)) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_HASHER;
    }
    if (!hash_u16(&context->hash, inputs_count)) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_HASHER;
    }

    context->inputs_count = inputs_count;
    context->data_inputs_count = data_inputs_count;
    context->outputs_count = outputs_count;

    ergo_tx_serializer_full_result_e res = map_table_result(
        ergo_tx_serializer_table_init(&context->table_ctx, tokens_count, &context->token_table));

    if (res != ERGO_TX_SERIALIZER_FULL_RES_OK) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return res;
    }

    context->state = tokens_count == 0 ? ERGO_TX_SERIALIZER_FULL_STATE_INPUTS_STARTED
                                       : ERGO_TX_SERIALIZER_FULL_STATE_TOKENS_STARTED;

    return ERGO_TX_SERIALIZER_FULL_RES_OK;
}

ergo_tx_serializer_full_result_e ergo_tx_serializer_full_add_tokens(
    ergo_tx_serializer_full_context_t* context,
    buffer_t* tokens) {
    if (context->state != ERGO_TX_SERIALIZER_FULL_STATE_TOKENS_STARTED) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
    ergo_tx_serializer_full_result_e res =
        map_table_result(ergo_tx_serializer_table_add(&context->table_ctx, tokens));
    switch (res) {
        case ERGO_TX_SERIALIZER_FULL_RES_OK:
            context->state = ERGO_TX_SERIALIZER_FULL_STATE_INPUTS_STARTED;
            return res;
        case ERGO_TX_SERIALIZER_FULL_RES_MORE_DATA:
            return res;
        default:
            context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
            return res;
    }
}

ergo_tx_serializer_full_result_e ergo_tx_serializer_full_add_input(
    ergo_tx_serializer_full_context_t* context,
    uint8_t box_id[BOX_ID_LEN],
    uint64_t value,
    uint8_t frames_count,
    uint32_t proof_data_size) {
    if (context->state != ERGO_TX_SERIALIZER_FULL_STATE_INPUTS_STARTED) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
    if (context->inputs_count == 0) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_TOO_MANY_INPUTS;
    }
    if (context->input_ctx.frames_count != context->input_ctx.frames_processed) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
    if (proof_data_size == 0) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_PROOF_SIZE;
    }

    memset(&context->input_ctx, 0, sizeof(ergo_tx_serializer_input_context_t));
    memcpy(context->input_ctx.box_id, box_id, BOX_ID_LEN);
    context->input_ctx.frames_count = frames_count;
    context->input_ctx.frames_processed = 0;
    context->input_ctx.proof_data_size = proof_data_size;

    if (!checked_add_u64(context->value, value, &context->value)) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_U64_OVERFLOW;
    }

    return ERGO_TX_SERIALIZER_FULL_RES_OK;
}

ergo_tx_serializer_full_result_e ergo_tx_serializer_full_add_input_tokens(
    ergo_tx_serializer_full_context_t* context,
    uint8_t box_id[BOX_ID_LEN],
    uint8_t frame_index,
    buffer_t* tokens) {
    if (context->state != ERGO_TX_SERIALIZER_FULL_STATE_INPUTS_STARTED) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
    if (memcmp(context->input_ctx.box_id, box_id, BOX_ID_LEN) != 0) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
    if (frame_index >= context->input_ctx.frames_count) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_TOO_MANY_INPUT_FRAMES;
    }
    if (frame_index != context->input_ctx.frames_processed) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
    while (buffer_data_len(tokens) > 0) {
        uint8_t token_id[TOKEN_ID_LEN];
        uint64_t token_value;
        bool token_found = false;
        if (!buffer_read_bytes(tokens, token_id, TOKEN_ID_LEN)) {
            context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_TOKEN_ID;
        }
        if (!buffer_read_u64(tokens, &token_value, BE)) {
            context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_TOKEN_VALUE;
        }
        for (int i = 0; i < context->token_table.count; i++) {
            if (memcmp(token_id, context->token_table.tokens[i].id, TOKEN_ID_LEN) == 0) {
                token_found = true;
                if (!checked_add_u64(context->token_table.tokens[i].amount,
                                     token_value,
                                     &context->token_table.tokens[i].amount)) {
                    context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
                    return ERGO_TX_SERIALIZER_FULL_RES_ERR_U64_OVERFLOW;
                }
                break;
            }
        }
        if (!token_found) {
            context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_TOKEN_ID;
        }
    }
    context->input_ctx.frames_processed++;
    if (context->input_ctx.frames_processed == context->input_ctx.frames_count) {
        if (!blake2b_update(&context->hash, box_id, BOX_ID_LEN)) {
            context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_HASHER;
        }
        return ERGO_TX_SERIALIZER_FULL_RES_OK;
    }
    return ERGO_TX_SERIALIZER_FULL_RES_MORE_DATA;
}

ergo_tx_serializer_full_result_e ergo_tx_serializer_full_add_input_proof(
    ergo_tx_serializer_full_context_t* context,
    buffer_t* proof_chunk) {
    if (context->state != ERGO_TX_SERIALIZER_FULL_STATE_INPUTS_STARTED) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
    if (context->input_ctx.frames_count != context->input_ctx.frames_processed) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
    size_t len = buffer_data_len(proof_chunk);
    if (context->input_ctx.proof_data_size < len) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_TOO_MUCH_DATA;
    }
    if (!blake2b_update(&context->hash, buffer_read_ptr(proof_chunk), len)) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_HASHER;
    }
    context->input_ctx.proof_data_size -= len;

    if (context->input_ctx.proof_data_size == 0) {
        context->inputs_count--;
        if (context->inputs_count == 0) {
            if (!hash_u16(&context->hash, context->data_inputs_count)) {
                context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
                return ERGO_TX_SERIALIZER_FULL_RES_ERR_HASHER;
            }
            if (context->data_inputs_count == 0) {
                return add_token_table_and_output_count(context);
            }
            context->state = ERGO_TX_SERIALIZER_FULL_STATE_DATA_INPUTS_STARTED;
        }
        return ERGO_TX_SERIALIZER_FULL_RES_OK;
    }
    return ERGO_TX_SERIALIZER_FULL_RES_MORE_DATA;
}

ergo_tx_serializer_full_result_e ergo_tx_serializer_full_add_data_inputs(
    ergo_tx_serializer_full_context_t* context,
    buffer_t* inputs) {
    if (context->state != ERGO_TX_SERIALIZER_FULL_STATE_DATA_INPUTS_STARTED) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
    while (buffer_data_len(inputs) > 0) {
        if (context->data_inputs_count == 0) {
            context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_TOO_MANY_DATA_INPUTS;
        }
        uint8_t box_id[BOX_ID_LEN];
        if (!buffer_read_bytes(inputs, box_id, BOX_ID_LEN)) {
            context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_DATA_INPUT;
        }
        if (!blake2b_update(&context->hash, box_id, BOX_ID_LEN)) {
            context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
            return ERGO_TX_SERIALIZER_FULL_RES_ERR_HASHER;
        }
        context->data_inputs_count--;
    }
    if (context->data_inputs_count == 0) {
        return add_token_table_and_output_count(context);
    }
    return ERGO_TX_SERIALIZER_FULL_RES_MORE_DATA;
}

ergo_tx_serializer_full_result_e ergo_tx_serializer_full_add_box(
    ergo_tx_serializer_full_context_t* context,
    uint64_t value,
    uint32_t ergo_tree_size,
    uint32_t creation_height,
    uint8_t tokens_count,
    uint8_t registers_count) {
    if (context->state != ERGO_TX_SERIALIZER_FULL_STATE_OUTPUTS_STARTED) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
    if (context->outputs_count == 0) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_TOO_MANY_OUTPUTS;
    }
    ergo_tx_serializer_full_result_e res =
        map_box_result(ergo_tx_serializer_box_init(&context->box_ctx,
                                                   value,
                                                   ergo_tree_size,
                                                   creation_height,
                                                   tokens_count,
                                                   registers_count,
                                                   false,
                                                   &context->token_table,
                                                   &context->hash));
    if (res != ERGO_TX_SERIALIZER_FULL_RES_OK) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
    }
    return res;
}

ergo_tx_serializer_full_result_e ergo_tx_serializer_full_add_box_ergo_tree(
    ergo_tx_serializer_full_context_t* context,
    buffer_t* tree_chunk) {
    if (context->state != ERGO_TX_SERIALIZER_FULL_STATE_OUTPUTS_STARTED) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
    ergo_tx_serializer_full_result_e res =
        map_box_result(ergo_tx_serializer_box_add_tree(&context->box_ctx, tree_chunk));
    if (res != ERGO_TX_SERIALIZER_FULL_RES_OK && res != ERGO_TX_SERIALIZER_FULL_RES_MORE_DATA) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
    }
    return res;
}

ergo_tx_serializer_full_result_e ergo_tx_serializer_full_add_box_change_tree(
    ergo_tx_serializer_full_context_t* context,
    uint32_t* bip32_path,
    uint8_t bip32_path_len) {
    if (context->state != ERGO_TX_SERIALIZER_FULL_STATE_OUTPUTS_STARTED) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
    ergo_tx_serializer_full_result_e res = map_box_result(
        ergo_tx_serializer_box_add_change_tree(&context->box_ctx, bip32_path, bip32_path_len));
    if (res != ERGO_TX_SERIALIZER_FULL_RES_OK && res != ERGO_TX_SERIALIZER_FULL_RES_MORE_DATA) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
    }
    return res;
}

ergo_tx_serializer_full_result_e ergo_tx_serializer_full_add_box_miners_change_tree(
    ergo_tx_serializer_full_context_t* context) {
    if (context->state != ERGO_TX_SERIALIZER_FULL_STATE_OUTPUTS_STARTED) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
    ergo_tx_serializer_full_result_e res =
        map_box_result(ergo_tx_serializer_box_add_miners_fee_tree(&context->box_ctx));
    if (res != ERGO_TX_SERIALIZER_FULL_RES_OK && res != ERGO_TX_SERIALIZER_FULL_RES_MORE_DATA) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
    }
    return res;
}

ergo_tx_serializer_full_result_e ergo_tx_serializer_full_add_box_tokens(
    ergo_tx_serializer_full_context_t* context,
    buffer_t* tokens) {
    if (context->state != ERGO_TX_SERIALIZER_FULL_STATE_OUTPUTS_STARTED) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
    ergo_tx_serializer_full_result_e res =
        map_box_result(ergo_tx_serializer_box_add_tokens(&context->box_ctx, tokens));
    switch (res) {
        case ERGO_TX_SERIALIZER_FULL_RES_OK:
            if (ergo_tx_serializer_box_is_registers_finished(&context->box_ctx)) {
                context->outputs_count--;
                if (context->outputs_count == 0) {
                    context->state = ERGO_TX_SERIALIZER_FULL_STATE_FINISHED;
                }
            }
            return res;
        case ERGO_TX_SERIALIZER_FULL_RES_MORE_DATA:
            return res;
        default:
            context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
            return res;
    }
}

ergo_tx_serializer_full_result_e ergo_tx_serializer_full_add_box_register(
    ergo_tx_serializer_full_context_t* context,
    buffer_t* value) {
    if (context->state != ERGO_TX_SERIALIZER_FULL_STATE_OUTPUTS_STARTED) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
    ergo_tx_serializer_full_result_e res =
        map_box_result(ergo_tx_serializer_box_add_register(&context->box_ctx, value));
    switch (res) {
        case ERGO_TX_SERIALIZER_FULL_RES_OK:
            context->outputs_count--;
            if (context->outputs_count == 0) {
                context->state = ERGO_TX_SERIALIZER_FULL_STATE_FINISHED;
            }
            return res;
        case ERGO_TX_SERIALIZER_FULL_RES_MORE_DATA:
            return res;
        default:
            context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
            return res;
    }
}

ergo_tx_serializer_full_result_e ergo_tx_serializer_full_hash(
    ergo_tx_serializer_full_context_t* context,
    uint8_t tx_id[static TRANSACTION_HASH_LEN]) {
    if (context->state != ERGO_TX_SERIALIZER_FULL_STATE_FINISHED) {
        context->state = ERGO_TX_SERIALIZER_FULL_STATE_ERROR;
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_BAD_STATE;
    }
    if (!blake2b_256_finalize(&context->hash, tx_id)) {
        return ERGO_TX_SERIALIZER_FULL_RES_ERR_HASHER;
    }
    return ERGO_TX_SERIALIZER_FULL_RES_OK;
}
