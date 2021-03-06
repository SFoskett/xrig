/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2017-2018 XMR-Stak    <https://github.com/fireice-uk>, <https://github.com/psychocrypt>
 * Copyright 2018      Lee Clagett <https://github.com/vtnerd>
 * Copyright 2016-2018 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include "crypto/CryptoNight.h"
#include "crypto/CryptoNight_test.h"
#include "crypto/CryptoNight_x86.h"
#include "net/Job.h"
#include "net/JobResult.h"
#include "Options.h"


void (*cryptonight_hash_ctx)(const void *input, size_t size, void *output, cryptonight_ctx *ctx, int variant) = nullptr;


#define CRYPTONIGHT_HASH(NAME, ITERATIONS, MEM, MASK, SOFT_AES) \
    switch (variant) { \
    case Options::VARIANT_V1: \
        return cryptonight_##NAME##_hash<ITERATIONS, MEM, MASK, SOFT_AES, Options::VARIANT_V1>(input, size, output, ctx); \
    \
    case Options::VARIANT_NONE: \
        return cryptonight_##NAME##_hash<ITERATIONS, MEM, MASK, SOFT_AES, Options::VARIANT_NONE>(input, size, output, ctx); \
    \
    default: \
        break; \
    }


static void cryptonight_av1_aesni(const void *input, size_t size, void *output, struct cryptonight_ctx *ctx, int variant) {
    CRYPTONIGHT_HASH(single, MONERO_ITER, MONERO_MEMORY, MONERO_MASK, false)
}


static void cryptonight_av3_softaes(const void *input, size_t size, void *output, cryptonight_ctx *ctx, int variant) {
    CRYPTONIGHT_HASH(single, MONERO_ITER, MONERO_MEMORY, MONERO_MASK, true)
}


#ifndef XMRIG_NO_AEON
static void cryptonight_lite_av1_aesni(const void *input, size_t size, void *output, cryptonight_ctx *ctx, int variant) {
    CRYPTONIGHT_HASH(single, AEON_ITER, AEON_MEMORY, AEON_MASK, false)
}


static void cryptonight_lite_av3_softaes(const void *input, size_t size, void *output, cryptonight_ctx *ctx, int variant) {
    CRYPTONIGHT_HASH(single, AEON_ITER, AEON_MEMORY, AEON_MASK, true)
}


void (*cryptonight_variations[8])(const void *input, size_t size, void *output, cryptonight_ctx *ctx, int variant) = {
            cryptonight_av1_aesni,
            nullptr,
            cryptonight_av3_softaes,
            nullptr,
            cryptonight_lite_av1_aesni,
            nullptr,
            cryptonight_lite_av3_softaes,
            nullptr
        };
#else
void (*cryptonight_variations[4])(const void *input, size_t size, void *output, cryptonight_ctx *ctx, int variant) = {
            cryptonight_av1_aesni,
            nullptr,
            cryptonight_av3_softaes,
            nullptr
        };
#endif


bool CryptoNight::hash(const Job &job, JobResult &result, cryptonight_ctx *ctx)
{
    cryptonight_hash_ctx(job.blob(), job.size(), result.result, ctx, job.variant());

    return *reinterpret_cast<uint64_t*>(result.result + 24) < job.target();
}


bool CryptoNight::init(int algo, int variant)
{
    if (variant < 1 || variant > 4) {
        return false;
    }

#   ifndef XMRIG_NO_AEON
    const int index = algo == Options::ALGO_CRYPTONIGHT_LITE ? (variant + 3) : (variant - 1);
#   else
    const int index = variant - 1;
#   endif

    cryptonight_hash_ctx = cryptonight_variations[index];

    return selfTest(algo);
}


void CryptoNight::hash(const uint8_t *input, size_t size, uint8_t *output, cryptonight_ctx *ctx, int variant)
{
    cryptonight_hash_ctx(input, size, output, ctx, variant);
}


bool CryptoNight::selfTest(int algo) {
    if (cryptonight_hash_ctx == nullptr) {
        return false;
    }

    char output[32];

    cryptonight_ctx *ctx = static_cast<cryptonight_ctx *>(_mm_malloc(sizeof(cryptonight_ctx), 16));

    cryptonight_hash_ctx(test_input, 76, output, ctx, 0);

#   ifndef XMRIG_NO_AEON
    bool rc = memcmp(output, algo == Options::ALGO_CRYPTONIGHT_LITE ? test_output_v0_lite : test_output_v0, 32) == 0;
#   else
    bool rc = memcmp(output, test_output_v0, 32) == 0;
#   endif

    if (rc) {
        cryptonight_hash_ctx(test_input, 76, output, ctx, 1);

#       ifndef XMRIG_NO_AEON
        rc = memcmp(output, algo == Options::ALGO_CRYPTONIGHT_LITE ? test_output_v1_lite : test_output_v1, 32) == 0;
#       else
        rc = memcmp(output, test_output_v1, 32) == 0;
#       endif
    }

    _mm_free(ctx);

    return rc;
}