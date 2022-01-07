//-----------------------------------------------------------------------------
// Copyright (C) Proxmark3 contributors. See AUTHORS.md for details.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// See LICENSE.txt for the text of the license.
//-----------------------------------------------------------------------------
//  tests for desfire
//  tests for LRP here: Leakage Resilient Primitive (LRP) Specification, https://www.nxp.com/docs/en/application-note/AN12304.pdf
//-----------------------------------------------------------------------------

#include "desfiretest.h"

#include <unistd.h>
#include <string.h>      // memcpy memset
#include "fileutils.h"

#include "crypto/libpcrypto.h"
#include "mifare/desfirecrypto.h"
#include "mifare/lrpcrypto.h"

static uint8_t CMACData[] = {0x6B, 0xC1, 0xBE, 0xE2, 0x2E, 0x40, 0x9F, 0x96,
                             0xE9, 0x3D, 0x7E, 0x11, 0x73, 0x93, 0x17, 0x2A,
                             0xAE, 0x2D, 0x8A, 0x57, 0x1E, 0x03, 0xAC, 0x9C,
                             0x9E, 0xB7, 0x6F, 0xAC, 0x45, 0xAF, 0x8E, 0x51
                            };


static bool TestCRC16(void) {
    uint8_t data[] = {0x04, 0x44, 0x0F, 0x32, 0x76, 0x31, 0x80, 0x27, 0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    bool res = true;
    size_t len = DesfireSearchCRCPos(data, 16, 0x00, 2);
    res = res && (len == 7);

    len = DesfireSearchCRCPos(data, 7 + 2, 0x00, 2);
    res = res && (len == 7);

    len = DesfireSearchCRCPos(data, 7, 0x00, 2);
    res = res && (len == 0);

    len = DesfireSearchCRCPos(data, 3, 0x00, 2);
    res = res && (len == 0);

    len = DesfireSearchCRCPos(data, 1, 0x00, 2);
    res = res && (len == 0);

    if (res)
        PrintAndLogEx(INFO, "crc16............. " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR, "crc16............. " _RED_("fail"));

    return res;
}

static bool TestCRC32(void) {
    uint8_t data[] = {0x04, 0x44, 0x0F, 0x32, 0x76, 0x31, 0x80, 0x99, 0xCE, 0x1A, 0xD4, 0x00, 0x00, 0x00, 0x00, 0x00};

    bool res = true;
    size_t len = DesfireSearchCRCPos(data, 16, 0x00, 4);
    res = res && (len == 7);

    len = DesfireSearchCRCPos(data, 7 + 4, 0x00, 4);
    res = res && (len == 7);

    len = DesfireSearchCRCPos(data, 5, 0x00, 4);
    res = res && (len == 0);

    len = DesfireSearchCRCPos(data, 4, 0x00, 4);
    res = res && (len == 0);

    len = DesfireSearchCRCPos(data, 2, 0x00, 4);
    res = res && (len == 0);

    if (res)
        PrintAndLogEx(INFO, "crc32............. " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR, "crc32............. " _RED_("fail"));

    return res;
}

// https://www.nxp.com/docs/en/application-note/AN10922.pdf
static bool TestCMACSubkeys(void) {
    bool res = true;

    uint8_t key[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    uint8_t sk1[DESFIRE_MAX_CRYPTO_BLOCK_SIZE] = {0};
    uint8_t sk2[DESFIRE_MAX_CRYPTO_BLOCK_SIZE] = {0};
    DesfireContext_t dctx;
    // AES
    DesfireSetKey(&dctx, 0, T_AES, key);

    DesfireCMACGenerateSubkeys(&dctx, DCOMainKey, sk1, sk2);

    uint8_t sk1test[] = {0xFB, 0xC9, 0xF7, 0x5C, 0x94, 0x13, 0xC0, 0x41, 0xDF, 0xEE, 0x45, 0x2D, 0x3F, 0x07, 0x06, 0xD1};
    uint8_t sk2test[] = {0xF7, 0x93, 0xEE, 0xB9, 0x28, 0x27, 0x80, 0x83, 0xBF, 0xDC, 0x8A, 0x5A, 0x7E, 0x0E, 0x0D, 0x25};

    res = res && (memcmp(sk1, sk1test, sizeof(sk1test)) == 0);
    res = res && (memcmp(sk2, sk2test, sizeof(sk2test)) == 0);

    // 2tdea
    DesfireSetKey(&dctx, 0, T_3DES, key);

    DesfireCMACGenerateSubkeys(&dctx, DCOMainKey, sk1, sk2);

    uint8_t sk1_2tdea[] = {0xF6, 0x12, 0xEB, 0x32, 0xE4, 0x60, 0x35, 0xF3};
    uint8_t sk2_2tdea[] = {0xEC, 0x25, 0xD6, 0x65, 0xC8, 0xC0, 0x6B, 0xFD};

    res = res && (memcmp(sk1, sk1_2tdea, sizeof(sk1_2tdea)) == 0);
    res = res && (memcmp(sk2, sk2_2tdea, sizeof(sk2_2tdea)) == 0);

    // 3tdea
    uint8_t key3[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    DesfireSetKey(&dctx, 0, T_3K3DES, key3);

    DesfireCMACGenerateSubkeys(&dctx, DCOMainKey, sk1, sk2);

    uint8_t sk1_3tdea[] = {0xA3, 0xED, 0x58, 0xF8, 0xE6, 0x94, 0x1B, 0xCA};
    uint8_t sk2_3tdea[] = {0x47, 0xDA, 0xB1, 0xF1, 0xCD, 0x28, 0x37, 0x8F};

    res = res && (memcmp(sk1, sk1_3tdea, sizeof(sk1_3tdea)) == 0);
    res = res && (memcmp(sk2, sk2_3tdea, sizeof(sk2_3tdea)) == 0);

    if (res)
        PrintAndLogEx(INFO, "CMAC subkeys...... " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "CMAC subkeys...... " _RED_("fail"));

    return res;
}

// https://www.nxp.com/docs/en/application-note/AN10922.pdf
// page 8
static bool TestAn10922KDFAES(void) {
    bool res = true;

    uint8_t key[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    DesfireContext_t dctx;
    DesfireSetKey(&dctx, 0, T_AES, key);

    uint8_t kdfInput[] = {0x04, 0x78, 0x2E, 0x21, 0x80, 0x1D, 0x80, 0x30, 0x42, 0xF5, 0x4E, 0x58, 0x50, 0x20, 0x41, 0x62, 0x75};
    MifareKdfAn10922(&dctx, DCOMainKey, kdfInput, sizeof(kdfInput));

    uint8_t dkey[] = {0xA8, 0xDD, 0x63, 0xA3, 0xB8, 0x9D, 0x54, 0xB3, 0x7C, 0xA8, 0x02, 0x47, 0x3F, 0xDA, 0x91, 0x75};
    res = res && (memcmp(dctx.key, dkey, sizeof(dkey)) == 0);

    if (res)
        PrintAndLogEx(INFO, "An10922 AES....... " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "An10922 AES....... " _RED_("fail"));

    return res;
}

static bool TestAn10922KDF2TDEA(void) {
    bool res = true;

    uint8_t key[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    DesfireContext_t dctx;
    DesfireSetKey(&dctx, 0, T_3DES, key);

    uint8_t kdfInput[] = {0x04, 0x78, 0x2E, 0x21, 0x80, 0x1D, 0x80, 0x30, 0x42, 0xF5, 0x4E, 0x58, 0x50, 0x20, 0x41};
    MifareKdfAn10922(&dctx, DCOMainKey, kdfInput, sizeof(kdfInput));

    uint8_t dkey[] = {0x16, 0xF8, 0x59, 0x7C, 0x9E, 0x89, 0x10, 0xC8, 0x6B, 0x96, 0x48, 0xD0, 0x06, 0x10, 0x7D, 0xD7};
    res = res && (memcmp(dctx.key, dkey, sizeof(dkey)) == 0);

    if (res)
        PrintAndLogEx(INFO, "An10922 2TDEA..... " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "An10922 2TDEA..... " _RED_("fail"));

    return res;
}

static bool TestAn10922KDF3TDEA(void) {
    bool res = true;

    uint8_t key[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    DesfireContext_t dctx;
    DesfireSetKey(&dctx, 0, T_3K3DES, key);

    uint8_t kdfInput[] = {0x04, 0x78, 0x2E, 0x21, 0x80, 0x1D, 0x80, 0x30, 0x42, 0xF5, 0x4E, 0x58, 0x50};
    MifareKdfAn10922(&dctx, DCOMainKey, kdfInput, sizeof(kdfInput));

    uint8_t dkey[] = {0x2F, 0x0D, 0xD0, 0x36, 0x75, 0xD3, 0xFB, 0x9A, 0x57, 0x05, 0xAB, 0x0B, 0xDA, 0x91, 0xCA, 0x0B,
                      0x55, 0xB8, 0xE0, 0x7F, 0xCD, 0xBF, 0x10, 0xEC
                     };
    res = res && (memcmp(dctx.key, dkey, sizeof(dkey)) == 0);

    if (res)
        PrintAndLogEx(INFO, "An10922 3TDEA..... " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "An10922 3TDEA..... " _RED_("fail"));

    return res;
}

// https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/TDES_CMAC.pdf
static bool TestCMAC3TDEA(void) {
    bool res = true;

    uint8_t key[DESFIRE_MAX_KEY_SIZE] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                                         0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01,
                                         0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01, 0x23
                                        };
    DesfireContext_t dctx;
    DesfireSetKey(&dctx, 0, T_3K3DES, key);
    memcpy(dctx.sessionKeyMAC, key, DESFIRE_MAX_KEY_SIZE);
    uint8_t cmac[DESFIRE_MAX_KEY_SIZE] = {0};

    uint8_t cmac1[] = {0x7D, 0xB0, 0xD3, 0x7D, 0xF9, 0x36, 0xC5, 0x50};
    memset(dctx.IV, 0, DESFIRE_MAX_KEY_SIZE);
    DesfireCryptoCMAC(&dctx, CMACData, 0, cmac);
    res = res && (memcmp(cmac, cmac1, sizeof(cmac1)) == 0);

    uint8_t cmac2[] = {0x30, 0x23, 0x9C, 0xF1, 0xF5, 0x2E, 0x66, 0x09};
    memset(cmac, 0, sizeof(cmac));
    memset(dctx.IV, 0, DESFIRE_MAX_KEY_SIZE);
    DesfireCryptoCMAC(&dctx, CMACData, 16, cmac);
    res = res && (memcmp(cmac, cmac2, sizeof(cmac1)) == 0);

    uint8_t cmac3[] = {0x6C, 0x9F, 0x3E, 0xE4, 0x92, 0x3F, 0x6B, 0xE2};
    memset(cmac, 0, sizeof(cmac));
    memset(dctx.IV, 0, DESFIRE_MAX_KEY_SIZE);
    DesfireCryptoCMAC(&dctx, CMACData, 20, cmac);
    res = res && (memcmp(cmac, cmac3, sizeof(cmac1)) == 0);

    uint8_t cmac4[] = {0x99, 0x42, 0x9B, 0xD0, 0xBF, 0x79, 0x04, 0xE5};
    memset(cmac, 0, sizeof(cmac));
    memset(dctx.IV, 0, DESFIRE_MAX_KEY_SIZE);
    DesfireCryptoCMAC(&dctx, CMACData, 32, cmac);
    res = res && (memcmp(cmac, cmac4, sizeof(cmac1)) == 0);

    if (res)
        PrintAndLogEx(INFO, "CMAC 3TDEA........ " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR, "CMAC 3TDEA........ " _RED_("fail"));

    return res;
}

// https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Standards-and-Guidelines/documents/examples/TDES_CMAC.pdf
static bool TestCMAC2TDEA(void) {
    bool res = true;

    uint8_t key[DESFIRE_MAX_KEY_SIZE] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                                         0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01
                                        };
    DesfireContext_t dctx;
    DesfireSetKey(&dctx, 0, T_3DES, key);
    memcpy(dctx.sessionKeyMAC, key, DESFIRE_MAX_KEY_SIZE);
    uint8_t cmac[DESFIRE_MAX_KEY_SIZE] = {0};

    uint8_t cmac1[] = {0x79, 0xCE, 0x52, 0xA7, 0xF7, 0x86, 0xA9, 0x60};
    memset(dctx.IV, 0, DESFIRE_MAX_KEY_SIZE);
    DesfireCryptoCMAC(&dctx, CMACData, 0, cmac);
//    PrintAndLogEx(INFO, "cmac: %s", sprint_hex(cmac, 16));
    res = res && (memcmp(cmac, cmac1, sizeof(cmac1)) == 0);

    uint8_t cmac2[] = {0xCC, 0x18, 0xA0, 0xB7, 0x9A, 0xF2, 0x41, 0x3B};
    memset(cmac, 0, sizeof(cmac));
    memset(dctx.IV, 0, DESFIRE_MAX_KEY_SIZE);
    DesfireCryptoCMAC(&dctx, CMACData, 16, cmac);
    res = res && (memcmp(cmac, cmac2, sizeof(cmac1)) == 0);

    uint8_t cmac3[] = {0xC0, 0x6D, 0x37, 0x7E, 0xCD, 0x10, 0x19, 0x69};
    memset(cmac, 0, sizeof(cmac));
    memset(dctx.IV, 0, DESFIRE_MAX_KEY_SIZE);
    DesfireCryptoCMAC(&dctx, CMACData, 20, cmac);
    res = res && (memcmp(cmac, cmac3, sizeof(cmac1)) == 0);

    uint8_t cmac4[] = {0x9C, 0xD3, 0x35, 0x80, 0xF9, 0xB6, 0x4D, 0xFB};
    memset(cmac, 0, sizeof(cmac));
    memset(dctx.IV, 0, DESFIRE_MAX_KEY_SIZE);
    DesfireCryptoCMAC(&dctx, CMACData, 32, cmac);
    res = res && (memcmp(cmac, cmac4, sizeof(cmac1)) == 0);

    if (res)
        PrintAndLogEx(INFO, "CMAC 2TDEA........ " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR, "CMAC 2TDEA........ " _RED_("fail"));

    return res;
}

static bool TestCMACDES(void) {
    bool res = true;

    uint8_t key[DESFIRE_MAX_KEY_SIZE] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    DesfireContext_t dctx;
    DesfireSetKey(&dctx, 0, T_DES, key);
    memcpy(dctx.sessionKeyMAC, key, DESFIRE_MAX_KEY_SIZE);
    uint8_t cmac[DESFIRE_MAX_KEY_SIZE] = {0};

    uint8_t cmac1[] = {0x86, 0xF7, 0x9C, 0x13, 0xFD, 0x30, 0x6E, 0x67};
    memset(dctx.IV, 0, DESFIRE_MAX_KEY_SIZE);
    DesfireCryptoCMAC(&dctx, CMACData, 0, cmac);
    res = res && (memcmp(cmac, cmac1, sizeof(cmac1)) == 0);

    uint8_t cmac2[] = {0xBE, 0xA4, 0x21, 0x22, 0x92, 0x46, 0x2A, 0x85};
    memset(cmac, 0, sizeof(cmac));
    memset(dctx.IV, 0, DESFIRE_MAX_KEY_SIZE);
    DesfireCryptoCMAC(&dctx, CMACData, 16, cmac);
    res = res && (memcmp(cmac, cmac2, sizeof(cmac1)) == 0);

    uint8_t cmac3[] = {0x3E, 0x2F, 0x83, 0x10, 0xC5, 0x69, 0x27, 0x5E};
    memset(cmac, 0, sizeof(cmac));
    memset(dctx.IV, 0, DESFIRE_MAX_KEY_SIZE);
    DesfireCryptoCMAC(&dctx, CMACData, 20, cmac);
    res = res && (memcmp(cmac, cmac3, sizeof(cmac1)) == 0);

    uint8_t cmac4[] = {0x9D, 0x1F, 0xC4, 0xD4, 0xC0, 0x25, 0x91, 0x32};
    memset(cmac, 0, sizeof(cmac));
    memset(dctx.IV, 0, DESFIRE_MAX_KEY_SIZE);
    DesfireCryptoCMAC(&dctx, CMACData, 32, cmac);
    res = res && (memcmp(cmac, cmac4, sizeof(cmac1)) == 0);

    if (res)
        PrintAndLogEx(INFO, "CMAC DES.......... " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR, "CMAC DES.......... " _RED_("fail"));

    return res;
}

// https://www.nxp.com/docs/en/application-note/AN12343.pdf
// page 33-34
static bool TestEV2SessionKeys(void) {
    bool res = true;

    uint8_t key[16] = {0};
    uint8_t rnda[] = {0xB0, 0x4D, 0x07, 0x87, 0xC9, 0x3E, 0xE0, 0xCC, 0x8C, 0xAC, 0xC8, 0xE8, 0x6F, 0x16, 0xC6, 0xFE};
    uint8_t rndb[] = {0xFA, 0x65, 0x9A, 0xD0, 0xDC, 0xA7, 0x38, 0xDD, 0x65, 0xDC, 0x7D, 0xC3, 0x86, 0x12, 0xAD, 0x81};
    uint8_t sessionkeyauth[] = {0x63, 0xDC, 0x07, 0x28, 0x62, 0x89, 0xA7, 0xA6, 0xC0, 0x33, 0x4C, 0xA3, 0x1C, 0x31, 0x4A, 0x04};
    uint8_t sessionkeymac[] = {0x77, 0x4F, 0x26, 0x74, 0x3E, 0xCE, 0x6A, 0xF5, 0x03, 0x3B, 0x6A, 0xE8, 0x52, 0x29, 0x46, 0xF6};

    uint8_t sessionkey[16] = {0};
    DesfireGenSessionKeyEV2(key, rnda, rndb, true, sessionkey);
    res = res && (memcmp(sessionkey, sessionkeyauth, sizeof(sessionkeyauth)) == 0);

    memset(sessionkey, 0, sizeof(sessionkey));
    DesfireGenSessionKeyEV2(key, rnda, rndb, false, sessionkey);
    res = res && (memcmp(sessionkey, sessionkeymac, sizeof(sessionkeymac)) == 0);

    if (res)
        PrintAndLogEx(INFO, "EV2 session keys.. " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "EV2 session keys.. " _RED_("fail"));

    return res;
}

static bool TestEV2IVEncode(void) {
    bool res = true;

    uint8_t key[] = {0x66, 0xA8, 0xCB, 0x93, 0x26, 0x9D, 0xC9, 0xBC, 0x28, 0x85, 0xB7, 0xA9, 0x1B, 0x9C, 0x69, 0x7B};
    uint8_t ti[] = {0xED, 0x56, 0xF6, 0xE6};
    uint8_t ivres[] = {0xDA, 0x0F, 0x64, 0x4A, 0x49, 0x86, 0x27, 0x59, 0x57, 0xCF, 0x1E, 0xC3, 0xAF, 0x4C, 0xCE, 0x53};

    DesfireContext_t ctx = {0};
    ctx.keyType = T_AES;
    memcpy(ctx.sessionKeyEnc, key, 16);
    memcpy(ctx.TI, ti, 4);
    ctx.cmdCntr = 0;

    uint8_t iv[16] = {0};
    DesfireEV2FillIV(&ctx, true, iv);
    res = res && (memcmp(iv, ivres, sizeof(ivres)) == 0);

    uint8_t key2[] = {0x44, 0x5A, 0x86, 0x26, 0xB3, 0x33, 0x84, 0x59, 0x32, 0x12, 0x32, 0xfA, 0xDf, 0x6a, 0xDe, 0x2B};
    uint8_t ti2[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t ivres2[] = {0x17, 0x74, 0x94, 0xFC, 0xC4, 0xF1, 0xDA, 0xB2, 0xAF, 0xBE, 0x8F, 0xAE, 0x20, 0x57, 0xA9, 0xD2};
    memcpy(ctx.sessionKeyEnc, key2, 16);
    memcpy(ctx.TI, ti2, 4);
    ctx.cmdCntr = 5;

    memset(iv, 0, 16);
    DesfireEV2FillIV(&ctx, true, iv);
    res = res && (memcmp(iv, ivres2, sizeof(ivres2)) == 0);

    if (res)
        PrintAndLogEx(INFO, "EV2 IV calc....... " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "EV2 IV calc....... " _RED_("fail"));

    return res;
}

// https://www.nxp.com/docs/en/application-note/AN12343.pdf
// page 54
static bool TestEV2MAC(void) {
    bool res = true;

    uint8_t key[] = {0x93, 0x66, 0xFA, 0x19, 0x5E, 0xB5, 0x66, 0xF5, 0xBD, 0x2B, 0xAD, 0x40, 0x20, 0xB8, 0x30, 0x02};
    uint8_t ti[] = {0xE2, 0xD3, 0xAF, 0x69};
    uint8_t cmd = 0x8D;
    uint8_t cmddata[] = {0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
                         0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22
                        };
    uint8_t macres[] = {0x68, 0xF2, 0xC2, 0x8C, 0x57, 0x5A, 0x16, 0x28};

    // init
    DesfireContext_t ctx = {0};
    ctx.keyType = T_AES;
    memcpy(ctx.sessionKeyMAC, key, 16);
    memcpy(ctx.TI, ti, 4);
    ctx.cmdCntr = 0;

    // tx 1
    uint8_t mac[16] = {0};
    DesfireEV2CalcCMAC(&ctx, cmd, cmddata, sizeof(cmddata), mac);
    res = res && (memcmp(mac, macres, sizeof(macres)) == 0);

    // rx 1
    memset(mac, 0, sizeof(mac));
    uint8_t macres2[] = {0x08, 0x20, 0xF6, 0x88, 0x98, 0xC2, 0xA7, 0xF1};
    uint8_t rc = 0;
    ctx.cmdCntr++;
    DesfireEV2CalcCMAC(&ctx, rc, NULL, 0, mac);
    res = res && (memcmp(mac, macres2, sizeof(macres2)) == 0);

    // tx 2
    memset(mac, 0, sizeof(mac));
    cmd = 0xAD;
    uint8_t cmddata3[] = {0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00};
    uint8_t macres3[] = {0x0D, 0x9B, 0xE1, 0x91, 0xD5, 0x96, 0x08, 0x34};
    DesfireEV2CalcCMAC(&ctx, cmd, cmddata3, sizeof(cmddata3), mac);
    res = res && (memcmp(mac, macres3, sizeof(macres3)) == 0);

    // rx 2
    rc = 0;
    ctx.cmdCntr++;
    uint8_t cmddata4[] = {0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
                          0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                         };
    uint8_t macres4[] = {0xA4, 0x9A, 0x44, 0x22, 0x2D, 0x92, 0x66, 0x66};
    DesfireEV2CalcCMAC(&ctx, rc, cmddata4, sizeof(cmddata4), mac);
    res = res && (memcmp(mac, macres4, sizeof(macres4)) == 0);

    if (res)
        PrintAndLogEx(INFO, "EV2 MAC calc...... " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "EV2 MAC calc...... " _RED_("fail"));

    return res;
}

static bool TestTransSessionKeys(void) {
    bool res = true;

    uint8_t key[] = {0x66, 0xA8, 0xCB, 0x93, 0x26, 0x9D, 0xC9, 0xBC, 0x28, 0x85, 0xB7, 0xA9, 0x1B, 0x9C, 0x69, 0x7B};
    uint8_t uid[] = {0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint32_t trCntr = 8;

    uint8_t sessionkey[16] = {0};
    DesfireGenTransSessionKeyEV2(key, trCntr, uid, true, sessionkey);
    uint8_t keymac[] = {0x7C, 0x1A, 0xD2, 0xD9, 0xC5, 0xC0, 0x81, 0x54, 0xA0, 0xA4, 0x91, 0x4B, 0x40, 0x1A, 0x65, 0x98};
    res = res && (memcmp(sessionkey, keymac, sizeof(keymac)) == 0);

    DesfireGenTransSessionKeyEV2(key, trCntr, uid, false, sessionkey);
    uint8_t keyenc[] = {0x11, 0x9B, 0x90, 0x2A, 0x07, 0xB1, 0x8A, 0x86, 0x5B, 0x8E, 0x1B, 0x00, 0x60, 0x59, 0x47, 0x84};
    res = res && (memcmp(sessionkey, keyenc, sizeof(keyenc)) == 0);

    if (res)
        PrintAndLogEx(INFO, "Trans session key. " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "Trans session key. " _RED_("fail"));

    return res;
}

// https://www.nxp.com/docs/en/application-note/AN12304.pdf
// page 10
static bool TestLRPPlaintexts(void) {
    bool res = true;

    uint8_t key[] = {0x56, 0x78, 0x26, 0xB8, 0xDA, 0x8E, 0x76, 0x84, 0x32, 0xA9, 0x54, 0x8D, 0xBE, 0x4A, 0xA3, 0xA0};
    LRPContext_t ctx = {0};
    LRPSetKey(&ctx, key, 0, false);

    uint8_t pt0[] = {0xAC, 0x20, 0xD3, 0x9F, 0x53, 0x41, 0xFE, 0x98, 0xDF, 0xCA, 0x21, 0xDA, 0x86, 0xBA, 0x79, 0x14};
    res = res && (memcmp(ctx.plaintexts[0], pt0, sizeof(pt0)) == 0);

    uint8_t pt1[] = {0x90, 0x7D, 0xA0, 0x3D, 0x67, 0x24, 0x49, 0x16, 0x69, 0x15, 0xE4, 0x56, 0x3E, 0x08, 0x9D, 0x6D};
    res = res && (memcmp(ctx.plaintexts[1], pt1, sizeof(pt1)) == 0);

    uint8_t pt14[] = {0x37, 0xD7, 0x34, 0xA5, 0x1C, 0x07, 0x6E, 0xB8, 0x03, 0xBD, 0x53, 0x0E, 0x17, 0xEB, 0x87, 0xDC};
    res = res && (memcmp(ctx.plaintexts[14], pt14, sizeof(pt14)) == 0);

    uint8_t pt15[] = {0x71, 0xB4, 0x44, 0xAF, 0x25, 0x7A, 0x93, 0x21, 0x53, 0x11, 0xD7, 0x58, 0xDD, 0x33, 0x32, 0x47};
    res = res && (memcmp(ctx.plaintexts[15], pt15, sizeof(pt15)) == 0);

    if (res)
        PrintAndLogEx(INFO, "LRP plaintexts.... " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "LRP plaintexts.... " _RED_("fail"));

    return res;
}

// https://www.nxp.com/docs/en/application-note/AN12304.pdf
// page 12
static bool TestLRPUpdatedKeys(void) {
    bool res = true;

    uint8_t key[] = {0x56, 0x78, 0x26, 0xB8, 0xDA, 0x8E, 0x76, 0x84, 0x32, 0xA9, 0x54, 0x8D, 0xBE, 0x4A, 0xA3, 0xA0};
    LRPContext_t ctx = {0};
    LRPSetKey(&ctx, key, 0, false);

    uint8_t key0[] = {0x16, 0x3D, 0x14, 0xED, 0x24, 0xED, 0x93, 0x53, 0x73, 0x56, 0x8E, 0xC5, 0x21, 0xE9, 0x6C, 0xF4};
    res = res && (memcmp(ctx.updatedKeys[0], key0, sizeof(key0)) == 0);

    uint8_t key1[] = {0x1C, 0x51, 0x9C, 0x00, 0x02, 0x08, 0xB9, 0x5A, 0x39, 0xA6, 0x5D, 0xB0, 0x58, 0x32, 0x71, 0x88};
    res = res && (memcmp(ctx.updatedKeys[1], key1, sizeof(key1)) == 0);

    uint8_t key2[] = {0xFE, 0x30, 0xAB, 0x50, 0x46, 0x7E, 0x61, 0x78, 0x3B, 0xFE, 0x6B, 0x5E, 0x05, 0x60, 0x16, 0x0E};
    res = res && (memcmp(ctx.updatedKeys[2], key2, sizeof(key2)) == 0);

    if (res)
        PrintAndLogEx(INFO, "LRP updated keys.. " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "LRP updated keys.. " _RED_("fail"));

    return res;
}

// https://www.nxp.com/docs/en/application-note/AN12304.pdf
// 3.2 LRP Eval, page 13
static bool TestLRPEval(void) {
    bool res = true;

    LRPContext_t ctx = {0};

    uint8_t y[CRYPTO_AES128_KEY_SIZE] = {0};
    uint8_t key[] = {0x56, 0x78, 0x26, 0xB8, 0xDA, 0x8E, 0x76, 0x84, 0x32, 0xA9, 0x54, 0x8D, 0xBE, 0x4A, 0xA3, 0xA0};
    uint8_t iv[] = {0x13, 0x59};
    LRPSetKey(&ctx, key, 2, false);
    LRPEvalLRP(&ctx, iv, sizeof(iv) * 2, true, y);

    uint8_t y1[] = {0x1B, 0xA2, 0xC0, 0xC5, 0x78, 0x99, 0x6B, 0xC4, 0x97, 0xDD, 0x18, 0x1C, 0x68, 0x85, 0xA9, 0xDD};
    res = res && (memcmp(y, y1, sizeof(y1)) == 0);

    uint8_t key2[] = {0xB6, 0x55, 0x57, 0xCE, 0x0E, 0x9B, 0x4C, 0x58, 0x86, 0xF2, 0x32, 0x20, 0x01, 0x13, 0x56, 0x2B};
    uint8_t iv2[] = {0xBB, 0x4F, 0xCF, 0x27, 0xC9, 0x40, 0x76, 0xF7, 0x56, 0xAB, 0x03, 0x0D};
    LRPSetKey(&ctx, key2, 1, false);
    LRPEvalLRP(&ctx, iv2, sizeof(iv2) * 2, false, y);

    uint8_t y2[] = {0x6F, 0xDF, 0xA8, 0xD2, 0xA6, 0xAA, 0x84, 0x76, 0xBF, 0x94, 0xE7, 0x1F, 0x25, 0x63, 0x7F, 0x96};
    res = res && (memcmp(y, y2, sizeof(y2)) == 0);

    uint8_t key3[] = {0xC4, 0x8A, 0x8E, 0x8B, 0x16, 0x57, 0x16, 0x45, 0xA1, 0x55, 0x78, 0x25, 0xAA, 0x66, 0xAC, 0x91};
    uint8_t iv3[] = {0x1F, 0x0B, 0x7C, 0x0D, 0xB1, 0x28, 0x89, 0xCA, 0x43, 0x6C, 0xAB, 0xB7, 0x8B, 0xE4, 0x2F, 0x90};
    LRPSetKey(&ctx, key3, 3, false);
    LRPEvalLRP(&ctx, iv3, sizeof(iv3) * 2 - 1, true, y);

    uint8_t y3[] = {0x51, 0x29, 0x6B, 0x5E, 0x6D, 0x3B, 0x8D, 0xB8, 0xA1, 0xA7, 0x39, 0x97, 0x60, 0xA1, 0x91, 0x89};
    res = res && (memcmp(y, y3, sizeof(y3)) == 0);

    uint8_t key4[] = {0x54, 0x9C, 0x67, 0xEC, 0xD6, 0x0E, 0x84, 0x8F, 0x77, 0x39, 0x90, 0x99, 0x0C, 0xAC, 0x68, 0x1E};
    uint8_t iv4[] = {0x47, 0x5B, 0xB4, 0x18, 0x78, 0xEB, 0x17, 0x46, 0x8F, 0x7A, 0x68, 0x84, 0x7D, 0xDD, 0x3B, 0xAC};
    LRPSetKey(&ctx, key4, 3, false);
    LRPEvalLRP(&ctx, iv4, sizeof(iv4) * 2, true, y);

    uint8_t y4[] = {0xC3, 0xB5, 0xEE, 0x74, 0xA7, 0x22, 0xE7, 0x84, 0x88, 0x7C, 0x4C, 0x9F, 0xDB, 0x49, 0x78, 0x55};
    res = res && (memcmp(y, y4, sizeof(y4)) == 0);

    uint8_t key5[] = {0x80, 0x6A, 0x50, 0x53, 0x0D, 0x77, 0x35, 0xB4, 0x0A, 0xC4, 0xEF, 0x16, 0x38, 0xE8, 0xAD, 0x6A};
    uint8_t iv5[] = {0xD4, 0x13, 0x77, 0x64, 0x71, 0x6D, 0xBC, 0x8C, 0x57, 0x9B, 0xEA, 0xB7, 0xE7, 0x67, 0x54, 0xE0};
    LRPSetKey(&ctx, key5, 3, false);
    LRPEvalLRP(&ctx, iv5, sizeof(iv5) * 2 - 1, false, y);

    uint8_t y5[] = {0xCF, 0x99, 0x13, 0x92, 0xF0, 0x36, 0x93, 0x50, 0xA7, 0xE2, 0x1B, 0xE5, 0x2F, 0x74, 0x88, 0x21};
    res = res && (memcmp(y, y5, sizeof(y5)) == 0);

    if (res)
        PrintAndLogEx(INFO, "LRP eval.......... " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "LRP eval.......... " _RED_("fail"));

    return res;
}

static bool TestLRPIncCounter(void) {
    bool res = true;

    uint8_t ctr1[] = {0x00, 0x01};
    LRPIncCounter(ctr1, 4);
    uint8_t ctrr1[] = {0x00, 0x02};
    res = res && (memcmp(ctr1, ctrr1, sizeof(ctrr1)) == 0);

    uint8_t ctr2[] = {0x00, 0xf0};
    LRPIncCounter(ctr2, 3);
    uint8_t ctrr2[] = {0x01, 0x00};
    res = res && (memcmp(ctr2, ctrr2, sizeof(ctrr2)) == 0);

    uint8_t ctr3[] = {0xff, 0xf0};
    LRPIncCounter(ctr3, 3);
    uint8_t ctrr3[] = {0x00, 0x00};
    res = res && (memcmp(ctr3, ctrr3, sizeof(ctrr3)) == 0);

    uint8_t ctr4[] = {0xf0};
    LRPIncCounter(ctr4, 1);
    uint8_t ctrr4[] = {0x00};
    res = res && (memcmp(ctr4, ctrr4, sizeof(ctrr4)) == 0);

    if (res)
        PrintAndLogEx(INFO, "LRP inc counter... " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "LRP inc counter... " _RED_("fail"));

    return res;
}

static bool TestLRPEncode(void) {
    bool res = true;

    uint8_t resp[100] = {0};
    size_t resplen = 0;

    LRPContext_t ctx = {0};

    uint8_t key1[] = {0xE0, 0xC4, 0x93, 0x5F, 0xF0, 0xC2, 0x54, 0xCD, 0x2C, 0xEF, 0x8F, 0xDD, 0xC3, 0x24, 0x60, 0xCF};
    uint8_t iv1[] = {0xC3, 0x31, 0x5D, 0xBF};
    LRPSetKeyEx(&ctx, key1, iv1, sizeof(iv1) * 2, 0, true);
    uint8_t data1[] = {0x01, 0x2D, 0x7F, 0x16, 0x53, 0xCA, 0xF6, 0x50, 0x3C, 0x6A, 0xB0, 0xC1, 0x01, 0x0E, 0x8C, 0xB0};
    LRPEncode(&ctx, data1, sizeof(data1), resp, &resplen);
    uint8_t res1[] = {0xFC, 0xBB, 0xAC, 0xAA, 0x4F, 0x29, 0x18, 0x24, 0x64, 0xF9, 0x9D, 0xE4, 0x10, 0x85, 0x26, 0x6F,
                      0x48, 0x0E, 0x86, 0x3E, 0x48, 0x7B, 0xAA, 0xF6, 0x87, 0xB4, 0x3E, 0xD1, 0xEC, 0xE0, 0xD6, 0x23
                     };
    res = res && (resplen == sizeof(res1));
    res = res && (memcmp(resp, res1, sizeof(res1)) == 0);

    uint8_t key2[] = {0xEF, 0xA5, 0xB7, 0x42, 0x9C, 0xD1, 0x53, 0xBF, 0x00, 0x86, 0xDE, 0xF9, 0x00, 0xC0, 0xF2, 0x35};
    uint8_t iv2[] = {0x90, 0x36, 0xFF, 0xFF};
    LRPSetKeyEx(&ctx, key2, iv2, sizeof(iv2) * 2, 0, false);
    uint8_t data2[] = {0xE7, 0xF6, 0x1E, 0x01, 0x2F, 0x4F, 0x32, 0x55, 0x31, 0x2B, 0xA6, 0x8B, 0x1D, 0x2F, 0xDA, 0xBF};
    LRPEncode(&ctx, data2, sizeof(data2), resp, &resplen);
    uint8_t res2[] = {0xEA, 0x6E, 0x09, 0xAC, 0x2F, 0xB9, 0x7E, 0x10, 0x2D, 0x8C, 0xA6, 0x4C, 0x1C, 0xBC, 0x0C, 0x0C};
    res = res && (resplen == sizeof(res2));
    res = res && (memcmp(resp, res2, sizeof(res2)) == 0);

    uint8_t key3[] = {0x9D, 0x81, 0x31, 0x34, 0xCF, 0xDE, 0xE9, 0xD5, 0x87, 0x55, 0xDE, 0xAC, 0xD4, 0xAF, 0x72, 0xA7};
    uint8_t iv3[] = {0xFF, 0xFF, 0xFF, 0xFF};
    LRPSetKeyEx(&ctx, key3, iv3, sizeof(iv3) * 2, 0, true);
    uint8_t data3[] = {0x27};
    LRPEncode(&ctx, data3, sizeof(data3), resp, &resplen);
    uint8_t res3[] = {0xF5, 0x83, 0x3F, 0xC3, 0x97, 0x35, 0x6E, 0xA3, 0xD9, 0xEC, 0xAD, 0xBB, 0x9F, 0x6F, 0xE4, 0x40};
    res = res && (resplen == sizeof(res3));
    res = res && (memcmp(resp, res3, sizeof(res3)) == 0);

    uint8_t key4[] = {0xF5, 0xC3, 0xE9, 0x9F, 0xB7, 0x5E, 0x31, 0x6B, 0x76, 0x68, 0x9F, 0xC5, 0x46, 0x42, 0x60, 0xCD};
    uint8_t iv4[] = {0x07, 0x97, 0xF6, 0xB7};
    LRPSetKeyEx(&ctx, key4, iv4, sizeof(iv4) * 2, 0, true);
    LRPEncode(&ctx, NULL, 0, resp, &resplen);
    uint8_t res4[] = {0x93, 0xDC, 0x3E, 0xE1, 0x4B, 0x61, 0x2B, 0xE6, 0xA3, 0xE9, 0xE2, 0xE8, 0x04, 0x0C, 0xDF, 0xCB};
    res = res && (resplen == sizeof(res4));
    res = res && (memcmp(resp, res4, sizeof(res4)) == 0);

    uint8_t key5[] = {0x9B, 0x1E, 0x41, 0x8D, 0xF9, 0x75, 0x2F, 0x37, 0xEB, 0xBD, 0x8E, 0xE8, 0x33, 0xBD, 0xF2, 0xD7};
    uint8_t iv5[] = {0x24, 0xFF, 0xFF, 0xFF};
    LRPSetKeyEx(&ctx, key5, iv5, sizeof(iv5) * 2, 0, true);
    uint8_t data5[] = {0x55, 0x53, 0x4E, 0x15, 0x9F, 0x14, 0xDD, 0x77, 0x31, 0x36, 0x89, 0x88, 0xEE, 0x6D, 0xD7, 0xC6,
                       0x11, 0x4E, 0x74, 0x7F, 0x9C, 0x17, 0xA9, 0x1B, 0xBC, 0x12, 0xD6, 0x8C, 0x26, 0x53, 0x1F, 0x2F,
                       0xFC, 0xFC
                      };
    LRPEncode(&ctx, data5, sizeof(data5), resp, &resplen);
    uint8_t res5[] = {0x15, 0x8B, 0x3B, 0x9C, 0x61, 0x36, 0xFB, 0x71, 0x5C, 0xCF, 0x43, 0x5C, 0xA4, 0xCA, 0xDE, 0x80,
                      0x8D, 0x1F, 0x98, 0x43, 0x13, 0x27, 0x06, 0x1A, 0x9A, 0x64, 0xD5, 0x2A, 0x5F, 0xE7, 0xB2, 0x74,
                      0x6D, 0x7F, 0x5A, 0x63, 0x3F, 0xC0, 0xCF, 0xE7, 0x85, 0x56, 0x56, 0xAD, 0x3C, 0x6B, 0x94, 0xCF
                     };
    res = res && (resplen == sizeof(res5));
    res = res && (memcmp(resp, res5, sizeof(res5)) == 0);

    if (res)
        PrintAndLogEx(INFO, "LRP encode........ " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "LRP encode........ " _RED_("fail"));

    return res;
}

static bool TestLRPDecode(void) {
    bool res = true;

    uint8_t resp[100] = {0};
    size_t resplen = 0;

    LRPContext_t ctx = {0};

    uint8_t key1[] = {0xE0, 0xC4, 0x93, 0x5F, 0xF0, 0xC2, 0x54, 0xCD, 0x2C, 0xEF, 0x8F, 0xDD, 0xC3, 0x24, 0x60, 0xCF};
    uint8_t iv1[] = {0xC3, 0x31, 0x5D, 0xBF};
    LRPSetKeyEx(&ctx, key1, iv1, sizeof(iv1) * 2, 0, true);
    uint8_t data1[] = {0xFC, 0xBB, 0xAC, 0xAA, 0x4F, 0x29, 0x18, 0x24, 0x64, 0xF9, 0x9D, 0xE4, 0x10, 0x85, 0x26, 0x6F,
                       0x48, 0x0E, 0x86, 0x3E, 0x48, 0x7B, 0xAA, 0xF6, 0x87, 0xB4, 0x3E, 0xD1, 0xEC, 0xE0, 0xD6, 0x23
                      };
    LRPDecode(&ctx, data1, sizeof(data1), resp, &resplen);
    uint8_t res1[] = {0x01, 0x2D, 0x7F, 0x16, 0x53, 0xCA, 0xF6, 0x50, 0x3C, 0x6A, 0xB0, 0xC1, 0x01, 0x0E, 0x8C, 0xB0};
    res = res && (resplen == sizeof(res1));
    res = res && (memcmp(resp, res1, sizeof(res1)) == 0);

    uint8_t key2[] = {0xEF, 0xA5, 0xB7, 0x42, 0x9C, 0xD1, 0x53, 0xBF, 0x00, 0x86, 0xDE, 0xF9, 0x00, 0xC0, 0xF2, 0x35};
    uint8_t iv2[] = {0x90, 0x36, 0xFF, 0xFF};
    LRPSetKeyEx(&ctx, key2, iv2, sizeof(iv2) * 2, 0, false);
    uint8_t data2[] = {0xEA, 0x6E, 0x09, 0xAC, 0x2F, 0xB9, 0x7E, 0x10, 0x2D, 0x8C, 0xA6, 0x4C, 0x1C, 0xBC, 0x0C, 0x0C};
    LRPDecode(&ctx, data2, sizeof(data2), resp, &resplen);
    uint8_t res2[] = {0xE7, 0xF6, 0x1E, 0x01, 0x2F, 0x4F, 0x32, 0x55, 0x31, 0x2B, 0xA6, 0x8B, 0x1D, 0x2F, 0xDA, 0xBF};
    res = res && (resplen == sizeof(res2));
    res = res && (memcmp(resp, res2, sizeof(res2)) == 0);

    uint8_t key3[] = {0x9D, 0x81, 0x31, 0x34, 0xCF, 0xDE, 0xE9, 0xD5, 0x87, 0x55, 0xDE, 0xAC, 0xD4, 0xAF, 0x72, 0xA7};
    uint8_t iv3[] = {0xFF, 0xFF, 0xFF, 0xFF};
    LRPSetKeyEx(&ctx, key3, iv3, sizeof(iv3) * 2, 0, true);
    uint8_t data3[] = {0xF5, 0x83, 0x3F, 0xC3, 0x97, 0x35, 0x6E, 0xA3, 0xD9, 0xEC, 0xAD, 0xBB, 0x9F, 0x6F, 0xE4, 0x40};
    LRPDecode(&ctx, data3, sizeof(data3), resp, &resplen);
    uint8_t res3[] = {0x27};
    res = res && (resplen == sizeof(res3));
    res = res && (memcmp(resp, res3, sizeof(res3)) == 0);

    uint8_t key4[] = {0xF5, 0xC3, 0xE9, 0x9F, 0xB7, 0x5E, 0x31, 0x6B, 0x76, 0x68, 0x9F, 0xC5, 0x46, 0x42, 0x60, 0xCD};
    uint8_t iv4[] = {0x07, 0x97, 0xF6, 0xB7};
    LRPSetKeyEx(&ctx, key4, iv4, sizeof(iv4) * 2, 0, true);
    uint8_t data4[] = {0x93, 0xDC, 0x3E, 0xE1, 0x4B, 0x61, 0x2B, 0xE6, 0xA3, 0xE9, 0xE2, 0xE8, 0x04, 0x0C, 0xDF, 0xCB};
    LRPDecode(&ctx, data4, sizeof(data4), resp, &resplen);
    res = res && (resplen == 0);

    uint8_t key5[] = {0x9B, 0x1E, 0x41, 0x8D, 0xF9, 0x75, 0x2F, 0x37, 0xEB, 0xBD, 0x8E, 0xE8, 0x33, 0xBD, 0xF2, 0xD7};
    uint8_t iv5[] = {0x24, 0xFF, 0xFF, 0xFF};
    LRPSetKeyEx(&ctx, key5, iv5, sizeof(iv5) * 2, 0, true);
    uint8_t data5[] = {0x15, 0x8B, 0x3B, 0x9C, 0x61, 0x36, 0xFB, 0x71, 0x5C, 0xCF, 0x43, 0x5C, 0xA4, 0xCA, 0xDE, 0x80,
                       0x8D, 0x1F, 0x98, 0x43, 0x13, 0x27, 0x06, 0x1A, 0x9A, 0x64, 0xD5, 0x2A, 0x5F, 0xE7, 0xB2, 0x74,
                       0x6D, 0x7F, 0x5A, 0x63, 0x3F, 0xC0, 0xCF, 0xE7, 0x85, 0x56, 0x56, 0xAD, 0x3C, 0x6B, 0x94, 0xCF
                      };
    LRPDecode(&ctx, data5, sizeof(data5), resp, &resplen);
    uint8_t res5[] = {0x55, 0x53, 0x4E, 0x15, 0x9F, 0x14, 0xDD, 0x77, 0x31, 0x36, 0x89, 0x88, 0xEE, 0x6D, 0xD7, 0xC6,
                      0x11, 0x4E, 0x74, 0x7F, 0x9C, 0x17, 0xA9, 0x1B, 0xBC, 0x12, 0xD6, 0x8C, 0x26, 0x53, 0x1F, 0x2F,
                      0xFC, 0xFC
                     };
    res = res && (resplen == sizeof(res5));
    res = res && (memcmp(resp, res5, sizeof(res5)) == 0);

    if (res)
        PrintAndLogEx(INFO, "LRP decode........ " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "LRP decode........ " _RED_("fail"));

    return res;
}

// https://www.nxp.com/docs/en/application-note/AN12304.pdf
// 3.4 LRP CMAC
static bool TestLRPSubkeys(void) {
    bool res = true;

    uint8_t sk1[CRYPTO_AES128_KEY_SIZE] = {0};
    uint8_t sk2[CRYPTO_AES128_KEY_SIZE] = {0};

    uint8_t key1[] = {0x81, 0x95, 0x08, 0x8C, 0xE6, 0xC3, 0x93, 0x70, 0x8E, 0xBB, 0xE6, 0xC7, 0x91, 0x4E, 0xCB, 0x0B};
    uint8_t sk1r1[] = {0x16, 0x91, 0x2B, 0x8D, 0x19, 0xD9, 0x4B, 0x2D, 0x4D, 0xA4, 0xFF, 0xA1, 0xCA, 0xD2, 0x18, 0x23};
    uint8_t sk2r1[] = {0x2D, 0x22, 0x57, 0x1A, 0x33, 0xB2, 0x96, 0x5A, 0x9B, 0x49, 0xFF, 0x43, 0x95, 0xA4, 0x30, 0x46};

    LRPGenSubkeys(key1, sk1, sk2);
    res = res && (memcmp(sk1, sk1r1, sizeof(sk1r1)) == 0);
    res = res && (memcmp(sk2, sk2r1, sizeof(sk2r1)) == 0);

    uint8_t key2[] = {0x11, 0xED, 0x02, 0x02, 0x25, 0x70, 0xCB, 0x10, 0x50, 0x2B, 0xC1, 0xDA, 0xCF, 0x64, 0xB2, 0x1F};
    uint8_t sk1r2[] = {0x5B, 0x5D, 0x85, 0x36, 0x61, 0xE5, 0x1B, 0xC9, 0x13, 0x77, 0xED, 0xCE, 0xB6, 0x22, 0xBF, 0x6E};
    uint8_t sk2r2[] = {0xB6, 0xBB, 0x0A, 0x6C, 0xC3, 0xCA, 0x37, 0x92, 0x26, 0xEF, 0xDB, 0x9D, 0x6C, 0x45, 0x7E, 0xDC};

    LRPGenSubkeys(key2, sk1, sk2);
    res = res && (memcmp(sk1, sk1r2, sizeof(sk1r2)) == 0);
    res = res && (memcmp(sk2, sk2r2, sizeof(sk2r2)) == 0);

    uint8_t key3[] = {0x5A, 0xA9, 0xF6, 0xC6, 0xDE, 0x51, 0x38, 0x11, 0x3D, 0xF5, 0xD6, 0xB6, 0xC7, 0x7D, 0x5D, 0x52};
    uint8_t sk1r3[] = {0x2A, 0xE0, 0xEB, 0xD3, 0x76, 0xBC, 0xD4, 0xA2, 0x7B, 0x1C, 0xD4, 0x06, 0xD2, 0x43, 0x1C, 0xF9};
    uint8_t sk2r3[] = {0x55, 0xC1, 0xD7, 0xA6, 0xED, 0x79, 0xA9, 0x44, 0xF6, 0x39, 0xA8, 0x0D, 0xA4, 0x86, 0x39, 0xF2};

    LRPGenSubkeys(key3, sk1, sk2);
    res = res && (memcmp(sk1, sk1r3, sizeof(sk1r3)) == 0);
    res = res && (memcmp(sk2, sk2r3, sizeof(sk2r3)) == 0);

    if (res)
        PrintAndLogEx(INFO, "LRP subkeys....... " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "LRP subkeys....... " _RED_("fail"));

    return res;
}

// https://www.nxp.com/docs/en/application-note/AN12304.pdf
// 3.4 LRP CMAC
static bool TestLRPCMAC(void) {
    bool res = true;

    LRPContext_t ctx = {0};
    uint8_t cmac[CRYPTO_AES128_KEY_SIZE] = {0};

    uint8_t key1[] = {0x81, 0x95, 0x08, 0x8C, 0xE6, 0xC3, 0x93, 0x70, 0x8E, 0xBB, 0xE6, 0xC7, 0x91, 0x4E, 0xCB, 0x0B};
    LRPSetKey(&ctx, key1, 0, true);
    uint8_t data1[] = {0xBB, 0xD5, 0xB8, 0x57, 0x72, 0xC7};
    LRPCMAC(&ctx, data1, sizeof(data1), cmac);
    uint8_t cmacres1[] = {0xAD, 0x85, 0x95, 0xE0, 0xB4, 0x9C, 0x5C, 0x0D, 0xB1, 0x8E, 0x77, 0x35, 0x5F, 0x5A, 0xAF, 0xF6};
    res = res && (memcmp(cmac, cmacres1, sizeof(cmacres1)) == 0);

    uint8_t key2[] = {0x5A, 0xA9, 0xF6, 0xC6, 0xDE, 0x51, 0x38, 0x11, 0x3D, 0xF5, 0xD6, 0xB6, 0xC7, 0x7D, 0x5D, 0x52};
    LRPSetKey(&ctx, key2, 0, true);
    uint8_t data2[] = {0xA4, 0x43, 0x4D, 0x74, 0x0C, 0x2C, 0xB6, 0x65, 0xFE, 0x53, 0x96, 0x95, 0x91, 0x89, 0x38, 0x3F};
    LRPCMAC(&ctx, data2, sizeof(data2), cmac);
    uint8_t cmacres2[] = {0x8B, 0x43, 0xAD, 0xF7, 0x67, 0xE4, 0x6B, 0x69, 0x2E, 0x8F, 0x24, 0xE8, 0x37, 0xCB, 0x5E, 0xFC};
    res = res && (memcmp(cmac, cmacres2, sizeof(cmacres2)) == 0);

    uint8_t key3[] = {0x0D, 0x46, 0x55, 0x75, 0x50, 0xCB, 0x31, 0x3F, 0x36, 0xAF, 0xBA, 0x87, 0x62, 0x5D, 0x96, 0x1A};
    LRPSetKey(&ctx, key3, 0, true);
    uint8_t data3[] = {0x90};
    LRPCMAC(&ctx, data3, sizeof(data3), cmac);
    uint8_t cmacres3[] = {0xF7, 0xC8, 0x55, 0x3D, 0xED, 0x57, 0x48, 0x29, 0xE6, 0xEE, 0x68, 0x11, 0x2C, 0xB3, 0x81, 0x7B};
    res = res && (memcmp(cmac, cmacres3, sizeof(cmacres3)) == 0);

    uint8_t key4[] = {0x2A, 0x47, 0x3E, 0x38, 0xBB, 0xF4, 0x53, 0x7C, 0x53, 0x97, 0xF4, 0x5A, 0xE4, 0x98, 0xCD, 0x4D};
    LRPSetKey(&ctx, key4, 0, true);
    uint8_t data4[] = {0xC2, 0xAC, 0x3D, 0x72, 0x50, 0xEE, 0xF0, 0x23, 0x18, 0xBC, 0x08, 0x4F, 0x29, 0x4B, 0x1A, 0xC7,
                       0x22, 0x91, 0xEE, 0x1D, 0xC0, 0x2A, 0xF4, 0x24, 0x94, 0x1C, 0xAA, 0xC6, 0x85, 0xFC, 0xA5, 0x9D,
                       0x90, 0x08, 0x67, 0x9B, 0x00, 0xC5, 0x6A, 0x05, 0x62, 0x58, 0x3B, 0xDA, 0xEC, 0x0B, 0xBA
                      };
    LRPCMAC(&ctx, data4, sizeof(data4), cmac);
    uint8_t cmacres4[] = {0x66, 0xDC, 0x2B, 0xCE, 0x26, 0x9B, 0x79, 0x3B, 0x4A, 0xCA, 0x1A, 0x4D, 0x04, 0xDD, 0xD6, 0x68};
    res = res && (memcmp(cmac, cmacres4, sizeof(cmacres4)) == 0);

    uint8_t key5[] = {0x63, 0xA0, 0x16, 0x9B, 0x4D, 0x9F, 0xE4, 0x2C, 0x72, 0xB2, 0x78, 0x4C, 0x80, 0x6E, 0xAC, 0x21};
    LRPSetKey(&ctx, key5, 0, true);
    LRPCMAC(&ctx, NULL, 0, cmac);
    uint8_t cmacres5[] = {0x0E, 0x07, 0xC6, 0x01, 0x97, 0x08, 0x14, 0xA4, 0x17, 0x6F, 0xDA, 0x63, 0x3C, 0x6F, 0xC3, 0xDE};
    res = res && (memcmp(cmac, cmacres5, sizeof(cmacres5)) == 0);

    uint8_t key6[] = {0x95, 0x2F, 0xDE, 0x83, 0x93, 0xC4, 0x5D, 0x23, 0x0A, 0x5B, 0xE9, 0xB3, 0x86, 0x36, 0xD1, 0x54};
    LRPSetKey(&ctx, key6, 0, true);
    uint8_t data6[] = {0xD7, 0x80, 0x0E, 0x25, 0x70, 0x01, 0xA7, 0x74, 0xAE, 0x7B, 0xCF, 0xB2, 0xCE, 0x13, 0x07, 0xB5,
                       0xB0, 0x44
                      };
    LRPCMAC(&ctx, data6, sizeof(data6), cmac);
    uint8_t cmacres6[] = {0x05, 0xF1, 0xCE, 0x30, 0x45, 0x1A, 0x03, 0xA6, 0xE4, 0x68, 0xB3, 0xA5, 0x90, 0x33, 0xA5, 0x54};
    res = res && (memcmp(cmac, cmacres6, sizeof(cmacres6)) == 0);

    if (res)
        PrintAndLogEx(INFO, "LRP CMAC.......... " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "LRP CMAC.......... " _RED_("fail"));

    return res;
}

// https://www.nxp.com/docs/en/application-note/AN12343.pdf
// page 49
static bool TestLRPSessionKeys(void) {
    bool res = true;

    uint8_t key[16] = {0};
    uint8_t rnda[] = {0x74, 0xD7, 0xDF, 0x6A, 0x2C, 0xEC, 0x0B, 0x72, 0xB4, 0x12, 0xDE, 0x0D, 0x2B, 0x11, 0x17, 0xE6};
    uint8_t rndb[] = {0x56, 0x10, 0x9A, 0x31, 0x97, 0x7C, 0x85, 0x53, 0x19, 0xCD, 0x46, 0x18, 0xC9, 0xD2, 0xAE, 0xD2};
    uint8_t sessionkeyres[] = {0x13, 0x2D, 0x7E, 0x6F, 0x35, 0xBA, 0x86, 0x1F, 0x39, 0xB3, 0x72, 0x21, 0x21, 0x4E, 0x25, 0xA5};

    uint8_t sessionkey[16] = {0};
    DesfireGenSessionKeyLRP(key, rnda, rndb, true, sessionkey);
    res = res && (memcmp(sessionkey, sessionkeyres, sizeof(sessionkeyres)) == 0);

    if (res)
        PrintAndLogEx(INFO, "LRP session keys.. " _GREEN_("passed"));
    else
        PrintAndLogEx(ERR,  "LRP session keys.. " _RED_("fail"));

    return res;
}

bool DesfireTest(bool verbose) {
    bool res = true;

    PrintAndLogEx(INFO, "------ " _CYAN_("Desfire Tests") " ------");

    res = res && TestCRC16();
    res = res && TestCRC32();
    res = res && TestCMACSubkeys();
    res = res && TestAn10922KDFAES();
    res = res && TestAn10922KDF2TDEA();
    res = res && TestAn10922KDF3TDEA();
    res = res && TestCMAC3TDEA();
    res = res && TestCMAC2TDEA();
    res = res && TestCMACDES();
    res = res && TestEV2SessionKeys();
    res = res && TestEV2IVEncode();
    res = res && TestEV2MAC();
    res = res && TestTransSessionKeys();
    res = res && TestLRPPlaintexts();
    res = res && TestLRPUpdatedKeys();
    res = res && TestLRPEval();
    res = res && TestLRPIncCounter();
    res = res && TestLRPEncode();
    res = res && TestLRPDecode();
    res = res && TestLRPSubkeys();
    res = res && TestLRPCMAC();
    res = res && TestLRPSessionKeys();

    PrintAndLogEx(INFO, "---------------------------");
    if (res)
        PrintAndLogEx(SUCCESS, "    Tests [ %s ]", _GREEN_("ok"));
    else
        PrintAndLogEx(FAILED, "    Tests [ %s ]", _RED_("fail"));

    PrintAndLogEx(NORMAL, "");
    return res;
}
