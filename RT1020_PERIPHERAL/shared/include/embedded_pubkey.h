#ifndef EMBEDDED_PUBKEY_H
#define EMBEDDED_PUBKEY_H

#include <stdint.h>
#include "bignum.h"

/* RSA-2048 public modulus (e=65537), embedded in Stage 1.
 * Source: tests/vectors/rsa_test_key.pem
 * 자동 생성: tools/extract_pubkey.py
 */
static const bn_t EMBEDDED_PUBKEY_MODULUS = {
    0x29125F73u,
    0xEA19B639u,
    0x01933EB5u,
    0xF61A3063u,
    0xBAB8E2F2u,
    0x702E5680u,
    0x8C8F5E7Cu,
    0x315BC20Du,
    0xFC1D6910u,
    0x93922D95u,
    0x0A3418E8u,
    0xE0A7CFB7u,
    0xE629AAD7u,
    0x4ACC0E19u,
    0x48BDF0DFu,
    0xF42860F1u,
    0x4214B0CDu,
    0x682A1E6Du,
    0x2A89D830u,
    0xC32203FDu,
    0x611F9069u,
    0x000828C9u,
    0xA05A57FFu,
    0xB98C02A7u,
    0x14D6D17Cu,
    0x14001727u,
    0xE5F2726Cu,
    0x50CCA771u,
    0xD6BBBBA3u,
    0xC50D316Du,
    0xDF7BAD70u,
    0x78D98131u,
    0x34253135u,
    0x509F82A2u,
    0xCEACAEC2u,
    0x979EA66Du,
    0xDF3C3D21u,
    0xE39FBD7Bu,
    0xB5619115u,
    0x97EBE9DCu,
    0xF4E4E9E2u,
    0x7C1A8330u,
    0x7356B799u,
    0xF551D61Fu,
    0xDA743B20u,
    0x5548E40Fu,
    0xCE57A511u,
    0x8FD74A77u,
    0xC4A0EF14u,
    0x7A750E98u,
    0x0ACB3A94u,
    0x454BB71Fu,
    0x6F6EE79Fu,
    0xBAAD155Cu,
    0x77A22A3Bu,
    0xF7A341F3u,
    0x17F213BEu,
    0x80BABB7Au,
    0x00F5421Cu,
    0x106EDE8Fu,
    0xD3C4980Fu,
    0x08CA1EF6u,
    0x9348A318u,
    0xB3E51853u
};

#endif /* EMBEDDED_PUBKEY_H */
