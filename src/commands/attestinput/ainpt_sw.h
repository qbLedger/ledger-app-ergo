//
//  autxo_errors.h
//  ErgoTxSerializer
//
//  Created by Yehor Popovych on 17.08.2021.
//

#pragma once

#include "../../sw.h"

#define SW_ATTEST_UTXO_PREFIX 0x6900

#define SW_ATTEST_UTXO_BAD_COMMAND           (SW_ATTEST_UTXO_PREFIX + 0x01)
#define SW_ATTEST_UTXO_BAD_P2                (SW_ATTEST_UTXO_PREFIX + 0x02)
#define SW_ATTEST_UTXO_BAD_SESSION           (SW_ATTEST_UTXO_PREFIX + 0x03)
#define SW_ATTEST_UTXO_NOT_ENOUGH_PARAMS     (SW_ATTEST_UTXO_PREFIX + 0x04)
#define SW_ATTEST_UTXO_MORE_DATA_THAN_NEEDED (SW_ATTEST_UTXO_PREFIX + 0x05)
#define SW_ATTEST_UTXO_BAD_FRAME_INDEX       (SW_ATTEST_UTXO_PREFIX + 0x06)
#define SW_ATTEST_UTXO_HASHER_ERROR          (SW_ATTEST_UTXO_PREFIX + 0x07)
#define SW_ATTEST_UTXO_BUFFER_ERROR          (SW_ATTEST_UTXO_PREFIX + 0x08)
#define SW_ATTEST_UTXO_HMAC_ERROR            (SW_ATTEST_UTXO_PREFIX + 0x09)

#define SW_ATTEST_UTXO_TX_ERROR_PREFIX  (SW_ATTEST_UTXO_PREFIX + 0x10)
#define SW_ATTEST_UTXO_BOX_ERROR_PREFIX (SW_ATTEST_UTXO_PREFIX + 0x30)
