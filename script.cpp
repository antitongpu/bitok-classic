// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2026 Bitok developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
//
// Script Exec: Consensus-Level Script Security Hardening

#include "headers.h"

bool CheckSig(vector<unsigned char> vchSig, vector<unsigned char> vchPubKey, CScript scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType, unsigned int flags);



typedef vector<unsigned char> valtype;
static const valtype vchFalse(0);
static const valtype vchZero(0);
static const valtype vchTrue(1, 1);
static const CBigNum bnZero(0);
static const CBigNum bnOne(1);
static const CBigNum bnFalse(0);
static const CBigNum bnTrue(1);


bool CastToBool(const valtype& vch)
{
    for (unsigned int i = 0; i < vch.size(); i++)
    {
        if (vch[i] != 0)
        {
            if (i == vch.size()-1 && vch[i] == 0x80)
                return false;
            return true;
        }
    }
    return false;
}

static bool CastToBigNum(const valtype& vch, CBigNum& bn, unsigned int flags, int nMaxSize = MAX_BIGNUM_SIZE)
{
    if ((flags & SCRIPT_VERIFY_EXEC) && (int)vch.size() > nMaxSize)
        return false;

    if ((flags & SCRIPT_VERIFY_EXEC) && vch.size() > 0)
    {
        if ((vch.back() & 0x7f) == 0)
        {
            if (vch.size() <= 1 || (vch[vch.size() - 2] & 0x80) == 0)
                return false;
        }
    }

    bn = CBigNum(vch);
    return true;
}

void MakeSameSize(valtype& vch1, valtype& vch2)
{
    if (vch1.size() < vch2.size())
        vch1.resize(vch2.size(), 0);
    if (vch2.size() < vch1.size())
        vch2.resize(vch1.size(), 0);
}


bool CheckMinimalPush(const vector<unsigned char>& data, opcodetype opcode)
{
    if (data.size() == 0)
        return opcode == OP_0;

    if (data.size() == 1 && data[0] >= 1 && data[0] <= 16)
        return opcode == OP_1 + (data[0] - 1);

    if (data.size() == 1 && data[0] == 0x81)
        return opcode == OP_1NEGATE;

    if (data.size() <= 75)
        return opcode == data.size();

    if (data.size() <= 255)
        return opcode == OP_PUSHDATA1;

    if (data.size() <= 65535)
        return opcode == OP_PUSHDATA2;

    return true;
}


static bool IsValidSignatureEncoding(const vector<unsigned char>& sig)
{
    if (sig.size() < 9) return false;
    if (sig.size() > 73) return false;
    if (sig[0] != 0x30) return false;
    if (sig[1] != sig.size() - 3) return false;
    unsigned int lenR = sig[3];
    if (5 + lenR >= sig.size()) return false;
    unsigned int lenS = sig[5 + lenR];
    if ((size_t)(lenR + lenS + 7) != sig.size()) return false;
    if (sig[2] != 0x02) return false;
    if (lenR == 0) return false;
    if (sig[4] & 0x80) return false;
    if (lenR > 1 && (sig[4] == 0x00) && !(sig[5] & 0x80)) return false;
    if (sig[lenR + 4] != 0x02) return false;
    if (lenS == 0) return false;
    if (sig[lenR + 6] & 0x80) return false;
    if (lenS > 1 && (sig[lenR + 6] == 0x00) && !(sig[lenR + 7] & 0x80)) return false;
    return true;
}

static bool IsLowDERSignature(const vector<unsigned char>& vchSig)
{
    if (!IsValidSignatureEncoding(vchSig))
        return false;

    unsigned int lenR = vchSig[3];
    unsigned int lenS = vchSig[5 + lenR];
    const unsigned char* S = &vchSig[6 + lenR];

    static const unsigned char halfOrder[] = {
        0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x5D, 0x57, 0x6E, 0x73, 0x57, 0xA4, 0x50, 0x1D,
        0xDF, 0xE9, 0x2F, 0x46, 0x68, 0x1B, 0x20, 0xA0,
    };

    if (lenS > 32) return false;

    unsigned char paddedS[32];
    memset(paddedS, 0, sizeof(paddedS));
    memcpy(paddedS + 32 - lenS, S, lenS);

    for (unsigned int i = 0; i < 32; i++)
    {
        if (paddedS[i] < halfOrder[i]) return true;
        if (paddedS[i] > halfOrder[i]) return false;
    }
    return true;
}

static bool IsDefinedHashtypeSignature(const vector<unsigned char>& vchSig)
{
    if (vchSig.size() == 0)
        return false;
    unsigned char nHashType = vchSig[vchSig.size() - 1] & (~SIGHASH_ANYONECANPAY);
    if (nHashType < SIGHASH_ALL || nHashType > SIGHASH_SINGLE)
        return false;
    return true;
}

bool CheckSignatureEncoding(const vector<unsigned char>& vchSig, unsigned int flags)
{
    if (vchSig.size() == 0)
        return true;
    if (!(flags & SCRIPT_VERIFY_EXEC))
        return true;
    if (!IsValidSignatureEncoding(vchSig))
        return false;
    if (!IsLowDERSignature(vchSig))
        return false;
    if (!IsDefinedHashtypeSignature(vchSig))
        return false;
    return true;
}


unsigned int GetSigOpCount(const CScript& script)
{
    unsigned int n = 0;
    CScript::const_iterator pc = script.begin();
    opcodetype lastOpcode = OP_INVALIDOPCODE;
    while (pc < script.end())
    {
        opcodetype opcode;
        valtype vchPushValue;
        if (!script.GetOp(pc, opcode, vchPushValue))
            break;
        if (opcode == OP_CHECKSIG || opcode == OP_CHECKSIGVERIFY)
            n++;
        else if (opcode == OP_CHECKMULTISIG || opcode == OP_CHECKMULTISIGVERIFY)
        {
            if (lastOpcode >= OP_1 && lastOpcode <= OP_16)
                n += (int)lastOpcode - (int)(OP_1 - 1);
            else
                n += MAX_PUBKEYS_PER_MULTISIG;
        }
        lastOpcode = opcode;
    }
    return n;
}


//
// Script is a stack machine (like Forth) that evaluates a predicate
// returning a bool indicating valid or not.  There are no loops.
//
#define stacktop(i)  (stack.at(stack.size()+(i)))
#define altstacktop(i)  (altstack.at(altstack.size()+(i)))

bool EvalScript(vector<vector<unsigned char> >& stack, const CScript& script,
                const CTransaction& txTo, unsigned int nIn, int nHashType,
                unsigned int flags)
{
    CAutoBN_CTX pctx;
    CScript::const_iterator pc = script.begin();
    CScript::const_iterator pend = script.end();
    CScript::const_iterator pbegincodehash = script.begin();
    vector<bool> vfExec;
    vector<valtype> altstack;
    int nOpCount = 0;

    if ((flags & SCRIPT_VERIFY_EXEC) && script.size() > MAX_SCRIPT_SIZE)
        return false;

    try
    {

    while (pc < pend)
    {
        bool fExec = !count(vfExec.begin(), vfExec.end(), false);

        //
        // Read instruction
        //
        opcodetype opcode;
        valtype vchPushValue;
        if (!script.GetOp(pc, opcode, vchPushValue))
            return false;

        if ((flags & SCRIPT_VERIFY_EXEC) && vchPushValue.size() > MAX_SCRIPT_ELEMENT_SIZE)
            return false;

        if ((flags & SCRIPT_VERIFY_EXEC) && opcode > OP_16 && ++nOpCount > (int)MAX_OPS_PER_SCRIPT)
            return false;

        if ((flags & SCRIPT_VERIFY_EXEC) &&
            (opcode == OP_VERIF || opcode == OP_VERNOTIF))
            return false;

        if (fExec && opcode <= OP_PUSHDATA4)
        {
            if ((flags & SCRIPT_VERIFY_EXEC) && !CheckMinimalPush(vchPushValue, opcode))
                return false;
            stack.push_back(vchPushValue);
        }
        else if (fExec || (OP_IF <= opcode && opcode <= OP_ENDIF))
        switch (opcode)
        {
            //
            // Push value
            //
            case OP_1NEGATE:
            case OP_1:
            case OP_2:
            case OP_3:
            case OP_4:
            case OP_5:
            case OP_6:
            case OP_7:
            case OP_8:
            case OP_9:
            case OP_10:
            case OP_11:
            case OP_12:
            case OP_13:
            case OP_14:
            case OP_15:
            case OP_16:
            {
                CBigNum bn((int)opcode - (int)(OP_1 - 1));
                stack.push_back(bn.getvch());
            }
            break;


            //
            // Control
            //
            case OP_NOP:
            break;

            case OP_VER:
            {
                if (flags & SCRIPT_VERIFY_EXEC)
                    return false;
                CBigNum bn(VERSION);
                stack.push_back(bn.getvch());
            }
            break;

            case OP_IF:
            case OP_NOTIF:
            case OP_VERIF:
            case OP_VERNOTIF:
            {
                bool fValue = false;
                if (fExec)
                {
                    if (stack.size() < 1)
                        return false;
                    valtype& vch = stacktop(-1);
                    if (opcode == OP_VERIF || opcode == OP_VERNOTIF)
                    {
                        if (flags & SCRIPT_VERIFY_EXEC)
                            return false;
                        fValue = (CBigNum(VERSION) >= CBigNum(vch));
                    }
                    else
                        fValue = CastToBool(vch);
                    if (opcode == OP_NOTIF || opcode == OP_VERNOTIF)
                        fValue = !fValue;
                    stack.pop_back();
                }
                vfExec.push_back(fValue);
            }
            break;

            case OP_ELSE:
            {
                if (vfExec.empty())
                    return false;
                vfExec.back() = !vfExec.back();
            }
            break;

            case OP_ENDIF:
            {
                if (vfExec.empty())
                    return false;
                vfExec.pop_back();
            }
            break;

            case OP_VERIFY:
            {
                if (stack.size() < 1)
                    return false;
                bool fValue = CastToBool(stacktop(-1));
                if (!fValue)
                    return false;
                stack.pop_back();
            }
            break;

            case OP_RETURN:
            {
                return false;
            }
            break;


            //
            // Stack ops
            //
            case OP_TOALTSTACK:
            {
                if (stack.size() < 1)
                    return false;
                altstack.push_back(stacktop(-1));
                stack.pop_back();
            }
            break;

            case OP_FROMALTSTACK:
            {
                if (altstack.size() < 1)
                    return false;
                stack.push_back(altstacktop(-1));
                altstack.pop_back();
            }
            break;

            case OP_2DROP:
            {
                if (stack.size() < 2)
                    return false;
                stack.pop_back();
                stack.pop_back();
            }
            break;

            case OP_2DUP:
            {
                if (stack.size() < 2)
                    return false;
                valtype vch1 = stacktop(-2);
                valtype vch2 = stacktop(-1);
                stack.push_back(vch1);
                stack.push_back(vch2);
            }
            break;

            case OP_3DUP:
            {
                if (stack.size() < 3)
                    return false;
                valtype vch1 = stacktop(-3);
                valtype vch2 = stacktop(-2);
                valtype vch3 = stacktop(-1);
                stack.push_back(vch1);
                stack.push_back(vch2);
                stack.push_back(vch3);
            }
            break;

            case OP_2OVER:
            {
                if (stack.size() < 4)
                    return false;
                valtype vch1 = stacktop(-4);
                valtype vch2 = stacktop(-3);
                stack.push_back(vch1);
                stack.push_back(vch2);
            }
            break;

            case OP_2ROT:
            {
                if (stack.size() < 6)
                    return false;
                valtype vch1 = stacktop(-6);
                valtype vch2 = stacktop(-5);
                stack.erase(stack.end()-6, stack.end()-4);
                stack.push_back(vch1);
                stack.push_back(vch2);
            }
            break;

            case OP_2SWAP:
            {
                if (stack.size() < 4)
                    return false;
                swap(stacktop(-4), stacktop(-2));
                swap(stacktop(-3), stacktop(-1));
            }
            break;

            case OP_IFDUP:
            {
                if (stack.size() < 1)
                    return false;
                valtype vch = stacktop(-1);
                if (CastToBool(vch))
                    stack.push_back(vch);
            }
            break;

            case OP_DEPTH:
            {
                CBigNum bn(stack.size());
                stack.push_back(bn.getvch());
            }
            break;

            case OP_DROP:
            {
                if (stack.size() < 1)
                    return false;
                stack.pop_back();
            }
            break;

            case OP_DUP:
            {
                if (stack.size() < 1)
                    return false;
                valtype vch = stacktop(-1);
                stack.push_back(vch);
            }
            break;

            case OP_NIP:
            {
                if (stack.size() < 2)
                    return false;
                stack.erase(stack.end() - 2);
            }
            break;

            case OP_OVER:
            {
                if (stack.size() < 2)
                    return false;
                valtype vch = stacktop(-2);
                stack.push_back(vch);
            }
            break;

            case OP_PICK:
            case OP_ROLL:
            {
                if (stack.size() < 2)
                    return false;
                CBigNum bnN;
                if (!CastToBigNum(stacktop(-1), bnN, flags))
                    return false;
                int n = bnN.getint();
                stack.pop_back();
                if (n < 0 || n >= (int)stack.size())
                    return false;
                valtype vch = stacktop(-n-1);
                if (opcode == OP_ROLL)
                    stack.erase(stack.end()-n-1);
                stack.push_back(vch);
            }
            break;

            case OP_ROT:
            {
                if (stack.size() < 3)
                    return false;
                swap(stacktop(-3), stacktop(-2));
                swap(stacktop(-2), stacktop(-1));
            }
            break;

            case OP_SWAP:
            {
                if (stack.size() < 2)
                    return false;
                swap(stacktop(-2), stacktop(-1));
            }
            break;

            case OP_TUCK:
            {
                if (stack.size() < 2)
                    return false;
                valtype vch = stacktop(-1);
                stack.insert(stack.end()-2, vch);
            }
            break;


            //
            // Splice ops
            //
            case OP_CAT:
            {
                if (stack.size() < 2)
                    return false;
                valtype& vch1 = stacktop(-2);
                valtype& vch2 = stacktop(-1);
                if ((flags & SCRIPT_VERIFY_EXEC) &&
                    (vch1.size() + vch2.size() > MAX_SCRIPT_ELEMENT_SIZE))
                    return false;
                vch1.insert(vch1.end(), vch2.begin(), vch2.end());
                stack.pop_back();
            }
            break;

            case OP_SUBSTR:
            {
                if (stack.size() < 3)
                    return false;
                valtype& vch = stacktop(-3);
                CBigNum bnBegin, bnSize;
                if (!CastToBigNum(stacktop(-2), bnBegin, flags))
                    return false;
                if (!CastToBigNum(stacktop(-1), bnSize, flags))
                    return false;
                int nBegin = bnBegin.getint();
                int nEnd = nBegin + bnSize.getint();
                if (nBegin < 0 || nEnd < nBegin)
                    return false;
                if (nBegin > (int)vch.size())
                    nBegin = vch.size();
                if (nEnd > (int)vch.size())
                    nEnd = vch.size();
                vch.erase(vch.begin() + nEnd, vch.end());
                vch.erase(vch.begin(), vch.begin() + nBegin);
                if ((flags & SCRIPT_VERIFY_EXEC) && vch.size() > MAX_SCRIPT_ELEMENT_SIZE)
                    return false;
                stack.pop_back();
                stack.pop_back();
            }
            break;

            case OP_LEFT:
            case OP_RIGHT:
            {
                if (stack.size() < 2)
                    return false;
                valtype& vch = stacktop(-2);
                CBigNum bnSize;
                if (!CastToBigNum(stacktop(-1), bnSize, flags))
                    return false;
                int nSize = bnSize.getint();
                if (nSize < 0)
                    return false;
                if (nSize > (int)vch.size())
                    nSize = vch.size();
                if (opcode == OP_LEFT)
                    vch.erase(vch.begin() + nSize, vch.end());
                else
                    vch.erase(vch.begin(), vch.end() - nSize);
                stack.pop_back();
            }
            break;

            case OP_SIZE:
            {
                if (stack.size() < 1)
                    return false;
                CBigNum bn(stacktop(-1).size());
                stack.push_back(bn.getvch());
            }
            break;


            //
            // Bitwise logic
            //
            case OP_INVERT:
            {
                if (stack.size() < 1)
                    return false;
                valtype& vch = stacktop(-1);
                for (int i = 0; i < (int)vch.size(); i++)
                    vch[i] = ~vch[i];
            }
            break;

            case OP_AND:
            case OP_OR:
            case OP_XOR:
            {
                if (stack.size() < 2)
                    return false;
                valtype& vch1 = stacktop(-2);
                valtype& vch2 = stacktop(-1);
                MakeSameSize(vch1, vch2);
                if (opcode == OP_AND)
                {
                    for (int i = 0; i < (int)vch1.size(); i++)
                        vch1[i] &= vch2[i];
                }
                else if (opcode == OP_OR)
                {
                    for (int i = 0; i < (int)vch1.size(); i++)
                        vch1[i] |= vch2[i];
                }
                else if (opcode == OP_XOR)
                {
                    for (int i = 0; i < (int)vch1.size(); i++)
                        vch1[i] ^= vch2[i];
                }
                stack.pop_back();
            }
            break;

            case OP_EQUAL:
            case OP_EQUALVERIFY:
            {
                if (stack.size() < 2)
                    return false;
                valtype& vch1 = stacktop(-2);
                valtype& vch2 = stacktop(-1);
                bool fEqual = (vch1 == vch2);
                stack.pop_back();
                stack.pop_back();
                stack.push_back(fEqual ? vchTrue : vchFalse);
                if (opcode == OP_EQUALVERIFY)
                {
                    if (fEqual)
                        stack.pop_back();
                    else
                        return false;
                }
            }
            break;


            //
            // Numeric
            //
            case OP_1ADD:
            case OP_1SUB:
            case OP_2MUL:
            case OP_2DIV:
            case OP_NEGATE:
            case OP_ABS:
            case OP_NOT:
            case OP_0NOTEQUAL:
            {
                if (stack.size() < 1)
                    return false;
                CBigNum bn;
                if (!CastToBigNum(stacktop(-1), bn, flags))
                    return false;
                switch (opcode)
                {
                case OP_1ADD:       bn += bnOne; break;
                case OP_1SUB:       bn -= bnOne; break;
                case OP_2MUL:       bn <<= 1; break;
                case OP_2DIV:       bn >>= 1; break;
                case OP_NEGATE:     bn = -bn; break;
                case OP_ABS:        if (bn < bnZero) bn = -bn; break;
                case OP_NOT:        bn = (bn == bnZero); break;
                case OP_0NOTEQUAL:  bn = (bn != bnZero); break;
                }
                stack.pop_back();
                valtype vchResult = bn.getvch();
                if ((flags & SCRIPT_VERIFY_EXEC) && (int)vchResult.size() > MAX_BIGNUM_SIZE)
                    return false;
                stack.push_back(vchResult);
            }
            break;

            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV:
            case OP_MOD:
            case OP_LSHIFT:
            case OP_RSHIFT:
            case OP_BOOLAND:
            case OP_BOOLOR:
            case OP_NUMEQUAL:
            case OP_NUMEQUALVERIFY:
            case OP_NUMNOTEQUAL:
            case OP_LESSTHAN:
            case OP_GREATERTHAN:
            case OP_LESSTHANOREQUAL:
            case OP_GREATERTHANOREQUAL:
            case OP_MIN:
            case OP_MAX:
            {
                if (stack.size() < 2)
                    return false;
                CBigNum bn1, bn2;
                if (!CastToBigNum(stacktop(-2), bn1, flags))
                    return false;
                if (!CastToBigNum(stacktop(-1), bn2, flags))
                    return false;
                CBigNum bn;
                switch (opcode)
                {
                case OP_ADD:
                    bn = bn1 + bn2;
                    break;

                case OP_SUB:
                    bn = bn1 - bn2;
                    break;

                case OP_MUL:
                    if (!BN_mul(bn.get(), bn1.get(), bn2.get(), pctx))
                        return false;
                    break;

                case OP_DIV:
                    if (bn2 == bnZero)
                        return false;
                    if (!BN_div(bn.get(), NULL, bn1.get(), bn2.get(), pctx))
                        return false;
                    break;

                case OP_MOD:
                    if (bn2 == bnZero)
                        return false;
                    if (!BN_mod(bn.get(), bn1.get(), bn2.get(), pctx))
                        return false;
                    break;

                case OP_LSHIFT:
                    if (bn2 < bnZero)
                        return false;
                    if ((flags & SCRIPT_VERIFY_EXEC) && bn2 > CBigNum(31))
                        return false;
                    bn = bn1 << bn2.getulong();
                    break;

                case OP_RSHIFT:
                    if (bn2 < bnZero)
                        return false;
                    if ((flags & SCRIPT_VERIFY_EXEC) && bn2 > CBigNum(31))
                        return false;
                    bn = bn1 >> bn2.getulong();
                    break;

                case OP_BOOLAND:             bn = (bn1 != bnZero && bn2 != bnZero); break;
                case OP_BOOLOR:              bn = (bn1 != bnZero || bn2 != bnZero); break;
                case OP_NUMEQUAL:            bn = (bn1 == bn2); break;
                case OP_NUMEQUALVERIFY:      bn = (bn1 == bn2); break;
                case OP_NUMNOTEQUAL:         bn = (bn1 != bn2); break;
                case OP_LESSTHAN:            bn = (bn1 < bn2); break;
                case OP_GREATERTHAN:         bn = (bn1 > bn2); break;
                case OP_LESSTHANOREQUAL:     bn = (bn1 <= bn2); break;
                case OP_GREATERTHANOREQUAL:  bn = (bn1 >= bn2); break;
                case OP_MIN:                 bn = (bn1 < bn2 ? bn1 : bn2); break;
                case OP_MAX:                 bn = (bn1 > bn2 ? bn1 : bn2); break;
                }
                stack.pop_back();
                stack.pop_back();
                valtype vchResult = bn.getvch();
                if ((flags & SCRIPT_VERIFY_EXEC) && (int)vchResult.size() > MAX_BIGNUM_SIZE)
                    return false;
                stack.push_back(vchResult);

                if (opcode == OP_NUMEQUALVERIFY)
                {
                    if (CastToBool(stacktop(-1)))
                        stack.pop_back();
                    else
                        return false;
                }
            }
            break;

            case OP_WITHIN:
            {
                if (stack.size() < 3)
                    return false;
                CBigNum bn1, bn2, bn3;
                if (!CastToBigNum(stacktop(-3), bn1, flags))
                    return false;
                if (!CastToBigNum(stacktop(-2), bn2, flags))
                    return false;
                if (!CastToBigNum(stacktop(-1), bn3, flags))
                    return false;
                bool fValue = (bn2 <= bn1 && bn1 < bn3);
                stack.pop_back();
                stack.pop_back();
                stack.pop_back();
                stack.push_back(fValue ? vchTrue : vchFalse);
            }
            break;


            //
            // Crypto
            //
            case OP_RIPEMD160:
            case OP_SHA1:
            case OP_SHA256:
            case OP_HASH160:
            case OP_HASH256:
            {
                if (stack.size() < 1)
                    return false;
                valtype& vch = stacktop(-1);
                valtype vchHash((opcode == OP_RIPEMD160 || opcode == OP_SHA1 || opcode == OP_HASH160) ? 20 : 32);
                if (opcode == OP_RIPEMD160)
                    RIPEMD160(&vch[0], vch.size(), &vchHash[0]);
                else if (opcode == OP_SHA1)
                    SHA1(&vch[0], vch.size(), &vchHash[0]);
                else if (opcode == OP_SHA256)
                    SHA256(&vch[0], vch.size(), &vchHash[0]);
                else if (opcode == OP_HASH160)
                {
                    uint160 hash160 = Hash160(vch);
                    memcpy(&vchHash[0], &hash160, sizeof(hash160));
                }
                else if (opcode == OP_HASH256)
                {
                    uint256 hash = Hash(vch.begin(), vch.end());
                    memcpy(&vchHash[0], &hash, sizeof(hash));
                }
                stack.pop_back();
                stack.push_back(vchHash);
            }
            break;

            case OP_CODESEPARATOR:
            {
                pbegincodehash = pc;
            }
            break;

            case OP_CHECKSIG:
            case OP_CHECKSIGVERIFY:
            {
                if (stack.size() < 2)
                    return false;

                valtype& vchSig    = stacktop(-2);
                valtype& vchPubKey = stacktop(-1);

                if (!CheckSignatureEncoding(vchSig, flags))
                    return false;

                CScript scriptCode(pbegincodehash, pend);
                scriptCode.FindAndDelete(CScript(vchSig));

                bool fSuccess = CheckSig(vchSig, vchPubKey, scriptCode, txTo, nIn, nHashType, flags);

                stack.pop_back();
                stack.pop_back();
                stack.push_back(fSuccess ? vchTrue : vchFalse);
                if (opcode == OP_CHECKSIGVERIFY)
                {
                    if (fSuccess)
                        stack.pop_back();
                    else
                        return false;
                }
            }
            break;

            case OP_CHECKMULTISIG:
            case OP_CHECKMULTISIGVERIFY:
            {
                int i = 1;
                if ((int)stack.size() < i)
                    return false;

                CBigNum bnKeys;
                if (!CastToBigNum(stacktop(-i), bnKeys, flags))
                    return false;
                int nKeysCount = bnKeys.getint();
                if (nKeysCount < 0 || nKeysCount > (int)MAX_PUBKEYS_PER_MULTISIG)
                    return false;
                nOpCount += nKeysCount;
                if ((flags & SCRIPT_VERIFY_EXEC) && nOpCount > (int)MAX_OPS_PER_SCRIPT)
                    return false;
                int ikey = ++i;
                i += nKeysCount;
                if ((int)stack.size() < i)
                    return false;

                CBigNum bnSigs;
                if (!CastToBigNum(stacktop(-i), bnSigs, flags))
                    return false;
                int nSigsCount = bnSigs.getint();
                if (nSigsCount < 0 || nSigsCount > nKeysCount)
                    return false;
                int isig = ++i;
                i += nSigsCount;
                if ((int)stack.size() < i)
                    return false;

                CScript scriptCode(pbegincodehash, pend);

                for (int k = 0; k < nSigsCount; k++)
                {
                    valtype& vchSig = stacktop(-isig-k);
                    scriptCode.FindAndDelete(CScript(vchSig));
                }

                bool fSuccess = true;
                while (fSuccess && nSigsCount > 0)
                {
                    valtype& vchSig    = stacktop(-isig);
                    valtype& vchPubKey = stacktop(-ikey);

                    if (!CheckSignatureEncoding(vchSig, flags))
                        return false;

                    if (CheckSig(vchSig, vchPubKey, scriptCode, txTo, nIn, nHashType, flags))
                    {
                        isig++;
                        nSigsCount--;
                    }
                    ikey++;
                    nKeysCount--;

                    if (nSigsCount > nKeysCount)
                        fSuccess = false;
                }

                if ((flags & SCRIPT_VERIFY_EXEC) && stacktop(-i).size() != 0)
                    return false;

                while (i-- > 0)
                    stack.pop_back();
                stack.push_back(fSuccess ? vchTrue : vchFalse);

                if (opcode == OP_CHECKMULTISIGVERIFY)
                {
                    if (fSuccess)
                        stack.pop_back();
                    else
                        return false;
                }
            }
            break;

            default:
                return false;
        }

        if ((flags & SCRIPT_VERIFY_EXEC) &&
            (stack.size() + altstack.size() > MAX_STACK_SIZE))
            return false;
    }

    if (!vfExec.empty())
        return false;

    }
    catch (...)
    {
        return false;
    }

    return true;
}

#undef top




bool VerifyScript(const CScript& scriptSig, const CScript& scriptPubKey,
                  const CTransaction& txTo, unsigned int nIn, int nHashType,
                  unsigned int flags)
{
    if ((flags & SCRIPT_VERIFY_EXEC) && !scriptSig.IsPushOnly())
        return false;

    vector<valtype> stack;
    if (!EvalScript(stack, scriptSig, txTo, nIn, nHashType, flags))
        return false;

    vector<valtype> stackCopy = stack;

    if (!EvalScript(stack, scriptPubKey, txTo, nIn, nHashType, flags))
        return false;

    if (stack.empty())
        return false;

    if (!CastToBool(stack.back()))
        return false;

    return true;
}




uint256 SignatureHash(CScript scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType)
{
    if (nIn >= txTo.vin.size())
    {
        printf("ERROR: SignatureHash() : nIn=%d out of range\n", nIn);
        return 1;
    }
    CTransaction txTmp(txTo);

    scriptCode.FindAndDelete(CScript(OP_CODESEPARATOR));

    for (int i = 0; i < (int)txTmp.vin.size(); i++)
        txTmp.vin[i].scriptSig = CScript();
    txTmp.vin[nIn].scriptSig = scriptCode;

    if ((nHashType & 0x1f) == SIGHASH_NONE)
    {
        txTmp.vout.clear();

        for (int i = 0; i < (int)txTmp.vin.size(); i++)
            if (i != (int)nIn)
                txTmp.vin[i].nSequence = 0;
    }
    else if ((nHashType & 0x1f) == SIGHASH_SINGLE)
    {
        unsigned int nOut = nIn;
        if (nOut >= txTmp.vout.size())
        {
            printf("ERROR: SignatureHash() : nOut=%d out of range\n", nOut);
            return 1;
        }
        txTmp.vout.resize(nOut+1);
        for (int i = 0; i < (int)nOut; i++)
            txTmp.vout[i].SetNull();

        for (int i = 0; i < (int)txTmp.vin.size(); i++)
            if (i != (int)nIn)
                txTmp.vin[i].nSequence = 0;
    }

    if (nHashType & SIGHASH_ANYONECANPAY)
    {
        txTmp.vin[0] = txTmp.vin[nIn];
        txTmp.vin.resize(1);
    }

    CDataStream ss(SER_GETHASH);
    ss.reserve(10000);
    ss << txTmp << nHashType;
    return Hash(ss.begin(), ss.end());
}


bool CheckSig(vector<unsigned char> vchSig, vector<unsigned char> vchPubKey, CScript scriptCode,
              const CTransaction& txTo, unsigned int nIn, int nHashType, unsigned int flags)
{
    CKey key;
    if (vchPubKey.empty())
        return false;
    if (!key.SetPubKey(vchPubKey))
        return false;

    if (vchSig.empty())
        return false;
    if (nHashType == 0)
        nHashType = vchSig.back();
    else if (nHashType != vchSig.back())
        return false;
    vchSig.pop_back();

    if ((flags & SCRIPT_VERIFY_EXEC) && (nHashType & 0x1f) == SIGHASH_SINGLE && nIn >= txTo.vout.size())
        return false;

    if (key.Verify(SignatureHash(scriptCode, txTo, nIn, nHashType), vchSig))
        return true;

    return false;
}




bool Solver(const CScript& scriptPubKey, vector<pair<opcodetype, valtype> >& vSolutionRet)
{
    static vector<CScript> vTemplates;
    if (vTemplates.empty())
    {
        vTemplates.push_back(CScript() << OP_PUBKEY << OP_CHECKSIG);
        vTemplates.push_back(CScript() << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG);
    }

    const CScript& script1 = scriptPubKey;
    foreach(const CScript& script2, vTemplates)
    {
        vSolutionRet.clear();
        opcodetype opcode1, opcode2;
        vector<unsigned char> vch1, vch2;

        CScript::const_iterator pc1 = script1.begin();
        CScript::const_iterator pc2 = script2.begin();
        loop
        {
            bool f1 = script1.GetOp(pc1, opcode1, vch1);
            bool f2 = script2.GetOp(pc2, opcode2, vch2);
            if (!f1 && !f2)
            {
                reverse(vSolutionRet.begin(), vSolutionRet.end());
                return true;
            }
            else if (f1 != f2)
            {
                break;
            }
            else if (opcode2 == OP_PUBKEY)
            {
                if (vch1.size() <= sizeof(uint256))
                    break;
                vSolutionRet.push_back(make_pair(opcode2, vch1));
            }
            else if (opcode2 == OP_PUBKEYHASH)
            {
                if (vch1.size() != sizeof(uint160))
                    break;
                vSolutionRet.push_back(make_pair(opcode2, vch1));
            }
            else if (opcode1 != opcode2)
            {
                break;
            }
        }
    }

    vSolutionRet.clear();
    return false;
}


bool Solver(const CScript& scriptPubKey, uint256 hash, int nHashType, CScript& scriptSigRet)
{
    scriptSigRet.clear();

    vector<pair<opcodetype, valtype> > vSolution;
    if (!Solver(scriptPubKey, vSolution))
        return false;

    CRITICAL_BLOCK(cs_mapKeys)
    {
        foreach(PAIRTYPE(opcodetype, valtype)& item, vSolution)
        {
            if (item.first == OP_PUBKEY)
            {
                const valtype& vchPubKey = item.second;
                if (!mapKeys.count(vchPubKey))
                    return false;
                if (hash != 0)
                {
                    vector<unsigned char> vchSig;
                    if (!CKey::Sign(mapKeys[vchPubKey], hash, vchSig))
                        return false;
                    vchSig.push_back((unsigned char)nHashType);
                    scriptSigRet << vchSig;
                }
            }
            else if (item.first == OP_PUBKEYHASH)
            {
                map<uint160, valtype>::iterator mi = mapPubKeys.find(uint160(item.second));
                if (mi == mapPubKeys.end())
                    return false;
                const vector<unsigned char>& vchPubKey = (*mi).second;
                if (!mapKeys.count(vchPubKey))
                    return false;
                if (hash != 0)
                {
                    vector<unsigned char> vchSig;
                    if (!CKey::Sign(mapKeys[vchPubKey], hash, vchSig))
                        return false;
                    vchSig.push_back((unsigned char)nHashType);
                    scriptSigRet << vchSig << vchPubKey;
                }
            }
        }
    }

    return true;
}


bool IsMine(const CScript& scriptPubKey)
{
    CScript scriptSig;
    return Solver(scriptPubKey, 0, 0, scriptSig);
}


bool ExtractPubKey(const CScript& scriptPubKey, bool fMineOnly, vector<unsigned char>& vchPubKeyRet)
{
    vchPubKeyRet.clear();

    vector<pair<opcodetype, valtype> > vSolution;
    if (!Solver(scriptPubKey, vSolution))
        return false;

    CRITICAL_BLOCK(cs_mapKeys)
    {
        foreach(PAIRTYPE(opcodetype, valtype)& item, vSolution)
        {
            valtype vchPubKey;
            if (item.first == OP_PUBKEY)
            {
                vchPubKey = item.second;
            }
            else if (item.first == OP_PUBKEYHASH)
            {
                map<uint160, valtype>::iterator mi = mapPubKeys.find(uint160(item.second));
                if (mi == mapPubKeys.end())
                    continue;
                vchPubKey = (*mi).second;
            }
            if (!fMineOnly || mapKeys.count(vchPubKey))
            {
                vchPubKeyRet = vchPubKey;
                return true;
            }
        }
    }
    return false;
}


bool ExtractHash160(const CScript& scriptPubKey, uint160& hash160Ret)
{
    hash160Ret = 0;

    vector<pair<opcodetype, valtype> > vSolution;
    if (!Solver(scriptPubKey, vSolution))
        return false;

    foreach(PAIRTYPE(opcodetype, valtype)& item, vSolution)
    {
        if (item.first == OP_PUBKEYHASH)
        {
            hash160Ret = uint160(item.second);
            return true;
        }
    }
    return false;
}


bool SignSignature(const CTransaction& txFrom, CTransaction& txTo, unsigned int nIn, int nHashType, CScript scriptPrereq)
{
    assert(nIn < txTo.vin.size());
    CTxIn& txin = txTo.vin[nIn];
    assert(txin.prevout.n < txFrom.vout.size());
    const CTxOut& txout = txFrom.vout[txin.prevout.n];

    uint256 hash = SignatureHash(scriptPrereq + txout.scriptPubKey, txTo, nIn, nHashType);

    if (!Solver(txout.scriptPubKey, hash, nHashType, txin.scriptSig))
        return false;

    txin.scriptSig = scriptPrereq + txin.scriptSig;

    if (scriptPrereq.empty())
    {
        unsigned int nVerifyFlags = SCRIPT_VERIFY_NONE;
        if (nBestHeight + 1 >= SCRIPT_EXEC_ACTIVATION_HEIGHT)
            nVerifyFlags |= SCRIPT_VERIFY_EXEC;
        if (!VerifyScript(txin.scriptSig, txout.scriptPubKey, txTo, nIn, nHashType, nVerifyFlags))
            return false;
    }

    return true;
}


bool VerifySignature(const CTransaction& txFrom, const CTransaction& txTo, unsigned int nIn, int nHashType, unsigned int flags)
{
    assert(nIn < txTo.vin.size());
    const CTxIn& txin = txTo.vin[nIn];
    if (txin.prevout.n >= txFrom.vout.size())
        return false;
    const CTxOut& txout = txFrom.vout[txin.prevout.n];

    if (txin.prevout.hash != txFrom.GetHash())
        return false;

    if (!VerifyScript(txin.scriptSig, txout.scriptPubKey, txTo, nIn, nHashType, flags))
        return false;

    WalletUpdateSpent(txin.prevout);

    return true;
}
