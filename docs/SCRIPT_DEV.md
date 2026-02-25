# Bitok Script Developer Guide

Complete reference for building, deploying, and spending custom script contracts on Bitok.

**Script Exec activation:** Block 18,000
**Current status:** Active

---

## Table of Contents

1. [What Bitok Is and Is Not](#1-what-bitok-is-and-is-not)
2. [How Scripts Work](#2-how-scripts-work)
3. [Opcode Reference](#3-opcode-reference)
4. [Execution Limits](#4-execution-limits)
5. [Evaluation Model](#5-evaluation-model)
6. [Signature Hashing](#6-signature-hashing)
7. [Transaction Structure](#7-transaction-structure)
8. [Fees and Dust](#8-fees-and-dust)
9. [Standardness](#9-standardness)
10. [RPC Toolkit](#10-rpc-toolkit)
11. [Script Classification](#11-script-classification)
12. [Number Encoding](#12-number-encoding)
13. [Building Scripts](#13-building-scripts)
14. [Complete Workflows](#14-complete-workflows)
15. [Contract Patterns](#15-contract-patterns)
16. [Debugging](#16-debugging)
17. [Quick Reference Tables](#17-quick-reference-tables)

---

## 1. What Bitok Is and Is Not

Bitok is a modified Bitcoin 0.3.19. Know the boundaries before writing scripts.

**Bitok HAS:**

- All original opcodes **enabled** -- OP_CAT, OP_MUL, OP_DIV, OP_AND, OP_OR, OP_LSHIFT, OP_SUBSTR, OP_INVERT, OP_XOR, OP_2MUL, OP_2DIV, OP_MOD, OP_RSHIFT, OP_LEFT, OP_RIGHT -- everything
- Bounded execution with deterministic limits (post block 18,000)
- Separated evaluation -- scriptSig is push-only, scriptPubKey runs the logic
- nLockTime on transactions (block height gating or Unix timestamp gating)
- All six sighash types: ALL, NONE, SINGLE, with or without ANYONECANPAY
- Full raw transaction RPC toolkit including script building and verification tools
- Permissive standardness -- ANY parseable script is relayed post-activation

**Bitok does NOT have:**

- No P2SH (no BIP16, no redeemScript, no script hash addresses)
- No SegWit (no witness data, no bech32 addresses)
- No OP_CHECKLOCKTIMEVERIFY (use nLockTime on the transaction itself)
- No OP_CHECKSEQUENCEVERIFY (no relative timelocks)
- No Taproot, no Schnorr signatures
- No post-2010 Bitcoin soft forks as separate features

If you are coming from Bitcoin development: there is no P2SH wrapping, no witness program, no address versioning beyond the original format. What you write in the scriptPubKey is what gets executed directly.

---

## 2. How Scripts Work

Every output locks coins with a **scriptPubKey**. To spend, you provide a **scriptSig**.

### Separated Evaluation (post block 18,000)

1. **scriptSig executes first.** It must contain **only push operations**. No OP_DUP, no OP_ADD, nothing except data pushes. This produces a data stack.
2. **scriptPubKey executes second** using the stack from step 1.
3. **Verification:** Stack must be non-empty. Top element must be truthy. Otherwise the spend fails.

scriptSig delivers data. scriptPubKey contains all logic. The scriptSig cannot influence control flow.

### Truth and Falsity

A value is **false** if:
- It is empty (zero-length byte vector)
- All bytes are `0x00`
- All bytes are `0x00` except the last byte is `0x80` (negative zero)

Everything else is **true**.

---

## 3. Opcode Reference

### Push Operations

These put data on the stack. They do NOT count toward the 201 opcode limit.

| Opcode | Hex | What It Does |
|--------|-----|--------------|
| `OP_0` / `OP_FALSE` | `0x00` | Push empty byte vector (false) |
| Direct push 1-75 | `0x01`-`0x4b` | Push next N bytes (N = opcode value) |
| `OP_PUSHDATA1` | `0x4c` | Read 1-byte length, push that many bytes |
| `OP_PUSHDATA2` | `0x4d` | Read 2-byte LE length, push that many bytes |
| `OP_PUSHDATA4` | `0x4e` | Read 4-byte LE length, push that many bytes |
| `OP_1NEGATE` | `0x4f` | Push number -1 |
| `OP_1` through `OP_16` | `0x51`-`0x60` | Push number 1 through 16 |

**Minimal push encoding is enforced.** You must use the shortest encoding. Empty data = `OP_0`. Single byte 1-16 = `OP_1`-`OP_16`. Byte `0x81` = `OP_1NEGATE`. Data 1-75 bytes = direct push. Data 76-255 = `OP_PUSHDATA1`. Data 256-65535 = `OP_PUSHDATA2`. Non-minimal encoding causes script failure.

### Control Flow

| Opcode | Hex | What It Does |
|--------|-----|--------------|
| `OP_NOP` | `0x61` | Nothing |
| `OP_IF` | `0x63` | Pop top. Execute following block if true. |
| `OP_NOTIF` | `0x64` | Pop top. Execute following block if false. |
| `OP_ELSE` | `0x67` | Toggle current IF branch |
| `OP_ENDIF` | `0x68` | End IF/ELSE block |
| `OP_VERIFY` | `0x69` | Pop top. If false: script fails. If true: continue. |
| `OP_RETURN` | `0x6a` | Script fails immediately. Output is unspendable. |

**Disabled post-activation:** `OP_VER` (`0x62`), `OP_VERIF` (`0x65`), `OP_VERNOTIF` (`0x66`). Encountering these = immediate failure.

**Reserved:** `OP_RESERVED` (`0x50`), `OP_RESERVED1` (`0x89`), `OP_RESERVED2` (`0x8a`). Fail if executed, but can exist in unexecuted IF branches.

### Stack Manipulation

| Opcode | Hex | Before -> After | Min Stack |
|--------|-----|-----------------|-----------|
| `OP_TOALTSTACK` | `0x6b` | `x` -> alt: `x` | 1 |
| `OP_FROMALTSTACK` | `0x6c` | alt: `x` -> `x` | alt: 1 |
| `OP_2DROP` | `0x6d` | `a b` -> (empty) | 2 |
| `OP_2DUP` | `0x6e` | `a b` -> `a b a b` | 2 |
| `OP_3DUP` | `0x6f` | `a b c` -> `a b c a b c` | 3 |
| `OP_2OVER` | `0x70` | `a b c d` -> `a b c d a b` | 4 |
| `OP_2ROT` | `0x71` | `a b c d e f` -> `c d e f a b` | 6 |
| `OP_2SWAP` | `0x72` | `a b c d` -> `c d a b` | 4 |
| `OP_IFDUP` | `0x73` | `x` -> `x x` if truthy, else `x` | 1 |
| `OP_DEPTH` | `0x74` | -> pushes stack size | 0 |
| `OP_DROP` | `0x75` | `x` -> (empty) | 1 |
| `OP_DUP` | `0x76` | `x` -> `x x` | 1 |
| `OP_NIP` | `0x77` | `a b` -> `b` | 2 |
| `OP_OVER` | `0x78` | `a b` -> `a b a` | 2 |
| `OP_PICK` | `0x79` | `... xn ... x0 n` -> `... xn ... x0 xn` | 2 |
| `OP_ROLL` | `0x7a` | `... xn ... x0 n` -> `... x0 xn` | 2 |
| `OP_ROT` | `0x7b` | `a b c` -> `b c a` | 3 |
| `OP_SWAP` | `0x7c` | `a b` -> `b a` | 2 |
| `OP_TUCK` | `0x7d` | `a b` -> `b a b` | 2 |

### Splice Operations (String/Byte Manipulation)

All enabled. Operate on raw byte vectors.

| Opcode | Hex | Stack | Notes |
|--------|-----|-------|-------|
| `OP_CAT` | `0x7e` | `a b` -> `a\|\|b` | Result must be <= 520 bytes |
| `OP_SUBSTR` | `0x7f` | `str begin size` -> `str[begin..begin+size]` | Bounds clamped to str length |
| `OP_LEFT` | `0x80` | `str n` -> first n bytes | n < 0 fails, clamped to length |
| `OP_RIGHT` | `0x81` | `str n` -> last n bytes | n < 0 fails, clamped to length |
| `OP_SIZE` | `0x82` | `str` -> `str length` | Does NOT consume str |

### Bitwise Logic

All enabled. Byte-wise operations. Shorter operand is zero-padded to match longer.

| Opcode | Hex | Stack | Result |
|--------|-----|-------|--------|
| `OP_INVERT` | `0x83` | `x` | Flip every bit |
| `OP_AND` | `0x84` | `a b` | Byte-wise AND |
| `OP_OR` | `0x85` | `a b` | Byte-wise OR |
| `OP_XOR` | `0x86` | `a b` | Byte-wise XOR |
| `OP_EQUAL` | `0x87` | `a b` | Push 1 if identical, 0 if not |
| `OP_EQUALVERIFY` | `0x88` | `a b` | Fail if not equal, consume if equal |

### Arithmetic

All enabled. Operands: little-endian signed magnitude, max 4 bytes. Results must fit in 4 bytes.

| Opcode | Hex | Stack | Result |
|--------|-----|-------|--------|
| `OP_1ADD` | `0x8b` | `a` | `a + 1` |
| `OP_1SUB` | `0x8c` | `a` | `a - 1` |
| `OP_2MUL` | `0x8d` | `a` | `a * 2` |
| `OP_2DIV` | `0x8e` | `a` | `a / 2` |
| `OP_NEGATE` | `0x8f` | `a` | `-a` |
| `OP_ABS` | `0x90` | `a` | `|a|` |
| `OP_NOT` | `0x91` | `a` | `1` if a==0, else `0` |
| `OP_0NOTEQUAL` | `0x92` | `a` | `0` if a==0, else `1` |
| `OP_ADD` | `0x93` | `a b` | `a + b` |
| `OP_SUB` | `0x94` | `a b` | `a - b` |
| `OP_MUL` | `0x95` | `a b` | `a * b` |
| `OP_DIV` | `0x96` | `a b` | `a / b` -- b=0 fails |
| `OP_MOD` | `0x97` | `a b` | `a mod b` -- b=0 fails |
| `OP_LSHIFT` | `0x98` | `a b` | `a << b` -- b<0 fails, b>31 fails |
| `OP_RSHIFT` | `0x99` | `a b` | `a >> b` -- b<0 fails, b>31 fails |
| `OP_BOOLAND` | `0x9a` | `a b` | `1` if both nonzero |
| `OP_BOOLOR` | `0x9b` | `a b` | `1` if either nonzero |
| `OP_NUMEQUAL` | `0x9c` | `a b` | `1` if a==b |
| `OP_NUMEQUALVERIFY` | `0x9d` | `a b` | Fail if a!=b |
| `OP_NUMNOTEQUAL` | `0x9e` | `a b` | `1` if a!=b |
| `OP_LESSTHAN` | `0x9f` | `a b` | `1` if a<b |
| `OP_GREATERTHAN` | `0xa0` | `a b` | `1` if a>b |
| `OP_LESSTHANOREQUAL` | `0xa1` | `a b` | `1` if a<=b |
| `OP_GREATERTHANOREQUAL` | `0xa2` | `a b` | `1` if a>=b |
| `OP_MIN` | `0xa3` | `a b` | Smaller value |
| `OP_MAX` | `0xa4` | `a b` | Larger value |
| `OP_WITHIN` | `0xa5` | `x min max` | `1` if min <= x < max |

### Cryptographic Operations

| Opcode | Hex | Stack | Output |
|--------|-----|-------|--------|
| `OP_RIPEMD160` | `0xa6` | `data` | RIPEMD160(data) -- 20 bytes |
| `OP_SHA1` | `0xa7` | `data` | SHA1(data) -- 20 bytes |
| `OP_SHA256` | `0xa8` | `data` | SHA256(data) -- 32 bytes |
| `OP_HASH160` | `0xa9` | `data` | RIPEMD160(SHA256(data)) -- 20 bytes |
| `OP_HASH256` | `0xaa` | `data` | SHA256(SHA256(data)) -- 32 bytes |
| `OP_CODESEPARATOR` | `0xab` | -- | Marks code boundary for sig hashing |
| `OP_CHECKSIG` | `0xac` | `sig pubkey` | `1` if valid, `0` if not |
| `OP_CHECKSIGVERIFY` | `0xad` | `sig pubkey` | Fail if invalid |
| `OP_CHECKMULTISIG` | `0xae` | `OP_0 sig..sig M pub..pub N` | `1` if M-of-N valid |
| `OP_CHECKMULTISIGVERIFY` | `0xaf` | Same | Fail if not valid |

**OP_CHECKMULTISIG details:**

Reads N (pubkey count) from stack top. Pops N pubkeys. Reads M (required sigs). Pops M signatures. Pops a mandatory **dummy element** (must be empty post-activation -- this is the historical off-by-one bug preserved as consensus).

Signatures must match pubkeys in left-to-right order. Each sig is tested against remaining pubkeys sequentially. A failed match advances to the next pubkey. No backtracking.

The key count N is added to the opcode counter. A 3-of-5 multisig costs 6 opcodes (5 keys + 1 for the opcode itself).

---

## 4. Execution Limits

All enforced post-activation (block >= 18,000) via the `SCRIPT_VERIFY_EXEC` flag.

| Limit | Value | Failure |
|-------|-------|---------|
| Script size | 10,000 bytes | Rejected before execution |
| Stack + altstack | 1,000 elements combined | Fails after each instruction |
| Single element | 520 bytes | Push rejected; OP_CAT/SUBSTR result rejected |
| Opcodes per script | 201 non-push opcodes | Fails when count exceeded |
| Numeric operand | 4 bytes max | Arithmetic fails on oversized input |
| Shift amount | 0-31 | OP_LSHIFT/OP_RSHIFT fail outside range |
| Sigops per block | 20,000 | Block rejected |
| Pubkeys per CHECKMULTISIG | 20 | Script fails |

**What counts toward 201:** Every opcode with value > `OP_16` (`0x60`). Push operations do NOT count: `OP_0`, direct pushes (`0x01`-`0x4b`), `OP_PUSHDATA1/2/4`, `OP_1NEGATE`, `OP_1` through `OP_16`.

**Sigops counting:** OP_CHECKSIG = 1. OP_CHECKSIGVERIFY = 1. OP_CHECKMULTISIG = N (from the preceding OP_1..OP_16, or 20 if not preceded by one). OP_CHECKMULTISIGVERIFY = same as CHECKMULTISIG.

---

## 5. Evaluation Model

### VerifyScript (consensus entry point)

```
VerifyScript(scriptSig, scriptPubKey, transaction, inputIndex, hashType, flags)
```

1. **Push-only check:** scriptSig must be push-only (under EXEC). If not, fail.
2. **Evaluate scriptSig:** `EvalScript(stack, scriptSig, ...)`. If error, fail.
3. **Evaluate scriptPubKey:** `EvalScript(stack, scriptPubKey, ...)` with the stack from step 2. If error, fail.
4. **Final check:** Stack must be non-empty. Top must be truthy. Otherwise fail.

No P2SH step. No redeemScript unwrapping. The scriptPubKey is the final word.

### EvalScript (bytecode interpreter)

Pre-execution: script size <= 10,000 bytes (under EXEC).

Per-instruction loop:
1. Decode opcode via `GetOp`
2. Inside unexecuted IF branches: only IF/NOTIF/ELSE/ENDIF processed, everything else skipped
3. Enforce: push data <= 520, opcode count <= 201, minimal push encoding (under EXEC)
4. Execute the opcode
5. Check: stack + altstack <= 1,000 (under EXEC)

After loop: IF/ELSE/ENDIF must be balanced. Any exception returns false.

---

## 6. Signature Hashing

When OP_CHECKSIG runs, it computes a transaction digest and verifies the signature against it.

### Computation Steps

1. Copy the spending transaction
2. Strip OP_CODESEPARATOR from the scriptCode (scriptPubKey from last OP_CODESEPARATOR onward)
3. Clear all input scriptSigs
4. Set signing input's scriptSig = scriptCode
5. Apply sighash type modifications (see table)
6. Serialize modified transaction + 4-byte hash type (little-endian)
7. Double SHA256 of the serialized data

### Sighash Types

| Type | Byte | Inputs | Outputs |
|------|------|--------|---------|
| ALL | `0x01` | All inputs, sequences preserved | All outputs |
| NONE | `0x02` | This input only, others' sequences zeroed | None (cleared) |
| SINGLE | `0x03` | This input only, others' sequences zeroed | Only output at same index |
| ALL\|ANYONECANPAY | `0x81` | Only this input (others removed) | All outputs |
| NONE\|ANYONECANPAY | `0x82` | Only this input (others removed) | None |
| SINGLE\|ANYONECANPAY | `0x83` | Only this input (others removed) | Output at same index |

**SIGHASH_SINGLE edge case:** If input index >= number of outputs, CheckSig returns false immediately.

### Signature Format in scriptSig

```
[DER-encoded ECDSA signature] [1 byte: hash type]
```

DER format: `30 [len] 02 [r_len] [R] 02 [s_len] [S]`

Post-activation enforcement:
- Strict DER encoding
- Low-S: S must be <= half the secp256k1 order (`0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF5D576E7357A4501DDFE92F46681B20A0`)
- Hash type byte must be a valid sighash type

### Public Key Formats

- Compressed: 33 bytes, prefix `0x02` (even Y) or `0x03` (odd Y)
- Uncompressed: 65 bytes, prefix `0x04`

---

## 7. Transaction Structure

### Wire Format

```
[4 bytes]   nVersion        (int32 LE, always 1)
[varint]    input count
  FOR EACH INPUT:
    [32 bytes]  prev txid     (uint256, internal byte order)
    [4 bytes]   prev vout     (uint32 LE)
    [varint]    scriptSig length
    [N bytes]   scriptSig
    [4 bytes]   nSequence     (uint32 LE, default 0xFFFFFFFF)
[varint]    output count
  FOR EACH OUTPUT:
    [8 bytes]   value         (int64 LE, in satoshis)
    [varint]    scriptPubKey length
    [N bytes]   scriptPubKey
[4 bytes]   nLockTime       (uint32 LE)
```

### Varint Encoding

| Value | Encoding |
|-------|----------|
| 0-252 | 1 byte directly |
| 253-65535 | `0xFD` + 2 bytes LE |
| 65536-4294967295 | `0xFE` + 4 bytes LE |
| Larger | `0xFF` + 8 bytes LE |

### nLockTime

- 0: No lock, transaction is final immediately
- 1 to 499,999,999: Block height -- tx invalid until chain reaches this height
- 500,000,000+: Unix timestamp -- tx invalid until this time

For nLockTime to be enforced, at least one input must have nSequence != `0xFFFFFFFF`. Setting nSequence to 0 is the standard way.

### Coinbase

A coinbase transaction has exactly 1 input with `prevout.hash = 0` and `prevout.n = 0xFFFFFFFF`. The scriptSig contains the block height and miner-chosen data (2-100 bytes). Coinbase outputs cannot be spent for 100 blocks.

---

## 8. Fees and Dust

### Post-Activation Fee Calculation

```
base_fee = ceil(tx_bytes / 1000) * 0.01 BITOK
```

0.01 BITOK (1,000,000 satoshis) per started kilobyte.

### Free Transactions

Both conditions must be met:
1. Transaction size < 1,000 bytes
2. Priority >= 57,600,000

Priority formula:
```
priority = sum(input_value_satoshis * input_confirmations) / tx_size_bytes
```

Example: One input worth 1 BITOK with 576 confirmations, tx size 250 bytes:
```
priority = 100000000 * 576 / 250 = 230,400,000 (passes threshold)
```

### Dust Threshold

Outputs below 0.01 BITOK (1,000,000 satoshis) are dust. Transactions with dust outputs always require at least 0.01 BITOK fee, even if they would otherwise qualify as free.

### Practical Fee Guidance

| Transaction Type | Typical Size | Minimum Fee |
|-----------------|-------------|-------------|
| Simple P2PKH (1 in, 2 out) | ~226 bytes | 0.01 BITOK |
| Custom script spend | varies | 0.01 per KB |
| OP_RETURN data embed | ~250 bytes | 0.01 BITOK |

Never create outputs below 0.01 BITOK.

---

## 9. Standardness

Post-activation, standardness is **permissive**:

1. `nVersion` must be 1
2. Every input scriptSig: push-only, size <= 10,000 bytes
3. Every output scriptPubKey: non-empty, parseable (all opcodes decode), size <= 10,000 bytes

That is all. **Any valid parseable script is standard and will be relayed.** There is no whitelist. Arithmetic puzzles, OP_CAT covenants, bitwise masks, splice operations -- all relay without restriction.

This is a fundamental difference from Bitcoin where non-template scripts are not relayed.

---

## 10. RPC Toolkit

### Building Scripts

**buildscript** -- Assemble script from opcode names and hex data.

```
buildscript ["element", ...]
```

Each element: opcode name (`"OP_DUP"`, `"OP_CHECKSIG"`), numeric (`"0"`-`"16"`, `"-1"`), or hex data to push (`"deadbeef"`).

```bash
bitokd buildscript '["OP_ADD","OP_5","OP_EQUAL"]'
# { "hex": "935587", "asm": "OP_ADD OP_5 OP_EQUAL", "type": "arithmetic", "withinLimits": true }
```

**createmultisig** -- Build M-of-N multisig script.

```
createmultisig <nrequired> ["pubkey1","pubkey2",...]
```

Max 20 keys. Returns hex, asm, type, addresses.

---

### Transaction Construction

**createrawtransaction** -- Create unsigned transaction.

```
createrawtransaction [{"txid":"id","vout":n}, ...] {"destination":amount, ...} [locktime]
```

Destination key determines output type:
- **Bitok address** (`"1ABC..."`) -- P2PKH output
- **`"data"`** -- OP_RETURN output (amount must be `0`, value is hex data)
- **Hex string** -- used directly as raw scriptPubKey bytes

The third form is how you fund custom scripts:
```bash
bitokd createrawtransaction \
  '[{"txid":"abc...","vout":0}]' \
  '{"935587":1.0, "1ChangeAddr...":0.489}'
```

Optional per-input sequence and locktime:
```bash
bitokd createrawtransaction \
  '[{"txid":"abc...","vout":0,"sequence":0}]' \
  '{"1Dest...":1.0}' \
  25000
```

**signrawtransaction** -- Sign transaction inputs.

```
signrawtransaction <hex> [prevtxs] [privkeys] [sighashtype]
```

- `prevtxs`: `[{"txid":"...","vout":N,"scriptPubKey":"hex"}, ...]` for outputs not on chain yet
- `privkeys`: array of WIF-encoded private keys (if omitted, uses wallet keys)
- `sighashtype`: `"ALL"` (default), `"NONE"`, `"SINGLE"`, `"ALL|ANYONECANPAY"`, `"NONE|ANYONECANPAY"`, `"SINGLE|ANYONECANPAY"`

Auto-signs: P2PKH, P2PK, multisig (with partial sig merging), hashlock, hashlock-sha256 (if preimage registered). Returns `{"hex":"...", "complete": true/false}`.

For custom scripts that `signrawtransaction` cannot auto-sign, use `setscriptsig`.

**setscriptsig** -- Inject custom scriptSig into a transaction input.

```
setscriptsig <tx hex> <input index> <scriptsig>
```

The scriptSig argument can be:
- **Array:** `["03","02"]` -- each element push-encoded
- **Raw hex:** `"01030102"` -- bytes used directly
- **Space-separated:** `"03 02"` -- parsed as hex data pushes

```bash
# Spend arithmetic puzzle: push 3, push 2
bitokd setscriptsig "$TX" 0 '["03","02"]'
```

Returns modified transaction hex with the scriptSig set.

**sendrawtransaction** -- Broadcast signed transaction.

```
sendrawtransaction <hex>
```

Node validates everything (structure, fees, script verification) before accepting into mempool and relaying. Returns txid.

---

### Signature Computation

**getscriptsighash** -- Get the sighash digest for manual signing.

```
getscriptsighash <tx hex> <input index> <scriptpubkey hex> [sighashtype]
```

Returns the 32-byte hash that OP_CHECKSIG checks. Use for external/offline signing.

```bash
bitokd getscriptsighash "$UNSIGNED_TX" 0 "76a914...88ac" "ALL"
# { "sighash": "a1b2c3d4...", "hashTypeName": "ALL" }
```

---

### Script Analysis

**decodescript** -- Disassemble and classify.

```
decodescript <hex>
```

Returns: `asm`, `hex`, `type`, `reqSigs`, `addresses`.

**analyzescript** -- Deep static analysis.

```
analyzescript <hex>
```

Returns: asm, type, opcode counts by category (crypto, stack, flow, arithmetic, splice, bitwise), sigops, limit usage (`"size": "25/10000"`, `"opcodes": "4/201"`, etc.), `withinLimits`.

**validatescript** -- Sandbox execution with pre-loaded stack.

```
validatescript <script hex> [stack] [flags]
```

- `stack`: array of hex values. First element = bottom of stack, last = top.
- `flags`: `"exec"` (default) or `"legacy"`
- OP_CHECKSIG fails here (no transaction context). Use `verifyscriptpair` for that.

```bash
# Test: 3 + 2 == 5
bitokd validatescript "935587" '["03","02"]'
# { "valid": true, "finalStack": ["01"] }

# Test: OP_CAT then OP_EQUAL
bitokd validatescript "7e87" '["dead","beef","deadbeef"]'
```

Returns: `valid` (execution succeeded AND top truthy), `success` (EvalScript returned true), `finalStack`, `warnings`.

**decodescriptsig** -- Decode scriptSig in context of its scriptPubKey.

```
decodescriptsig <scriptsig hex> <scriptpubkey hex>
```

Labels each push element with its role based on script type (signature, pubkey, preimage, push_N, etc.). Reports `isPushOnly`.

**verifyscriptpair** -- Full script verification with real transaction context.

```
verifyscriptpair <tx hex> <input index> <scriptpubkey hex> [flags]
```

Unlike `validatescript`, this runs `VerifyScript()` with the actual transaction, so OP_CHECKSIG works.

Returns `verified: true/false`. On failure, `diagnostics` array with specific reasons.

---

### Transaction Inspection

**getrawtransaction** -- Fetch raw transaction.

```
getrawtransaction <txid> [verbose=0]
```

`verbose=0`: hex. `verbose=1`: full JSON (inputs, outputs, block info, confirmations).

**decoderawtransaction** -- Parse raw transaction hex.

```
decoderawtransaction <hex>
```

Full JSON breakdown of all fields.

**getrawmempool** -- List unconfirmed transactions.

```
getrawmempool
```

---

### Wallet Helpers

**addmultisigaddress** -- Add multisig to wallet for tracking.

```
addmultisigaddress <nrequired> ["key",...] [label]
```

Keys can be hex pubkeys or Bitok addresses (resolved from wallet).

**addpreimage** -- Register hash preimage for hashlock auto-signing.

```
addpreimage <hex>
```

Stores HASH160 and SHA256 mappings. Max 520 bytes. After registration, `signrawtransaction` auto-signs hashlock outputs locked to this preimage.

**listpreimages** -- Show all registered preimages.

```
listpreimages
```

---

### Utility

**listunspent** -- Wallet UTXOs.

```
listunspent [minconf=1] [maxconf=9999999]
```

**dumpprivkey / importprivkey** -- Export/import WIF private keys.

**getaddressutxos / getaddressbalance / getaddresstxids** -- Address queries (requires `-indexer` flag).

---

## 11. Script Classification

`analyzescript` and `decodescript` classify scripts into these types. Classification is informational -- it has **no effect** on consensus or relay.

| Type | Pattern |
|------|---------|
| `pubkeyhash` | `OP_DUP OP_HASH160 <20> OP_EQUALVERIFY OP_CHECKSIG` |
| `pubkey` | `<compressed_pubkey> OP_CHECKSIG` |
| `multisig` | `OP_M <pubkeys> OP_N OP_CHECKMULTISIG` |
| `nulldata` | `OP_RETURN [data]` |
| `hashlock` | `OP_HASH160 <20> OP_EQUALVERIFY OP_DUP OP_HASH160 <20> OP_EQUALVERIFY OP_CHECKSIG` |
| `hashlock-sha256` | `OP_SHA256 <32> OP_EQUALVERIFY OP_DUP OP_HASH160 <20> OP_EQUALVERIFY OP_CHECKSIG` |
| `arithmetic` | Contains OP_ADD, OP_MUL, OP_DIV, OP_SUB, OP_MOD, OP_LSHIFT, etc. |
| `bitwise` | Contains OP_AND, OP_OR, OP_XOR, OP_INVERT (no signature) |
| `bitwise-sig` | Bitwise ops + OP_CHECKSIG |
| `cat-covenant` | OP_CAT + hash op + OP_CHECKSIG |
| `cat-hash` | OP_CAT + hash op (no signature) |
| `cat-script` | OP_CAT without hash or signature |
| `splice` | OP_SUBSTR, OP_LEFT, or OP_RIGHT |
| `nonstandard` | Everything else |

---

## 12. Number Encoding

Arithmetic opcodes use **little-endian signed magnitude**. Maximum 4 bytes.

The high bit of the most significant byte is the **sign bit**. If the magnitude would set this bit, an extra byte is needed.

| Number | Hex Encoding | How to Push |
|--------|-------------|-------------|
| 0 | (empty) | `OP_0` |
| 1 | -- | `OP_1` |
| -1 | -- | `OP_1NEGATE` |
| 2 through 16 | -- | `OP_2` through `OP_16` |
| 17 | `11` | push 1 byte |
| 127 | `7f` | push 1 byte |
| 128 | `8000` | push 2 bytes (sign bit would be set) |
| 255 | `ff00` | push 2 bytes |
| 256 | `0001` | push 2 bytes |
| -5 | `85` | push 1 byte (high bit = sign) |
| -128 | `8080` | push 2 bytes |
| -256 | `0081` | push 2 bytes |
| 32767 | `ff7f` | push 2 bytes |
| 32768 | `008000` | push 3 bytes |
| 2147483647 | `ffffff7f` | push 4 bytes (maximum) |
| -2147483647 | `ffffffff` | push 4 bytes (minimum) |

**Minimal encoding is enforced.** Leading zero bytes are invalid unless needed for the sign bit. `0x0500` is invalid for the number 5 (use `0x05`). `0x0080` is valid for 128 (the `0x00` disambiguates the sign bit).

---

## 13. Building Scripts

### Using buildscript (recommended)

```bash
# Arithmetic: x + y == 5
bitokd buildscript '["OP_ADD","OP_5","OP_EQUAL"]'
# hex: 935587

# P2PKH
bitokd buildscript '["OP_DUP","OP_HASH160","<20-byte-pubkeyhash>","OP_EQUALVERIFY","OP_CHECKSIG"]'

# Hashlock (HASH160 preimage + signature)
bitokd buildscript '["OP_HASH160","<20-byte-hash>","OP_EQUALVERIFY","OP_DUP","OP_HASH160","<20-byte-pubkeyhash>","OP_EQUALVERIFY","OP_CHECKSIG"]'

# OP_CAT + SHA256 check
bitokd buildscript '["OP_CAT","OP_SHA256","<32-byte-hash>","OP_EQUAL"]'

# IF/ELSE branching
bitokd buildscript '["OP_IF","<pubkey_a>","OP_CHECKSIG","OP_ELSE","OP_HASH160","<hash>","OP_EQUALVERIFY","<pubkey_b>","OP_CHECKSIG","OP_ENDIF"]'

# Bitwise mask + signature
bitokd buildscript '["OP_SWAP","<mask>","OP_AND","<expected>","OP_EQUALVERIFY","OP_CHECKSIG"]'
```

### Manual Hex

If you prefer raw hex:

```
93       OP_ADD
55       OP_5
87       OP_EQUAL
```

For a 20-byte data push:
```
14                     push 20 bytes (0x14 = 20)
[20 bytes of data]
```

For 100-byte data push:
```
4c                     OP_PUSHDATA1
64                     100 (one-byte length)
[100 bytes of data]
```

---

## 14. Complete Workflows

### Arithmetic Puzzle (no signatures)

Goal: lock coins so that anyone providing two numbers that sum to 5 can spend.

```bash
# 1. Build the script
bitokd buildscript '["OP_ADD","OP_5","OP_EQUAL"]'
# hex: "935587"

# 2. Test in sandbox before committing money
bitokd validatescript "935587" '["03","02"]'
# valid: true (3 + 2 = 5)
bitokd validatescript "935587" '["01","01"]'
# valid: false (1 + 1 != 5)

# 3. Fund the puzzle
TX=$(bitokd createrawtransaction \
  '[{"txid":"<your_utxo>","vout":0}]' \
  '{"935587":1.0,"<change_addr>":0.489}')
SIGNED=$(bitokd signrawtransaction "$TX")
# Parse hex from JSON result, then:
bitokd sendrawtransaction "<signed_hex>"
# -> funding txid

# 4. Create the spend transaction
SPEND=$(bitokd createrawtransaction \
  '[{"txid":"<funding_txid>","vout":0}]' \
  '{"<dest_addr>":0.989}')

# 5. Set the scriptSig: push 3, push 2
SPEND=$(bitokd setscriptsig "$SPEND" 0 '["03","02"]')
# Parse hex from JSON result

# 6. Verify before broadcast
bitokd verifyscriptpair "<spend_hex>" 0 "935587"
# verified: true

# 7. Broadcast
bitokd sendrawtransaction "<spend_hex>"
```

### Hashlock Contract

Lock coins: spender must know the secret preimage AND have a private key.

```bash
# 1. Choose a secret preimage (hex of "HelloWorld")
PREIMAGE="48656c6c6f576f726c64"

# 2. Register with wallet (stores HASH160 + SHA256 mappings)
bitokd addpreimage "$PREIMAGE"
# { "hash160": "<H160>", "sha256": "<S256>", "size": 10 }

# 3. Build hashlock script
bitokd buildscript '["OP_HASH160","<H160>","OP_EQUALVERIFY","OP_DUP","OP_HASH160","<your_pubkeyhash>","OP_EQUALVERIFY","OP_CHECKSIG"]'
# Save the hex

# 4. Fund it
TX=$(bitokd createrawtransaction \
  '[{"txid":"<utxo>","vout":0}]' \
  '{"<hashlock_hex>":1.0,"<change>":0.489}')
bitokd signrawtransaction "$TX"
bitokd sendrawtransaction "<signed_hex>"

# 5. Spend (automatic -- wallet finds preimage + key)
SPEND=$(bitokd createrawtransaction \
  '[{"txid":"<hashlock_txid>","vout":0}]' \
  '{"<dest>":0.989}')
bitokd signrawtransaction "$SPEND"
# complete: true

# 5-alt. Manual spend without wallet auto-signing:
HASH=$(bitokd getscriptsighash "$SPEND" 0 "<hashlock_scriptpubkey>" "ALL")
# Sign the sighash externally, then:
bitokd setscriptsig "$SPEND" 0 '["<sig_with_hashtype>","<pubkey>","48656c6c6f576f726c64"]'
bitokd verifyscriptpair "<spend_hex>" 0 "<hashlock_scriptpubkey>"
bitokd sendrawtransaction "<spend_hex>"
```

### Time-Locked Payment

```bash
# Create tx locked until block 25000
TX=$(bitokd createrawtransaction \
  '[{"txid":"<utxo>","vout":0,"sequence":0}]' \
  '{"<recipient>":1.0}' \
  25000)

# Sign it now
bitokd signrawtransaction "$TX"

# Give signed hex to recipient. They broadcast after block 25000:
bitokd sendrawtransaction "<signed_hex>"
# Before block 25000: "Transaction is not final"
# After block 25000: accepted
```

The `"sequence":0` is required -- nLockTime is only enforced when at least one input has sequence != 0xFFFFFFFF.

### Multi-Party Signing (2-of-2 Multisig)

```bash
# 1. Create multisig
bitokd createmultisig 2 '["<pubkey_alice>","<pubkey_bob>"]'
# hex: "5221<alice>21<bob>52ae"

# 2. Fund it
TX=$(bitokd createrawtransaction \
  '[{"txid":"<utxo>","vout":0}]' \
  '{"5221...52ae":1.0}')
bitokd signrawtransaction "$TX"
bitokd sendrawtransaction "<signed_hex>"

# 3. Create spend
SPEND=$(bitokd createrawtransaction \
  '[{"txid":"<multisig_txid>","vout":0}]' \
  '{"<dest>":0.989}')

# 4. Alice signs
PARTIAL=$(bitokd signrawtransaction "$SPEND" \
  '[{"txid":"<multisig_txid>","vout":0,"scriptPubKey":"5221...52ae"}]' \
  '["<alice_wif>"]')
# complete: false

# 5. Bob signs the partially-signed tx
FULL=$(bitokd signrawtransaction "<partial_hex>" \
  '[{"txid":"<multisig_txid>","vout":0,"scriptPubKey":"5221...52ae"}]' \
  '["<bob_wif>"]')
# complete: true

# 6. Broadcast
bitokd sendrawtransaction "<full_hex>"
```

### OP_CAT Covenant

Require spender to provide two fragments that concatenate to a known hash.

```bash
# 1. Build covenant
bitokd buildscript '["OP_CAT","OP_SHA256","<32-byte-expected-hash>","OP_EQUAL"]'

# 2. Test
bitokd validatescript "<script_hex>" '["<part1_hex>","<part2_hex>"]'
# Stack order: part1 pushed first (bottom), part2 pushed second (top)
# OP_CAT concatenates stacktop(-2) || stacktop(-1) = part1 || part2

# 3. Fund and spend
bitokd setscriptsig "$SPEND" 0 '["<part1_hex>","<part2_hex>"]'
```

### External/Offline Signing

```bash
# 1. Create unsigned tx
TX=$(bitokd createrawtransaction '[{"txid":"<utxo>","vout":0}]' '{"<dest>":0.989}')

# 2. Get sighash (online machine)
bitokd getscriptsighash "$TX" 0 "<scriptpubkey_hex>" "ALL"
# { "sighash": "abcd1234..." }

# 3. Sign offline with any ECDSA tool
# sig = ECDSA_sign(private_key, sighash_bytes)
# Append hash type: sig_bytes + 0x01

# 4. Assemble scriptSig
bitokd setscriptsig "$TX" 0 '["<sig_hex_01>","<pubkey_hex>"]'

# 5. Verify and broadcast
bitokd verifyscriptpair "<signed_hex>" 0 "<scriptpubkey_hex>"
bitokd sendrawtransaction "<signed_hex>"
```

---

## 15. Contract Patterns

### Anyone-Can-Spend

```
scriptPubKey: OP_TRUE    (hex: 51)
scriptSig:    (empty)
```

OP_TRUE pushes 1 on the stack. Verification passes. Anyone can claim. Testing only.

### Provably Unspendable (Data Carrier)

```
scriptPubKey: OP_RETURN [data]    (hex: 6a ...)
```

OP_RETURN fails immediately. Output can never be spent. Use for timestamping, data embedding.

```bash
bitokd createrawtransaction '[{"txid":"<utxo>","vout":0}]' '{"data":"48656c6c6f","<change>":0.489}'
```

### Hash Puzzle (no signature, anyone with the answer can spend)

```
scriptPubKey: OP_SHA256 <expected_hash> OP_EQUAL
scriptSig:    <preimage>
```

Dangerous alone -- miners can front-run by extracting the preimage from your unconfirmed tx and replacing it with their own spend. Always combine with a signature requirement.

### Hash Puzzle + Signature (hashlock)

```
scriptPubKey: OP_HASH160 <hash> OP_EQUALVERIFY OP_DUP OP_HASH160 <pubkeyhash> OP_EQUALVERIFY OP_CHECKSIG
scriptSig:    <sig> <pubkey> <preimage>
```

Requires the secret AND the private key. Safe against miner front-running. Wallet auto-signs if preimage registered via `addpreimage`.

### Conditional Branches (IF/ELSE)

```
scriptPubKey:
  OP_IF
    <pubkey_alice> OP_CHECKSIG
  OP_ELSE
    OP_HASH160 <hash> OP_EQUALVERIFY <pubkey_bob> OP_CHECKSIG
  OP_ENDIF

Path A scriptSig: <sig_alice> OP_1
Path B scriptSig: <sig_bob> <pubkey_bob> <preimage> OP_0
```

The last push selects the branch: truthy = IF path, falsy = ELSE path.

### Arithmetic Gate + Signature

```
scriptPubKey: OP_SWAP <value> OP_EQUALVERIFY OP_DUP OP_HASH160 <pkh> OP_EQUALVERIFY OP_CHECKSIG
scriptSig:    <sig> <pubkey> <numeric_answer>
```

Must know a specific number AND have the key.

### Bitwise Access Control

```
scriptPubKey: <mask> OP_AND <expected> OP_EQUALVERIFY OP_DUP OP_HASH160 <pkh> OP_EQUALVERIFY OP_CHECKSIG
scriptSig:    <sig> <pubkey> <flags_byte>
```

The flags byte is ANDed with a mask. Only specific bit patterns pass. Combine with sig for auth.

### Concatenation Proof

```
scriptPubKey: OP_2DUP OP_CAT OP_SHA256 <hash> OP_EQUALVERIFY OP_SIZE OP_SWAP OP_SIZE OP_NUMEQUAL
scriptSig:    <part1> <part2>
```

Two equal-length fragments whose concatenation hashes to a known value.

### Escrow with Timeout

Use nLockTime + multisig:

1. **Funding tx:** 2-of-3 multisig (buyer, seller, arbitrator)
2. **Refund tx:** Created simultaneously with nLockTime set to future block, pre-signed by buyer (needs 1 more signature to be valid)

Before timeout: 2-of-3 can release funds normally.
After timeout: buyer holds a valid refund path if seller disappears.

---

## 16. Debugging

### Step 1: Static Analysis

```bash
bitokd analyzescript "<your_script_hex>"
```

Check: `withinLimits: true`, `type` matches expectation, `opcodes.counted` < 201, `sigops` reasonable.

### Step 2: Sandbox Test

```bash
bitokd validatescript "<scriptpubkey_hex>" '["<push3>","<push2>","<push1>"]' "exec"
```

Array order: first = bottom of stack, last = top. Check `finalStack` for what remains. Check `warnings` for limit issues.

### Step 3: Full Verification

```bash
bitokd verifyscriptpair "<signed_tx_hex>" 0 "<scriptpubkey_hex>" "exec"
```

Real consensus verification. If `verified: true`, the network will accept it.

### Step 4: Inspect the scriptSig

```bash
bitokd decodescriptsig "<scriptsig_hex>" "<scriptpubkey_hex>"
```

Verify role labels and push counts match expectations.

### Common Mistakes

**1. Stack order confusion.**
scriptSig pushes execute left-to-right. The LAST push is on TOP. `setscriptsig ... '["05","03"]'` with `OP_SUB` computes `5 - 3 = 2` (not `3 - 5`). The first push (`05`) ends up deeper, the second push (`03`) is on top. OP_SUB does `stacktop(-2) - stacktop(-1)`.

**2. Non-push-only scriptSig.**
Post-activation scriptSig must be push-only. You cannot put `OP_DUP` or `OP_ADD` in scriptSig. All logic goes in scriptPubKey.

**3. Numeric overflow.**
Arithmetic operands max 4 bytes. `OP_MUL` of two large numbers can overflow. `2147483647 + 1` fails. Plan arithmetic to stay within [-2147483647, +2147483647].

**4. Forgetting VERIFY.**
`OP_EQUAL` leaves true/false on the stack but does not fail. If your script continues, the result stays there. Use `OP_EQUALVERIFY` to consume and fail on mismatch. Same for `OP_NUMEQUAL` vs `OP_NUMEQUALVERIFY`.

**5. CHECKMULTISIG dummy.**
The `OP_0` before signatures is mandatory. It is consumed and thrown away. Post-activation it must be empty (size 0). Omitting it causes signature misalignment.

**6. OP_CAT size limit.**
Result must be <= 520 bytes. Two 300-byte items cannot be concatenated.

**7. Shift limits.**
OP_LSHIFT/OP_RSHIFT: amount must be 0-31. Shifting by 32 or more fails.

**8. Division by zero.**
OP_DIV and OP_MOD with divisor 0 fail the script immediately.

**9. Unbalanced IF/ENDIF.**
Every OP_IF or OP_NOTIF needs a matching OP_ENDIF. Missing it fails after all opcodes execute.

**10. Testing with validatescript for sig-dependent scripts.**
`validatescript` cannot verify signatures (no transaction context). Use `verifyscriptpair` when your script contains OP_CHECKSIG.

---

## 17. Quick Reference Tables

### Opcode Hex Map

```
00       OP_0/FALSE           51-60    OP_1 through OP_16
4c       OP_PUSHDATA1         4d       OP_PUSHDATA2
4e       OP_PUSHDATA4         4f       OP_1NEGATE

61       OP_NOP               63       OP_IF
64       OP_NOTIF             67       OP_ELSE
68       OP_ENDIF             69       OP_VERIFY
6a       OP_RETURN

6b       OP_TOALTSTACK        6c       OP_FROMALTSTACK
6d       OP_2DROP             6e       OP_2DUP
6f       OP_3DUP              70       OP_2OVER
71       OP_2ROT              72       OP_2SWAP
73       OP_IFDUP             74       OP_DEPTH
75       OP_DROP              76       OP_DUP
77       OP_NIP               78       OP_OVER
79       OP_PICK              7a       OP_ROLL
7b       OP_ROT               7c       OP_SWAP
7d       OP_TUCK

7e       OP_CAT               7f       OP_SUBSTR
80       OP_LEFT              81       OP_RIGHT
82       OP_SIZE

83       OP_INVERT            84       OP_AND
85       OP_OR                86       OP_XOR
87       OP_EQUAL             88       OP_EQUALVERIFY

8b       OP_1ADD              8c       OP_1SUB
8d       OP_2MUL              8e       OP_2DIV
8f       OP_NEGATE            90       OP_ABS
91       OP_NOT               92       OP_0NOTEQUAL
93       OP_ADD               94       OP_SUB
95       OP_MUL               96       OP_DIV
97       OP_MOD               98       OP_LSHIFT
99       OP_RSHIFT            9a       OP_BOOLAND
9b       OP_BOOLOR            9c       OP_NUMEQUAL
9d       OP_NUMEQUALVERIFY    9e       OP_NUMNOTEQUAL
9f       OP_LESSTHAN          a0       OP_GREATERTHAN
a1       OP_LESSTHANOREQUAL   a2       OP_GREATERTHANOREQUAL
a3       OP_MIN               a4       OP_MAX
a5       OP_WITHIN

a6       OP_RIPEMD160         a7       OP_SHA1
a8       OP_SHA256            a9       OP_HASH160
aa       OP_HASH256           ab       OP_CODESEPARATOR
ac       OP_CHECKSIG          ad       OP_CHECKSIGVERIFY
ae       OP_CHECKMULTISIG     af       OP_CHECKMULTISIGVERIFY
```

### Limits

| What | Value |
|------|-------|
| Script size | 10,000 bytes |
| Stack + altstack | 1,000 elements |
| Element size | 520 bytes |
| Opcodes per script | 201 |
| Numeric operand | 4 bytes |
| Shift amount | 0-31 |
| CHECKMULTISIG keys | 20 |
| Sigops per block | 20,000 |
| Block size | 1,000,000 bytes |
| Dust threshold | 0.01 BITOK |
| Fee per KB | 0.01 BITOK |
| Coinbase maturity | 100 blocks |
| Max supply | 21,000,000 BITOK |

### Chain Constants

| Constant | Value |
|----------|-------|
| COIN | 100,000,000 satoshis |
| CENT | 1,000,000 satoshis |
| Script activation | Block 18,000 |
| nLockTime threshold | 500,000,000 |
| Message start bytes | `0xb4 0x0b 0xc0 0xde` |
| nSequence final | `0xFFFFFFFF` |
| nVersion | 1 |

### Sighash Bytes

| Type | Byte |
|------|------|
| ALL | `0x01` |
| NONE | `0x02` |
| SINGLE | `0x03` |
| ALL\|ANYONECANPAY | `0x81` |
| NONE\|ANYONECANPAY | `0x82` |
| SINGLE\|ANYONECANPAY | `0x83` |
