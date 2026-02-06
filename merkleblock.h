// Copyright (c) 2024 Bitok developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITOK_MERKLEBLOCK_H
#define BITOK_MERKLEBLOCK_H

class CPartialMerkleTree
{
protected:
    unsigned int nTransactions;
    std::vector<uint256> vHash;
    std::vector<bool> vBits;

    unsigned int CalcTreeWidth(int height)
    {
        return (nTransactions + (1 << height) - 1) >> height;
    }

    uint256 CalcHash(int height, unsigned int pos, const std::vector<uint256>& vTxid)
    {
        if (height == 0)
        {
            return vTxid[pos];
        }
        else
        {
            uint256 left = CalcHash(height - 1, pos * 2, vTxid);
            uint256 right;
            if (pos * 2 + 1 < CalcTreeWidth(height - 1))
                right = CalcHash(height - 1, pos * 2 + 1, vTxid);
            else
                right = left;
            return Hash(BEGIN(left), END(left), BEGIN(right), END(right));
        }
    }

    void TraverseAndBuild(int height, unsigned int pos, const std::vector<uint256>& vTxid, const std::vector<bool>& vMatch)
    {
        bool fParentOfMatch = false;
        for (unsigned int p = pos << height; p < (pos + 1) << height && p < nTransactions; p++)
            fParentOfMatch |= vMatch[p];
        vBits.push_back(fParentOfMatch);
        if (height == 0 || !fParentOfMatch)
        {
            vHash.push_back(CalcHash(height, pos, vTxid));
        }
        else
        {
            TraverseAndBuild(height - 1, pos * 2, vTxid, vMatch);
            if (pos * 2 + 1 < CalcTreeWidth(height - 1))
                TraverseAndBuild(height - 1, pos * 2 + 1, vTxid, vMatch);
        }
    }

    uint256 TraverseAndExtract(int height, unsigned int pos, unsigned int& nBitsUsed, unsigned int& nHashUsed, std::vector<uint256>& vMatch)
    {
        if (nBitsUsed >= vBits.size())
            return 0;

        bool fParentOfMatch = vBits[nBitsUsed++];
        if (height == 0 || !fParentOfMatch)
        {
            if (nHashUsed >= vHash.size())
                return 0;
            uint256 hash = vHash[nHashUsed++];
            if (height == 0 && fParentOfMatch)
                vMatch.push_back(hash);
            return hash;
        }
        else
        {
            uint256 left = TraverseAndExtract(height - 1, pos * 2, nBitsUsed, nHashUsed, vMatch);
            uint256 right;
            if (pos * 2 + 1 < CalcTreeWidth(height - 1))
                right = TraverseAndExtract(height - 1, pos * 2 + 1, nBitsUsed, nHashUsed, vMatch);
            else
                right = left;
            return Hash(BEGIN(left), END(left), BEGIN(right), END(right));
        }
    }

public:
    CPartialMerkleTree()
    {
        nTransactions = 0;
    }

    CPartialMerkleTree(const std::vector<uint256>& vTxid, const std::vector<bool>& vMatch)
    {
        nTransactions = vTxid.size();
        int nHeight = 0;
        while (CalcTreeWidth(nHeight) > 1)
            nHeight++;
        TraverseAndBuild(nHeight, 0, vTxid, vMatch);
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(nTransactions);
        READWRITE(vHash);

        std::vector<unsigned char> vBytes;
        if (fWrite)
        {
            vBytes.resize((vBits.size() + 7) / 8);
            for (unsigned int p = 0; p < vBits.size(); p++)
                vBytes[p / 8] |= vBits[p] << (p % 8);
            READWRITE(vBytes);
        }
        else
        {
            READWRITE(vBytes);
            std::vector<bool>& bitsRef = const_cast<std::vector<bool>&>(vBits);
            bitsRef.resize(vBytes.size() * 8);
            for (unsigned int p = 0; p < bitsRef.size(); p++)
                bitsRef[p] = (vBytes[p / 8] >> (p % 8)) & 1;
        }
    )

    uint256 ExtractMatches(std::vector<uint256>& vMatch)
    {
        vMatch.clear();
        if (nTransactions == 0)
            return 0;
        if (nTransactions > MAX_BLOCK_SIZE / 60)
            return 0;
        if (vHash.size() > nTransactions)
            return 0;
        if (vBits.size() < vHash.size())
            return 0;

        int nHeight = 0;
        while (CalcTreeWidth(nHeight) > 1)
            nHeight++;

        unsigned int nBitsUsed = 0;
        unsigned int nHashUsed = 0;
        uint256 hashMerkleRoot = TraverseAndExtract(nHeight, 0, nBitsUsed, nHashUsed, vMatch);

        if (nBitsUsed != vBits.size())
        {
            bool fAllZero = true;
            for (unsigned int i = nBitsUsed; i < vBits.size(); i++)
                if (vBits[i])
                    fAllZero = false;
            if (!fAllZero)
                return 0;
        }

        if (nHashUsed != vHash.size())
            return 0;

        return hashMerkleRoot;
    }
};


class CMerkleBlock
{
public:
    CBlock header;
    CPartialMerkleTree txn;

    CMerkleBlock() {}

    CMerkleBlock(const CBlock& block, CBloomFilter& filter)
    {
        header = block;
        header.vtx.clear();

        std::vector<bool> vMatch;
        std::vector<uint256> vHashes;

        vMatch.reserve(block.vtx.size());
        vHashes.reserve(block.vtx.size());

        for (unsigned int i = 0; i < block.vtx.size(); i++)
        {
            uint256 hash = block.vtx[i].GetHash();
            vHashes.push_back(hash);
            vMatch.push_back(filter.IsRelevantAndUpdate(block.vtx[i], hash));
        }

        txn = CPartialMerkleTree(vHashes, vMatch);
    }

    CMerkleBlock(const CBlock& block, const std::set<uint256>& txids)
    {
        header = block;
        header.vtx.clear();

        std::vector<bool> vMatch;
        std::vector<uint256> vHashes;

        vMatch.reserve(block.vtx.size());
        vHashes.reserve(block.vtx.size());

        for (unsigned int i = 0; i < block.vtx.size(); i++)
        {
            uint256 hash = block.vtx[i].GetHash();
            vHashes.push_back(hash);
            vMatch.push_back(txids.count(hash) > 0);
        }

        txn = CPartialMerkleTree(vHashes, vMatch);
    }

    IMPLEMENT_SERIALIZE
    (
        nSerSize += SerReadWrite(s, header, nType | SER_BLOCKHEADERONLY, nVersion, ser_action);
        nSerSize += SerReadWrite(s, txn, nType, nVersion, ser_action);
    )
};

#endif
