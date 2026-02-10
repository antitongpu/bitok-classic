// Copyright (c) 2026 Bitok developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITOK_BLOOM_H
#define BITOK_BLOOM_H

#include <vector>
#include <cstring>

enum bloomflags
{
    BLOOM_UPDATE_NONE = 0,
    BLOOM_UPDATE_ALL = 1,
    BLOOM_UPDATE_P2PUBKEY_ONLY = 2,
};

static const unsigned int MAX_BLOOM_FILTER_SIZE = 36000;
static const unsigned int MAX_HASH_FUNCS = 50;
static const unsigned int BLOOM_TWEAK_DEFAULT = 2147483649UL;

class CBloomFilter
{
private:
    std::vector<unsigned char> vData;
    unsigned int nHashFuncs;
    unsigned int nTweak;
    unsigned char nFlags;

    unsigned int Hash(unsigned int nHashNum, const std::vector<unsigned char>& vDataToHash) const
    {
        unsigned int nIndex = nHashNum * 0xFBA4C795 + nTweak;
        for (unsigned int i = 0; i < vDataToHash.size(); i++)
        {
            nIndex ^= vDataToHash[i];
            nIndex += (nIndex << 1) + (nIndex << 4) + (nIndex << 7) + (nIndex << 8) + (nIndex << 24);
        }
        return nIndex % (vData.size() * 8);
    }

public:
    CBloomFilter()
    {
        nHashFuncs = 0;
        nTweak = 0;
        nFlags = 0;
    }

    CBloomFilter(unsigned int nElements, double nFPRate, unsigned int nTweakIn, unsigned char nFlagsIn)
    {
        double dFilterBytes = -1.0 / (0.6931471805599453 * 0.6931471805599453) * (double)nElements * log(nFPRate) / 8.0;
        unsigned int nFilterBytes = (unsigned int)dFilterBytes;
        if (nFilterBytes > MAX_BLOOM_FILTER_SIZE)
            nFilterBytes = MAX_BLOOM_FILTER_SIZE;
        vData.resize(nFilterBytes, 0);

        nHashFuncs = (unsigned int)((double)(vData.size() * 8) / (double)nElements * 0.6931471805599453);
        if (nHashFuncs > MAX_HASH_FUNCS)
            nHashFuncs = MAX_HASH_FUNCS;

        nTweak = nTweakIn;
        nFlags = nFlagsIn;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(vData);
        READWRITE(nHashFuncs);
        READWRITE(nTweak);
        READWRITE(nFlags);
    )

    void insert(const std::vector<unsigned char>& vKey)
    {
        for (unsigned int i = 0; i < nHashFuncs; i++)
        {
            unsigned int nIndex = Hash(i, vKey);
            vData[nIndex >> 3] |= (1 << (7 & nIndex));
        }
    }

    void insert(const uint256& hash)
    {
        std::vector<unsigned char> data(hash.begin(), hash.end());
        insert(data);
    }

    bool contains(const std::vector<unsigned char>& vKey) const
    {
        for (unsigned int i = 0; i < nHashFuncs; i++)
        {
            unsigned int nIndex = Hash(i, vKey);
            if (!(vData[nIndex >> 3] & (1 << (7 & nIndex))))
                return false;
        }
        return true;
    }

    bool contains(const uint256& hash) const
    {
        std::vector<unsigned char> data(hash.begin(), hash.end());
        return contains(data);
    }

    bool IsWithinSizeConstraints() const
    {
        return vData.size() <= MAX_BLOOM_FILTER_SIZE && nHashFuncs <= MAX_HASH_FUNCS;
    }

    bool IsRelevantAndUpdate(const CTransaction& tx, const uint256& hash)
    {
        bool fFound = false;

        if (vData.empty())
            return false;

        if (contains(hash))
            fFound = true;

        for (unsigned int i = 0; i < tx.vout.size(); i++)
        {
            const CTxOut& txout = tx.vout[i];
            CScript::const_iterator pc = txout.scriptPubKey.begin();
            opcodetype opcode;
            std::vector<unsigned char> data;
            while (txout.scriptPubKey.GetOp(pc, opcode, data))
            {
                if (data.size() != 0 && contains(data))
                {
                    fFound = true;
                    if ((nFlags & BLOOM_UPDATE_ALL) ||
                        ((nFlags & BLOOM_UPDATE_P2PUBKEY_ONLY) && (opcode == OP_CHECKSIG || opcode == OP_CHECKMULTISIG)))
                    {
                        COutPoint outpoint(hash, i);
                        CDataStream ss(SER_NETWORK);
                        ss << outpoint;
                        std::vector<unsigned char> vOutpoint(ss.begin(), ss.end());
                        insert(vOutpoint);
                    }
                    break;
                }
            }
        }

        if (fFound)
            return true;

        foreach(const CTxIn& txin, tx.vin)
        {
            CDataStream ssOut(SER_NETWORK);
            ssOut << txin.prevout;
            std::vector<unsigned char> vOutpoint(ssOut.begin(), ssOut.end());
            if (contains(vOutpoint))
                return true;

            CScript::const_iterator pc = txin.scriptSig.begin();
            opcodetype opcode;
            std::vector<unsigned char> data;
            while (txin.scriptSig.GetOp(pc, opcode, data))
            {
                if (data.size() != 0 && contains(data))
                    return true;
            }
        }

        return false;
    }

    void clear()
    {
        vData.assign(vData.size(), 0);
    }

    bool isEmpty() const
    {
        return vData.empty();
    }
};

#endif
