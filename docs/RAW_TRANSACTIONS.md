# Raw Transaction RPC Commands

Implementation of raw transaction construction, signing, and decoding for the Bitok node. These RPCs enable programmatic transaction building without the wallet's automatic coin selection, giving full control over inputs, outputs, and signing.

## New Commands

### createrawtransaction

Constructs an unsigned raw transaction from explicit inputs and outputs.

```
createrawtransaction [{"txid":"id","vout":n},...] {"address":amount,...} [locktime]
```

**Parameters:**

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | inputs | array | JSON array of input objects |
| 2 | outputs | object | JSON object mapping destinations to amounts |
| 3 | locktime | int | Optional. Block height or unix time for nLockTime |

Each input object:
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| txid | string | yes | Transaction ID of the UTXO to spend |
| vout | int | yes | Output index within that transaction |
| sequence | int | no | Sequence number (default: 0xffffffff) |

Output keys can be:
- **Bitok address** -- creates a standard P2PKH output
- **`"data"`** -- creates an OP_RETURN output (value is hex data string, amount is always 0)
- **Raw hex script** -- used as-is for the scriptPubKey (enables custom scripts like multisig, hash locks)

**Examples:**

Standard payment:
```bash
bitokd createrawtransaction \
  '[{"txid":"abc123...","vout":0}]' \
  '{"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa":0.5}'
```

With change and OP_RETURN data:
```bash
bitokd createrawtransaction \
  '[{"txid":"abc123...","vout":0},{"txid":"def456...","vout":1}]' \
  '{"1A1zP1...":1.0,"1B2cD3...":0.499,"data":"48656c6c6f"}'
```

Time-locked transaction:
```bash
bitokd createrawtransaction \
  '[{"txid":"abc123...","vout":0,"sequence":0}]' \
  '{"1A1zP1...":0.5}' \
  500000
```

Bare multisig output (raw hex scriptPubKey):
```bash
bitokd createrawtransaction \
  '[{"txid":"abc123...","vout":0}]' \
  '{"5121<pubkey1>21<pubkey2>52ae":1.0}'
```

**Returns:** Hex-encoded unsigned transaction string.

---

### signrawtransaction

Signs the inputs of a raw transaction.

```
signrawtransaction <hex> [prevtxs] [privkeys]
```

**Parameters:**

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | hex | string | Hex-encoded raw transaction to sign |
| 2 | prevtxs | array | Optional. Previous outputs this tx spends (for off-chain UTXOs) |
| 3 | privkeys | array | Optional. Private keys to sign with (WIF format). If provided, ONLY these keys are used |

Each prevtxs element:
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| txid | string | yes | Transaction ID |
| vout | int | yes | Output index |
| scriptPubKey | string | yes | Hex of the output's scriptPubKey |

**Signing behavior:**

- Without `privkeys`: uses wallet keys. Looks up the scriptPubKey from wallet transactions, the blockchain transaction index, or the mempool.
- With `privkeys`: uses ONLY the provided keys. Does NOT fall back to wallet keys.
- With `prevtxs`: uses the provided scriptPubKey for inputs not found in the wallet/chain/mempool.
- Supports P2PKH and P2PK script types.
- Preserves existing signatures on inputs it cannot sign (safe for multi-party signing workflows).
- Always signs with SIGHASH_ALL.
- Low-S normalized signatures (BIP 62 compatible).

**Examples:**

Sign with wallet keys:
```bash
bitokd signrawtransaction "0100000001..."
```

Sign with explicit private key:
```bash
bitokd signrawtransaction "0100000001..." '[]' '["5HueCGU8rMjxEXxiPuD5BDku4MkFqeZyd4dZ1jvhTVqvbTLvyTJ"]'
```

Sign with off-chain previous output:
```bash
bitokd signrawtransaction "0100000001..." \
  '[{"txid":"abc123...","vout":0,"scriptPubKey":"76a914...88ac"}]'
```

**Returns:**
```json
{
  "hex": "0100000001...",
  "complete": true
}
```

- `hex`: the (possibly partially) signed transaction
- `complete`: `true` if all inputs have valid signatures, `false` if any remain unsigned

---

### decoderawtransaction

Decodes a raw transaction hex into a human-readable JSON structure.

```
decoderawtransaction <hex>
```

**Parameters:**

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | hex | string | Hex-encoded raw transaction |

**Example:**
```bash
bitokd decoderawtransaction "0100000001..."
```

**Returns:**
```json
{
  "txid": "d4e5f6...",
  "version": 1,
  "locktime": 0,
  "vin": [
    {
      "txid": "abc123...",
      "vout": 0,
      "scriptSig": {
        "asm": "304402... 04abcd...",
        "hex": "4830440220..."
      },
      "sequence": 4294967295
    }
  ],
  "vout": [
    {
      "value": 0.50000000,
      "n": 0,
      "scriptPubKey": {
        "asm": "OP_DUP OP_HASH160 89abcd... OP_EQUALVERIFY OP_CHECKSIG",
        "hex": "76a914...",
        "type": "pubkeyhash",
        "reqSigs": 1,
        "addresses": ["1A1zP1..."]
      }
    }
  ]
}
```

Coinbase inputs show a `"coinbase"` field instead of `"txid"` / `"scriptSig"`.

---

### decodescript

Decodes a raw hex script into human-readable form with type classification.

```
decodescript <hex>
```

**Parameters:**

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | hex | string | Hex-encoded script |

**Example:**
```bash
bitokd decodescript "76a91489abcdefabbaabbaabbaabbaabbaabbaabbaabba88ac"
```

**Returns:**
```json
{
  "asm": "OP_DUP OP_HASH160 89abcd... OP_EQUALVERIFY OP_CHECKSIG",
  "hex": "76a914...88ac",
  "type": "pubkeyhash",
  "reqSigs": 1,
  "addresses": ["1A1zP1..."]
}
```

**Recognized script types:**

| Type | Pattern | Description |
|------|---------|-------------|
| `pubkeyhash` | `OP_DUP OP_HASH160 <20> OP_EQUALVERIFY OP_CHECKSIG` | Standard P2PKH |
| `pubkey` | `<33\|65> OP_CHECKSIG` | Pay-to-public-key (compressed or uncompressed) |
| `multisig` | `OP_m <keys> OP_n OP_CHECKMULTISIG` | Bare multisig (1-of-1 through 16-of-16) |
| `nulldata` | `OP_RETURN [data]` | Data carrier output |
| `nonstandard` | anything else | Unrecognized script pattern |

---

## Typical Workflow

### 1. Manual transaction construction

```bash
# List available unspent outputs
bitokd listunspent

# Create unsigned transaction
RAW=$(bitokd createrawtransaction \
  '[{"txid":"<txid>","vout":0}]' \
  '{"<recipient_address>":0.5,"<change_address>":0.499}')

# Inspect before signing
bitokd decoderawtransaction $RAW

# Sign with wallet keys
SIGNED=$(bitokd signrawtransaction $RAW)

# Broadcast
bitokd sendrawtransaction <signed_hex>
```

### 2. Offline signing (cold wallet)

On the online node:
```bash
# Create the unsigned transaction
RAW=$(bitokd createrawtransaction '[{"txid":"...","vout":0}]' '{"addr":1.0}')

# Get the scriptPubKey for the input
bitokd getrawtransaction <txid> 1
# Note the scriptPubKey hex from the relevant output
```

On the offline machine:
```bash
# Sign with the private key
SIGNED=$(bitokd signrawtransaction "$RAW" \
  '[{"txid":"...","vout":0,"scriptPubKey":"76a914...88ac"}]' \
  '["5Kb8kLf9zgWQnogidDA76MzPL6TsZZY36hWXMssSzNydYXYB9KF"]')
```

Back on the online node:
```bash
bitokd sendrawtransaction <signed_hex>
```

### 3. Inspecting scripts

```bash
# Decode a P2PKH scriptPubKey
bitokd decodescript "76a91489abcdefabbaabbaabbaabbaabbaabbaabbaabba88ac"

# Decode a 2-of-3 multisig
bitokd decodescript "5221<key1>21<key2>21<key3>53ae"
```

### 4. Multisig workflow

```bash
# Generate a 2-of-3 multisig script from three public keys
bitokd createmultisig 2 '["03aab...","02bbc...","03ccd..."]'
# Returns: { "asm": "2 03aab... 02bbc... 03ccd... 3 OP_CHECKMULTISIG", "hex": "5221...", ... }

# Use the hex as an output in createrawtransaction
SCRIPT_HEX="5221..."
RAW=$(bitokd createrawtransaction \
  '[{"txid":"abc...","vout":0}]' \
  '{"'"$SCRIPT_HEX"'":1.0}')

# Sign with first key
PARTIAL=$(bitokd signrawtransaction "$RAW" '[]' '["5Hue...first_WIF"]')
# Returns: { "hex": "...", "complete": false }

# Pass to second signer - signatures are preserved and merged
SIGNED=$(bitokd signrawtransaction "<partial_hex>" '[]' '["5Kb8...second_WIF"]')
# Returns: { "hex": "...", "complete": true }

# Broadcast
bitokd sendrawtransaction "<signed_hex>"
```

### 5. Crowdfunding with SIGHASH_ANYONECANPAY

```bash
# Each participant signs their input with ANYONECANPAY
# Their signatures only commit to the shared output, not other inputs
bitokd signrawtransaction "$RAW" '[]' '["5Hue..."]' 'ALL|ANYONECANPAY'

# Coordinator assembles all signed inputs into one transaction
```

### 6. Add multisig to wallet tracking

```bash
# If you have the keys in your wallet, resolve addresses to pubkeys automatically
bitokd addmultisigaddress 2 '["1A1zP1...","1B2cD3...","03ccd..."]' "shared fund"

# The wallet will detect incoming payments to this multisig if you hold enough keys
```

---

## Multisig Commands

### createmultisig

Generates a bare multisig scriptPubKey without touching the wallet.

```
createmultisig <nrequired> ["pubkey1","pubkey2",...]
```

**Parameters:**

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | nrequired | int | Number of signatures required to spend (1 to n) |
| 2 | keys | array | Hex-encoded public keys (compressed 33-byte or uncompressed 65-byte) |

**Returns:**
```json
{
  "asm": "2 03aab... 02bbc... 03ccd... 3 OP_CHECKMULTISIG",
  "hex": "522103aab...2102bbc...2103ccd...53ae",
  "type": "2-of-3",
  "reqSigs": 2,
  "addresses": ["1A1zP1...", "1B2cD3...", "1C3eF4..."]
}
```

The `hex` field can be used directly as an output key in `createrawtransaction`.

### addmultisigaddress

Like `createmultisig` but adds the script to the wallet for payment tracking.

```
addmultisigaddress <nrequired> ["key",...] [label]
```

Each key can be a hex public key or a Bitok address. Addresses are resolved to public keys via wallet lookup (the key must be in the wallet).

---

## Sighash Types

The `signrawtransaction` command now accepts an optional fourth parameter specifying the signature hash type:

| Type | Description |
|------|-------------|
| `ALL` | Signs all inputs and outputs (default) |
| `NONE` | Signs all inputs, no outputs (blank check) |
| `SINGLE` | Signs all inputs, only the output at the same index |
| `ALL\|ANYONECANPAY` | Signs only this input, all outputs (crowdfunding) |
| `NONE\|ANYONECANPAY` | Signs only this input, no outputs |
| `SINGLE\|ANYONECANPAY` | Signs only this input and matching output |

Usage:
```bash
bitokd signrawtransaction "<hex>" '[]' '["<privkey>"]' 'ALL|ANYONECANPAY'
```

---

## Custom Script Commands

These RPCs provide the complete toolkit for building, signing, verifying, and interacting with **any** custom script. Combined with `createrawtransaction` (which accepts raw hex scriptPubKey as output keys) and `sendrawtransaction`, they enable a fully self-contained workflow for arbitrary script contracts.

### buildscript

Assembles a script from opcode names and hex data pushes. Eliminates the need to manually hex-encode scripts.

```
buildscript ["element",...]
```

Each element is either an opcode name (`"OP_DUP"`, `"OP_HASH160"`, `"OP_CHECKSIG"`, etc.), a hex data string to push (`"deadbeef"`), or a numeric shorthand (`"0"` through `"16"`, `"-1"`).

**Examples:**

P2PKH script:
```bash
bitokd buildscript '["OP_DUP","OP_HASH160","89abcdef01234567890123456789012345678901","OP_EQUALVERIFY","OP_CHECKSIG"]'
```

Arithmetic puzzle (x + y == 5):
```bash
bitokd buildscript '["OP_ADD","OP_5","OP_EQUAL"]'
```

Hashlock:
```bash
bitokd buildscript '["OP_HASH160","<20-byte-hash>","OP_EQUALVERIFY","OP_DUP","OP_HASH160","<20-byte-pubkeyhash>","OP_EQUALVERIFY","OP_CHECKSIG"]'
```

**Returns:**
```json
{
  "hex": "935587",
  "asm": "OP_ADD OP_5 OP_EQUAL",
  "size": 3,
  "type": "arithmetic",
  "withinLimits": true
}
```

---

### setscriptsig

Sets a custom scriptSig on a raw transaction input. This is the key tool for spending custom scripts -- you provide the exact stack data needed to satisfy the scriptPubKey.

```
setscriptsig <tx hex> <input index> <scriptsig>
```

The scriptSig can be:
- **Raw hex**: `"0102030405"` -- bytes are used directly as scriptSig
- **Array of hex pushes**: `["03","02"]` -- each element is push-encoded into the scriptSig
- **Space-separated opcode/data string**: `"03 02"` -- parsed as opcodes or hex data

**Examples:**

Spend an arithmetic puzzle (x=3, y=2 to satisfy OP_ADD OP_5 OP_EQUAL):
```bash
bitokd setscriptsig "$UNSIGNED_TX" 0 '["03","02"]'
```

Spend a hashlock with signature + pubkey + preimage:
```bash
bitokd setscriptsig "$UNSIGNED_TX" 0 '["<sig>","<pubkey>","<preimage>"]'
```

**Returns:**
```json
{
  "hex": "0100000001...",
  "scriptSig": "0103010287",
  "scriptSigAsm": "3 2",
  "inputIndex": 0
}
```

---

### getscriptsighash

Computes the signature hash (sighash) digest for an input. This is the 32-byte value that must be signed for OP_CHECKSIG to pass.

```
getscriptsighash <tx hex> <input index> <scriptpubkey hex> [sighashtype]
```

Use this for external/offline signing workflows, or when building custom scripts that include OP_CHECKSIG.

**Example:**
```bash
bitokd getscriptsighash "$UNSIGNED_TX" 0 "76a914...88ac" "ALL"
```

**Returns:**
```json
{
  "sighash": "a1b2c3d4e5f6...",
  "inputIndex": 0,
  "hashType": 1,
  "hashTypeName": "ALL",
  "scriptPubKeyAsm": "OP_DUP OP_HASH160 ... OP_EQUALVERIFY OP_CHECKSIG"
}
```

---

### decodescriptsig

Decodes a scriptSig in the context of its scriptPubKey, showing the role of each push element.

```
decodescriptsig <scriptsig hex> <scriptpubkey hex>
```

**Example:**
```bash
bitokd decodescriptsig "483045...0121..." "76a914...88ac"
```

**Returns:**
```json
{
  "scriptSig": "483045...",
  "scriptSigAsm": "3045... 04abcd...",
  "scriptPubKey": "76a914...88ac",
  "scriptPubKeyAsm": "OP_DUP OP_HASH160 ... OP_EQUALVERIFY OP_CHECKSIG",
  "type": "pubkeyhash",
  "elements": [
    { "index": 0, "hex": "3045...", "size": 72, "role": "signature" },
    { "index": 1, "hex": "04abcd...", "size": 65, "role": "pubkey" }
  ],
  "pushCount": 2,
  "isPushOnly": true
}
```

---

### verifyscriptpair

Executes full scriptSig + scriptPubKey verification against a real transaction. Unlike `validatescript`, this supports OP_CHECKSIG because it has transaction context.

```
verifyscriptpair <tx hex> <input index> <scriptpubkey hex> [flags]
```

Flags: `"exec"` (default, post-activation rules) or `"legacy"`.

**Example:**
```bash
bitokd verifyscriptpair "$SIGNED_TX" 0 "935587" "exec"
```

**Returns:**
```json
{
  "verified": true,
  "inputIndex": 0,
  "scriptSig": "0103010287",
  "scriptSigAsm": "3 2",
  "scriptPubKey": "935587",
  "scriptPubKeyAsm": "OP_ADD OP_5 OP_EQUAL",
  "type": "arithmetic",
  "flags": "exec"
}
```

On failure, includes a `diagnostics` array with reasons.

---

## Complete Custom Script Workflows

### Arithmetic Puzzle (no signatures required)

```bash
# 1. Build the puzzle script: x + y must equal 5
SCRIPT=$(bitokd buildscript '["OP_ADD","OP_5","OP_EQUAL"]')
# hex: "935587"

# 2. Send coins to the puzzle
RAW=$(bitokd createrawtransaction '[{"txid":"<utxo>","vout":0}]' '{"935587":1.0,"<change_addr>":0.499}')
SIGNED=$(bitokd signrawtransaction "$RAW")
bitokd sendrawtransaction "<signed_hex>"

# 3. Spend the puzzle (provide x=3, y=2)
SPEND=$(bitokd createrawtransaction '[{"txid":"<puzzle_txid>","vout":0}]' '{"<dest_addr>":0.999}')
SPEND=$(bitokd setscriptsig "$SPEND" 0 '["03","02"]')
bitokd verifyscriptpair "<spend_hex>" 0 "935587"
bitokd sendrawtransaction "<spend_hex>"
```

### Hashlock Contract (preimage + signature)

```bash
# 1. Choose a secret preimage
PREIMAGE="48656c6c6f576f726c64"  # "HelloWorld"

# 2. Register it with wallet
bitokd addpreimage "$PREIMAGE"
# Returns: { hash160: "abc123...", sha256: "def456..." }

# 3. Build the hashlock script
SCRIPT=$(bitokd buildscript '["OP_HASH160","abc123...","OP_EQUALVERIFY","OP_DUP","OP_HASH160","<your_pubkeyhash>","OP_EQUALVERIFY","OP_CHECKSIG"]')

# 4. Fund the hashlock
RAW=$(bitokd createrawtransaction '[{"txid":"<utxo>","vout":0}]' '{"<script_hex>":1.0}')
SIGNED=$(bitokd signrawtransaction "$RAW")
bitokd sendrawtransaction "<signed_hex>"

# 5. Spend the hashlock (wallet auto-signs if preimage is registered)
# Or manually: get sighash, sign externally, assemble with setscriptsig
SPEND=$(bitokd createrawtransaction '[{"txid":"<hashlock_txid>","vout":0}]' '{"<dest>":0.999}')
HASH=$(bitokd getscriptsighash "$SPEND" 0 "<scriptpubkey_hex>" "ALL")
# Sign the sighash externally, then:
bitokd setscriptsig "$SPEND" 0 '["<signature_with_hashtype>","<pubkey>","48656c6c6f576f726c64"]'
```

### Custom Covenant (OP_CAT + hash verification)

```bash
# 1. Build a covenant that requires data to concatenate to a known hash
SCRIPT=$(bitokd buildscript '["OP_CAT","OP_SHA256","<expected_hash>","OP_EQUAL"]')

# 2. Analyze the script
bitokd analyzescript "<script_hex>"

# 3. Fund it, then spend by providing the two halves
SPEND=$(bitokd setscriptsig "$UNSIGNED_TX" 0 '["<part1>","<part2>"]')
bitokd verifyscriptpair "<spend_hex>" 0 "<script_hex>"
```

---

## Implementation Notes

- All hex output uses compact format (no spaces between bytes).
- `signrawtransaction` preserves partial multisig signatures across signing rounds. Each signer adds their signature and passes the partially-signed hex to the next signer.
- Multisig signatures are matched to pubkeys by verification, maintaining correct ordering for `OP_CHECKMULTISIG`.
- The `prevout.n` index is validated against a safety bound (100,000) to prevent integer overflow when constructing internal signing structures.
- Script classification in `ClassifyScript` handles compressed (33-byte) and uncompressed (65-byte) public keys for both P2PK detection and signing.
- `Solver()` in `script.cpp` now recognizes three script templates: P2PK, P2PKH, and bare multisig. `IsMine()` returns true for multisig outputs where the wallet holds at least `m` of `n` keys.
- Wallet key signing delegates to the existing `SignSignature` / `Solver` path. Provided-key signing uses direct `SignatureHash` + `CKey::Sign` for P2PKH, P2PK, and multisig.

## Helper Functions

Internal helper functions supporting the RPCs:

- **`ClassifyScript`** (`rpc.cpp`) -- Classifies a scriptPubKey into one of 5 types (pubkeyhash, pubkey, multisig, nulldata, nonstandard). Extracts the required signature count and associated addresses. Used by both `decoderawtransaction` and `decodescript`.

- **`ScriptPubKeyToJSON`** (`rpc.cpp`) -- Converts a scriptPubKey to its full JSON representation (asm, hex, type, reqSigs, addresses). Used by `decoderawtransaction` for each output.

- **`ParseMultisigScript`** (`rpc.cpp`) -- Parses a bare multisig scriptPubKey, extracting the required signature count and ordered public keys. Used by `signrawtransaction` for multisig signing and signature merging.

- **`ParseSighashString`** (`rpc.cpp`) -- Converts a sighash type string ("ALL", "NONE|ANYONECANPAY", etc.) to the corresponding integer flag.

- **`IsValidPubKey`** (`rpc.cpp`) -- Validates a public key byte vector has correct format (33-byte compressed or 65-byte uncompressed with correct prefix).

- **`GetOpcodeByName`** (`rpc.cpp`) -- Reverse lookup from opcode name string (e.g., "OP_DUP", "OP_CHECKSIG", "5") to `opcodetype` enum value. Used by `buildscript` and `setscriptsig`.

## Files Modified

- **`rpc.cpp`** -- Added `ClassifyScript`, `ScriptPubKeyToJSON`, `decoderawtransaction`, `decodescript`, `createrawtransaction`, `signrawtransaction`, `createmultisig`, `addmultisigaddress`, `ParseMultisigScript`, `ParseSighashString`, `IsValidPubKey`, `GetOpcodeByName`, `buildscript`, `setscriptsig`, `getscriptsighash`, `decodescriptsig`, `verifyscriptpair`. Registered in call table. Added CLI parameter type conversions for JSON array/object arguments. Extended `signrawtransaction` with multisig support, partial signing, sighash type parameter.
- **`script.cpp`** -- Extended `Solver()` with bare multisig template recognition. The signing `Solver()` overload constructs `OP_0 <sig1> ... <sigm>` scriptSig for multisig. `IsMine()` now detects multisig ownership when wallet holds sufficient keys.
