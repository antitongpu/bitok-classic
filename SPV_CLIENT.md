# BITOK SPV (Lightweight) Client - Technical Specification

This document describes the protocol and architecture for building a True P2P SPV (Simplified Payment Verification) client for the Bitok network. All required node-side changes have been implemented.

## Overview

An SPV client verifies payments without downloading the full blockchain. It downloads only block headers (80 bytes each), verifies proof-of-work on those headers, and requests Merkle proofs for transactions relevant to its wallet. This follows the design described in Section 8 of the Bitcoin whitepaper.

**Security model**: The SPV client trusts that the longest valid header chain represents the honest chain. It cannot fully validate transactions (e.g., double-spend detection) but can verify that a transaction was included in a block accepted by the network.

---

## Protocol Messages

### Header Synchronization

#### `getheaders` (client -> node)

Requests a batch of block headers. This is the primary synchronization mechanism for SPV clients.

**Payload**:
| Field | Type | Description |
|-------|------|-------------|
| version | int32 | Protocol version |
| hash_count | varint | Number of block locator hashes |
| block_locator_hashes | uint256[] | Block locator object (same as `getblocks`) |
| hash_stop | uint256 | Stop at this hash (0 = send as many as possible) |

**Behavior**:
- The node finds the first block in the locator that exists in its main chain
- Returns up to **2000 headers** starting from the block after the match
- If locator is empty and hash_stop is set, returns headers starting from that specific block

**Block Locator Construction**:
The locator is built by walking back from the client's best known header:
1. Add the 10 most recent hashes (step = 1)
2. Then exponentially increase step size (step *= 2)
3. Always include the genesis block hash as the last entry

```
[tip, tip-1, tip-2, ..., tip-9, tip-11, tip-15, tip-23, ..., genesis]
```

#### `headers` (node -> client)

Response to `getheaders`.

**Payload**:
| Field | Type | Description |
|-------|------|-------------|
| count | varint | Number of headers (max 2000) |
| headers | block_header[] | Array of block headers, each 81 bytes (80-byte header + varint 0 for empty tx count) |

**Block Header Format** (80 bytes):
| Field | Size | Description |
|-------|------|-------------|
| nVersion | 4 bytes | Block version |
| hashPrevBlock | 32 bytes | Hash of previous block header |
| hashMerkleRoot | 32 bytes | Root of the Merkle tree of transactions |
| nTime | 4 bytes | Block timestamp (Unix epoch) |
| nBits | 4 bytes | Compact difficulty target |
| nNonce | 4 bytes | Nonce used for proof-of-work |

### Bloom Filtering (BIP37-style)

Bloom filters allow SPV clients to request only transactions relevant to their wallet without revealing their exact addresses.

#### `filterload` (client -> node)

Sets a bloom filter on the connection. After this, the node will only relay transactions matching the filter and will send `merkleblock` messages instead of full `block` messages.

**Payload**:
| Field | Type | Description |
|-------|------|-------------|
| filter | uint8[] | The bloom filter data |
| nHashFuncs | uint32 | Number of hash functions |
| nTweak | uint32 | Random tweak for hash function seed |
| nFlags | uint8 | Filter update flags |

**Filter flags**:
- `BLOOM_UPDATE_NONE` (0): Filter is not updated when a match is found
- `BLOOM_UPDATE_ALL` (1): Filter is updated with outpoints for all matched outputs
- `BLOOM_UPDATE_P2PUBKEY_ONLY` (2): Only update for P2PK/multisig outputs

**Filter sizing**: For `n` elements and false-positive rate `p`:
- Filter size (bytes): `-1 / (ln(2)^2) * n * ln(p) / 8`
- Number of hash functions: `(filter_size_bits / n) * ln(2)`
- Max filter size: 36,000 bytes
- Max hash functions: 50

**Recommended parameters**:
- For a wallet with 100 addresses: ~1200 bytes at 0.0001 FP rate
- For a wallet with 1000 addresses: ~12000 bytes at 0.0001 FP rate
- Use `BLOOM_UPDATE_ALL` for simplicity, `BLOOM_UPDATE_P2PUBKEY_ONLY` for better privacy

#### `filteradd` (client -> node)

Adds a single data element to the existing bloom filter.

**Payload**:
| Field | Type | Description |
|-------|------|-------------|
| data | uint8[] | Data element to add (max 520 bytes) |

Typically used to add a new address/pubkey hash after generating a new receiving address.

#### `filterclear` (client -> node)

Removes the bloom filter from the connection. The node resumes full block relay.

**Payload**: Empty

### Merkle Block Verification

#### `merkleblock` (node -> client)

Sent instead of `block` when a bloom filter is active. Contains the block header and a partial Merkle tree proving which transactions matched the filter.

**Payload**:
| Field | Type | Description |
|-------|------|-------------|
| header | block_header | 80-byte block header |
| num_transactions | uint32 | Total number of transactions in the block |
| hashes | uint256[] | Hashes in depth-first order |
| flags | uint8[] | Bit flags for tree traversal |

After receiving `merkleblock`, the node sends individual `tx` messages for each matched transaction.

**Verification algorithm**:
1. Reconstruct the partial Merkle tree from `hashes` and `flags`
2. The reconstructed root must equal `header.hashMerkleRoot`
3. The matched transaction hashes are extracted during reconstruction
4. Verify proof-of-work on the header: `GetPoWHash(header) <= target_from_nBits`

#### `getdata` with `MSG_FILTERED_BLOCK` (client -> node)

To request a filtered block, the client sends `getdata` with inventory type `MSG_FILTERED_BLOCK` (3) instead of `MSG_BLOCK` (2).

**Inventory types**:
| Value | Name | Description |
|-------|------|-------------|
| 1 | MSG_TX | Transaction |
| 2 | MSG_BLOCK | Full block |
| 3 | MSG_FILTERED_BLOCK | Filtered block (merkleblock + matching txs) |

### Mempool Discovery

#### `mempool` (client -> node)

Requests the node to send `inv` messages for transactions in its mempool. If a bloom filter is active on the connection, only transactions matching the filter are included (per BIP37).

**Payload**: Empty

**Response**: The node sends an `inv` message containing matching mempool transaction hashes (or all hashes if no bloom filter is set).

---

## RPC Interface

These RPC commands support SPV client operations when connecting to a trusted full node.

### `getblockheader <hash>`

Returns header information for a specific block.

**Response**:
```json
{
  "hash": "0x...",
  "version": 1,
  "previousblockhash": "0x...",
  "merkleroot": "0x...",
  "time": 1234567890,
  "bits": 486604799,
  "nonce": 12345,
  "height": 1000,
  "confirmations": 500,
  "nextblockhash": "0x...",
  "hex": "01000000...0000"
}
```

The `hex` field contains the raw 80-byte header in hex encoding.

### `sendrawtransaction <hex>`

Broadcasts a raw signed transaction to the network.

**Input**: Hex-encoded serialized transaction
**Response**: Transaction hash (txid)

This is essential for SPV clients that create and sign transactions locally.

### `gettxoutproof <txid> [blockhash]`

Returns a hex-encoded Merkle proof that a transaction was included in a block.

**Input**:
- `txid`: Transaction hash to prove
- `blockhash` (optional): Specific block to look in

**Response**: Hex-encoded `CMerkleBlock` containing the header and partial Merkle tree.

### `verifytxoutproof <hex proof>`

Verifies a Merkle proof and returns the transaction IDs it commits to.

**Input**: Hex-encoded proof from `gettxoutproof`
**Response**: Array of transaction IDs the proof validates, or empty array if invalid.

---

## SPV Client Architecture

### Recommended Stack

**Option 1: Desktop (Rust)**
- Networking: `tokio` + custom P2P protocol
- Storage: `sled` or `sqlite` for headers
- PoW: Yespower C reference implementation (FFI binding) -- required for header verification
- Crypto: `secp256k1` crate (signing), `sha2` (double SHA256 for hashes), `ripemd` (Hash160 for addresses)
- Key management: BIP32/BIP39 HD wallet
- UI: `egui`, `tauri`, or CLI

**Option 2: Web/Mobile (TypeScript)**
- Networking: WebSocket bridge to a gateway node
- Storage: IndexedDB (browser) or SQLite (React Native)
- PoW: Yespower compiled to WASM -- required for header verification
- Crypto: `@noble/secp256k1` (signing), `@noble/hashes` (SHA256, RIPEMD160)
- UI: React/Vite (web), React Native (mobile)

**Critical note**: The node uses secp256k1 via OpenSSL (`key.h`). SPV clients need the same curve for transaction signing. Yespower (with personalization string `"BitokPoW"`) is mandatory for PoW verification and has no pure-JS implementation -- a WASM build of the C reference is recommended.

### Component Design

```
+------------------+
|    UI Layer      |
+------------------+
         |
+------------------+
|  Wallet Manager  |  <- Key generation, address derivation, balance tracking
+------------------+
         |
+------------------+
|  TX Builder      |  <- UTXO selection, transaction construction, signing
+------------------+
         |
+------------------+
|  SPV Engine      |  <- Header sync, Merkle verification, bloom filter management
+------------------+
         |
+------------------+
|  P2P Network     |  <- Connection management, message serialization, peer discovery
+------------------+
         |
+------------------+
|  Header Store    |  <- Persistent storage of validated header chain
+------------------+
```

### Synchronization Flow

1. **Initial connection**:
   - Connect to one or more full nodes
   - Send `version` message (set services to 0, indicating SPV)
   - Receive `verack`

2. **Header sync**:
   - Build block locator from stored headers (or genesis if first sync)
   - Send `getheaders` with locator
   - Receive `headers` response (up to 2000 headers)
   - Validate each header:
     - `hashPrevBlock` matches previous header's hash
     - Proof-of-work: `GetPoWHash(header) <= target_from_nBits` (Yespower)
     - Timestamp within acceptable range
     - Difficulty adjustment is correct
   - Store validated headers
   - If received 2000 headers, send another `getheaders` (more available)
   - Repeat until receiving fewer than 2000 headers

3. **Bloom filter setup**:
   - Construct bloom filter containing:
     - All wallet address hashes (Hash160 of public keys)
     - All public keys (for P2PK outputs)
     - All outpoints being tracked (for spending detection)
   - Send `filterload` message

4. **Transaction discovery**:
   - Send `mempool` to discover unconfirmed transactions
   - For historical transactions, request blocks via `getdata` with `MSG_FILTERED_BLOCK`
   - Node responds with `merkleblock` + matching `tx` messages
   - Verify each `merkleblock`:
     - Reconstruct partial Merkle tree
     - Verify root matches header's `hashMerkleRoot`
     - Verify header's proof-of-work
     - Verify header is in the validated chain

5. **Ongoing monitoring**:
   - Listen for `inv` messages for new blocks and transactions
   - Request new blocks via `getdata` with `MSG_FILTERED_BLOCK`
   - Request interesting transactions via `getdata` with `MSG_TX`
   - When generating new addresses, send `filteradd` to update the bloom filter

### Proof-of-Work Verification

Bitok uses **Yespower** as its proof-of-work algorithm. The SPV client MUST implement Yespower hashing to verify block headers.

**Yespower parameters** (from `yespower_hash.h`):
```
Version:  YESPOWER_1_0
N:        2048
R:        32
Pers:     "BitokPoW" (8 bytes)
```

The PoW hash is computed as:
```
pow_hash = yespower_1_0(header_bytes_80, N=2048, R=32, pers="BitokPoW")
valid = (pow_hash <= target_from_compact_bits)
```

**Target calculation** from compact `nBits` (OpenSSL MPI encoding, see `bignum.h:265`):
```
size = (nBits >> 24) & 0xff           // number of bytes in the target
byte1 = (nBits >> 16) & 0xff
byte2 = (nBits >> 8) & 0xff
byte3 = nBits & 0xff
target = (byte1 * 256^2 + byte2 * 256 + byte3) * 256^(size - 3)
```

The proof-of-work limit is `~uint256(0) >> 17` which has 17 leading zero bits.
In compact form this is `0x1e7fffff`, meaning the easiest valid target is:
```
0x00007FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
```

### Difficulty Adjustment

Bitok adjusts difficulty every 2016 blocks based on the actual timespan of the previous 2016 blocks. The SPV client should verify that `nBits` in each header follows the expected adjustment schedule. The difficulty retargeting parameters are:

- Target block time: 10 minutes (600 seconds)
- Target timespan: 2 weeks (1,209,600 seconds)
- Retarget interval: 2016 blocks
- Adjustment bounds: The difficulty cannot change by more than 4x in either direction per adjustment

### Transaction Format

Bitok transactions use the same format as Bitcoin (Satoshi serialization):

| Field | Type | Description |
|-------|------|-------------|
| nVersion | int32 | Transaction version (currently 1) |
| vin_count | varint | Number of inputs |
| vin | txin[] | Transaction inputs |
| vout_count | varint | Number of outputs |
| vout | txout[] | Transaction outputs |
| nLockTime | uint32 | Lock time |

**TxIn format**:
| Field | Type | Description |
|-------|------|-------------|
| prevout_hash | uint256 | Previous transaction hash |
| prevout_n | uint32 | Previous output index |
| scriptSig_len | varint | Length of signature script |
| scriptSig | uint8[] | Signature script |
| nSequence | uint32 | Sequence number |

**TxOut format**:
| Field | Type | Description |
|-------|------|-------------|
| nValue | int64 | Amount in satoshis |
| scriptPubKey_len | varint | Length of pubkey script |
| scriptPubKey | uint8[] | Public key script |

### Address Format

Bitok uses the same address format as early Bitcoin:
- **Version byte**: 0x00 (mainnet)
- **Format**: Base58Check(version_byte + Hash160(pubkey))
- **Hash160**: RIPEMD160(SHA256(pubkey))

### Network Parameters

| Parameter | Value |
|-----------|-------|
| Default P2P port | 18333 |
| Default RPC port | 8332 |
| Protocol version | 319 |
| Magic bytes | `0xb4 0x0b 0xc0 0xde` |
| Genesis block hash | `0x0290400ea28d3fe79d102ca6b7cd11cee5eba9f17f2046c303d92f65d6ed2617` |
| Max block size | 1,000,000 bytes |
| Block time target | 10 minutes (600 seconds) |
| Subsidy halving | Every 210,000 blocks |
| PoW algorithm | Yespower (N=2048, R=32) |

### Message Header Format

All P2P messages use this header:

| Field | Size | Description |
|-------|------|-------------|
| magic | 4 bytes | Network magic bytes (`0xb4 0x0b 0xc0 0xde`) |
| command | 12 bytes | Command name (null-padded ASCII) |
| length | 4 bytes | Payload size |
| checksum | 4 bytes | First 4 bytes of SHA256(SHA256(payload)) |

### Checkpoints

The SPV client should include known checkpoints to prevent long-range attacks:

| Height | Hash |
|--------|------|
| 0 | `0x0290400ea28d3fe79d102ca6b7cd11cee5eba9f17f2046c303d92f65d6ed2617` |
| 6666 | `0xe4845bb3b5426ace955dea347359030656921883d8723105e4ab79343c27cdca` |
| 14000 | `0x10bb78b6ff9825b407f8d30e41f0aee7664759573382875dcf12bb947082c747` |

---

## Privacy Considerations

Bloom filters leak information about which addresses the client owns:
- A node serving an SPV client can correlate addresses in the filter
- Lower false-positive rates = more privacy leakage
- Higher false-positive rates = more bandwidth usage

**Recommendations**:
- Use a false-positive rate of at least 0.001 (0.1%)
- Connect to multiple nodes with different filters
- Consider adding decoy entries to the filter
- Periodically disconnect and reconnect with new filters

---

## Security Considerations

1. **Eclipse attacks**: Connect to multiple independent peers to reduce risk of being fed a fake chain
2. **Header-only attacks**: Always verify PoW on headers; reject chains with insufficient cumulative work
3. **Transaction withholding**: A malicious node can omit transactions from merkleblock responses. Connect to multiple peers for redundancy
4. **Bloom filter privacy**: See privacy section above
5. **Minimum confirmations**: Wait for at least 6 confirmations before considering a payment final

---

## Files Modified in Node

| File | Changes |
|------|---------|
| `bloom.h` | New - Bloom filter implementation (`CBloomFilter`) |
| `merkleblock.h` | New - Partial Merkle tree and merkle block (`CPartialMerkleTree`, `CMerkleBlock`) |
| `main.h` | Added `CNode` forward declaration, `CBlockLocator::IsNull()` method |
| `main.cpp` | Added P2P handlers: `getheaders`, `headers`, `filterload`, `filteradd`, `filterclear`, `mempool`; Updated `getdata` for `MSG_FILTERED_BLOCK`; Updated `AlreadyHave` for filtered blocks |
| `net.h` | Added `MSG_FILTERED_BLOCK` inventory type; Added bloom filter state to `CNode` (`pfilter`, `cs_filter`); Updated `ppszTypeName` array |
| `rpc.cpp` | Added RPC commands: `getblockheader`, `sendrawtransaction`, `gettxoutproof`, `verifytxoutproof` |
| `headers.h` | Added includes for `bloom.h` and `merkleblock.h` |
| `makefile.*` | Added new header files to HEADERS dependency list |
