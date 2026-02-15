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

## Implementation Notes

- All hex output uses compact format (no spaces between bytes).
- `signrawtransaction` saves and restores the original scriptSig when signing fails, making it safe for incremental multi-party signing.
- The `prevout.n` index is validated against a safety bound (100,000) to prevent integer overflow when constructing internal signing structures.
- Script classification in `ClassifyScript` handles compressed (33-byte) and uncompressed (65-byte) public keys for both P2PK detection and signing.
- Wallet key signing delegates to the existing `SignSignature` / `Solver` path. Provided-key signing uses direct `SignatureHash` + `CKey::Sign` for P2PKH and P2PK.

## Helper Functions

Two internal helper functions support the RPCs:

- **`ClassifyScript`** (`rpc.cpp`) -- Classifies a scriptPubKey into one of 5 types (pubkeyhash, pubkey, multisig, nulldata, nonstandard). Extracts the required signature count and associated addresses. Used by both `decoderawtransaction` and `decodescript`.

- **`ScriptPubKeyToJSON`** (`rpc.cpp`) -- Converts a scriptPubKey to its full JSON representation (asm, hex, type, reqSigs, addresses). Used by `decoderawtransaction` for each output.

## Files Modified

- **`rpc.cpp`** -- Added `ClassifyScript`, `ScriptPubKeyToJSON`, `decoderawtransaction`, `decodescript`, `createrawtransaction`, `signrawtransaction`. Registered in call table. Added CLI parameter type conversions for JSON array/object arguments.
