// Copyright (c) 2024 Bitok Developers
// Stealth address support - implementing Satoshi's "key blinding" concept
// from the BitcoinTalk "Not a suggestion" thread (August 2010)
//
// Protocol: ECDH-based stealth addresses
//   Receiver publishes stealth address = (scan_pubkey, spend_pubkey)
//   Sender generates ephemeral keypair, derives shared secret via ECDH,
//   creates a one-time destination key, and embeds the ephemeral pubkey
//   in an OP_RETURN output for the receiver to scan.

#ifndef STEALTH_H
#define STEALTH_H

#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/bn.h>
#include <openssl/sha.h>
#include <vector>
#include <string>

static const unsigned char STEALTH_ADDRESS_VERSION = 0x01;
static const char* STEALTH_ADDRESS_PREFIX = "ok";
static const unsigned char STEALTH_OP_RETURN_PREFIX = 0x06;

struct CStealthAddress
{
    vector<unsigned char> scan_pubkey;
    vector<unsigned char> spend_pubkey;
    string label;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(scan_pubkey);
        READWRITE(spend_pubkey);
        READWRITE(label);
    )

    bool SetEncoded(const string& strAddr);
    string Encoded() const;
    bool IsValid() const;
};

bool GenerateStealthAddress(CStealthAddress& stealthAddr, CKey& scanKey, CKey& spendKey);

bool StealthSecret(const vector<unsigned char>& vchSenderPrivOrScanPriv,
                   const vector<unsigned char>& vchEphemPubOrScanPub,
                   const vector<unsigned char>& vchSpendPub,
                   vector<unsigned char>& vchSharedSecret,
                   vector<unsigned char>& vchDestPubKey);

bool StealthSecretSpend(const vector<unsigned char>& vchScanPriv,
                        const vector<unsigned char>& vchEphemPub,
                        const vector<unsigned char>& vchSpendPriv,
                        vector<unsigned char>& vchDestPrivKey);

bool StealthEphemeral(const CStealthAddress& addr,
                      vector<unsigned char>& vchEphemPubKey,
                      vector<unsigned char>& vchDestPubKey,
                      vector<unsigned char>& vchSharedSecret);

bool StealthScan(const vector<unsigned char>& vchScanPriv,
                 const vector<unsigned char>& vchSpendPub,
                 const vector<unsigned char>& vchEphemPub,
                 vector<unsigned char>& vchDestPubKey);

vector<unsigned char> BuildStealthOpReturn(const vector<unsigned char>& vchEphemPub);

bool ParseStealthOpReturn(const CScript& script, vector<unsigned char>& vchEphemPub);

extern vector<CStealthAddress> vStealthAddresses;
extern map<vector<unsigned char>, pair<vector<unsigned char>, vector<unsigned char> > > mapStealthDestToScan;
extern CCriticalSection cs_stealthAddresses;

#endif
