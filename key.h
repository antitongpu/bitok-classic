// Copyright (c) 2009-2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
//
// Modernized for OpenSSL 3.x compatibility

#ifndef KEY_H
#define KEY_H

#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>

// secp160k1
// const unsigned int PRIVATE_KEY_SIZE = 192;
// const unsigned int PUBLIC_KEY_SIZE  = 41;
// const unsigned int SIGNATURE_SIZE   = 48;
//
// secp192k1
// const unsigned int PRIVATE_KEY_SIZE = 222;
// const unsigned int PUBLIC_KEY_SIZE  = 49;
// const unsigned int SIGNATURE_SIZE   = 57;
//
// secp224k1
// const unsigned int PRIVATE_KEY_SIZE = 250;
// const unsigned int PUBLIC_KEY_SIZE  = 57;
// const unsigned int SIGNATURE_SIZE   = 66;
//
// secp256k1:
// const unsigned int PRIVATE_KEY_SIZE = 279;
// const unsigned int PUBLIC_KEY_SIZE  = 65;
// const unsigned int SIGNATURE_SIZE   = 72;
//
// see www.keylength.com
// script supports up to 75 for single byte push



class key_error : public std::runtime_error
{
public:
    explicit key_error(const std::string& str) : std::runtime_error(str) {}
};


// secure_allocator is defined in serialize.h
typedef vector<unsigned char, secure_allocator<unsigned char> > CPrivKey;



class CKey
{
protected:
    EC_KEY* pkey;
    bool fSet;

public:
    CKey()
    {
        pkey = EC_KEY_new_by_curve_name(NID_secp256k1);
        if (pkey == NULL)
            throw key_error("CKey::CKey() : EC_KEY_new_by_curve_name failed");
        fSet = false;
    }

    CKey(const CKey& b)
    {
        pkey = EC_KEY_dup(b.pkey);
        if (pkey == NULL)
            throw key_error("CKey::CKey(const CKey&) : EC_KEY_dup failed");
        fSet = b.fSet;
    }

    CKey& operator=(const CKey& b)
    {
        if (!EC_KEY_copy(pkey, b.pkey))
            throw key_error("CKey::operator=(const CKey&) : EC_KEY_copy failed");
        fSet = b.fSet;
        return (*this);
    }

    ~CKey()
    {
        EC_KEY_free(pkey);
    }

    bool IsNull() const
    {
        return !fSet;
    }

    void MakeNewKey()
    {
        if (!EC_KEY_generate_key(pkey))
            throw key_error("CKey::MakeNewKey() : EC_KEY_generate_key failed");
        fSet = true;
    }

    bool SetPrivKey(const CPrivKey& vchPrivKey)
    {
        if (vchPrivKey.empty())
            return false;
        const unsigned char* pbegin = &vchPrivKey[0];
        if (!d2i_ECPrivateKey(&pkey, &pbegin, vchPrivKey.size()))
            return false;
        fSet = true;
        return true;
    }

    CPrivKey GetPrivKey() const
    {
        unsigned int nSize = i2d_ECPrivateKey(pkey, NULL);
        if (!nSize)
            throw key_error("CKey::GetPrivKey() : i2d_ECPrivateKey failed");
        CPrivKey vchPrivKey(nSize, 0);
        unsigned char* pbegin = &vchPrivKey[0];
        if (i2d_ECPrivateKey(pkey, &pbegin) != nSize)
            throw key_error("CKey::GetPrivKey() : i2d_ECPrivateKey returned unexpected size");
        return vchPrivKey;
    }

    bool SetSecret(const vector<unsigned char>& vchSecret)
    {
        if (vchSecret.size() != 32)
            return false;
        BIGNUM *bn = BN_bin2bn(&vchSecret[0], 32, BN_new());
        if (bn == NULL)
            return false;
        if (!EC_KEY_set_private_key(pkey, bn))
        {
            BN_free(bn);
            return false;
        }
        const EC_GROUP *group = EC_KEY_get0_group(pkey);
        EC_POINT *pub_key = EC_POINT_new(group);
        if (!EC_POINT_mul(group, pub_key, EC_KEY_get0_private_key(pkey), NULL, NULL, NULL))
        {
            EC_POINT_free(pub_key);
            BN_free(bn);
            return false;
        }
        EC_KEY_set_public_key(pkey, pub_key);
        EC_POINT_free(pub_key);
        BN_free(bn);
        fSet = true;
        return true;
    }

    vector<unsigned char> GetSecret() const
    {
        const BIGNUM *bn = EC_KEY_get0_private_key(pkey);
        if (bn == NULL)
            throw key_error("CKey::GetSecret() : EC_KEY_get0_private_key failed");
        int nBytes = BN_num_bytes(bn);
        vector<unsigned char> vchSecret(32, 0);
        int n = BN_bn2bin(bn, &vchSecret[32 - nBytes]);
        if (n != nBytes)
            throw key_error("CKey::GetSecret() : BN_bn2bin failed");
        return vchSecret;
    }

    bool SetPubKey(const vector<unsigned char>& vchPubKey)
    {
        if (vchPubKey.empty())
            return false;
        const unsigned char* pbegin = &vchPubKey[0];
        if (!o2i_ECPublicKey(&pkey, &pbegin, vchPubKey.size()))
            return false;
        fSet = true;
        return true;
    }

    vector<unsigned char> GetPubKey() const
    {
        unsigned int nSize = i2o_ECPublicKey(pkey, NULL);
        if (!nSize)
            throw key_error("CKey::GetPubKey() : i2o_ECPublicKey failed");
        vector<unsigned char> vchPubKey(nSize, 0);
        unsigned char* pbegin = &vchPubKey[0];
        if (i2o_ECPublicKey(pkey, &pbegin) != nSize)
            throw key_error("CKey::GetPubKey() : i2o_ECPublicKey returned unexpected size");
        return vchPubKey;
    }

    vector<unsigned char> GetCompressedPubKey() const
    {
        const EC_GROUP* group = EC_KEY_get0_group(pkey);
        const EC_POINT* point = EC_KEY_get0_public_key(pkey);
        if (!point)
            throw key_error("CKey::GetCompressedPubKey() : no public key");
        BN_CTX* ctx = BN_CTX_new();
        if (!ctx)
            throw key_error("CKey::GetCompressedPubKey() : BN_CTX_new failed");
        size_t nSize = EC_POINT_point2oct(group, point, POINT_CONVERSION_COMPRESSED, NULL, 0, ctx);
        vector<unsigned char> vchPubKey(nSize, 0);
        if (EC_POINT_point2oct(group, point, POINT_CONVERSION_COMPRESSED, &vchPubKey[0], nSize, ctx) != nSize)
        {
            BN_CTX_free(ctx);
            throw key_error("CKey::GetCompressedPubKey() : EC_POINT_point2oct failed");
        }
        BN_CTX_free(ctx);
        return vchPubKey;
    }

    bool Sign(uint256 hash, vector<unsigned char>& vchSig)
    {
        vchSig.clear();
        ECDSA_SIG* sig = ECDSA_do_sign((unsigned char*)&hash, sizeof(hash), pkey);
        if (sig == NULL)
            return false;

        const BIGNUM *r, *s;
        ECDSA_SIG_get0(sig, &r, &s);

        const EC_GROUP* group = EC_KEY_get0_group(pkey);
        BIGNUM* order = BN_new();
        BIGNUM* halforder = BN_new();
        EC_GROUP_get_order(group, order, NULL);
        BN_rshift1(halforder, order);

        if (BN_cmp(s, halforder) > 0)
        {
            BIGNUM* s_new = BN_new();
            BN_sub(s_new, order, s);
            ECDSA_SIG_set0(sig, BN_dup(r), s_new);
        }

        BN_free(order);
        BN_free(halforder);

        unsigned int nSize = i2d_ECDSA_SIG(sig, NULL);
        vchSig.resize(nSize);
        unsigned char* pos = &vchSig[0];
        i2d_ECDSA_SIG(sig, &pos);
        ECDSA_SIG_free(sig);
        return true;
    }

    bool Verify(uint256 hash, const vector<unsigned char>& vchSig)
    {
        if (vchSig.empty())
            return false;
        if (ECDSA_verify(0, (unsigned char*)&hash, sizeof(hash), &vchSig[0], vchSig.size(), pkey) != 1)
            return false;
        return true;
    }

    static bool Sign(const CPrivKey& vchPrivKey, uint256 hash, vector<unsigned char>& vchSig)
    {
        CKey key;
        if (!key.SetPrivKey(vchPrivKey))
            return false;
        return key.Sign(hash, vchSig);
    }

    static bool Verify(const vector<unsigned char>& vchPubKey, uint256 hash, const vector<unsigned char>& vchSig)
    {
        CKey key;
        if (!key.SetPubKey(vchPubKey))
            return false;
        return key.Verify(hash, vchSig);
    }
};

#endif // KEY_H
