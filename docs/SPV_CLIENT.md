# BITOK SPV (Lightweight) Client - Technical Specification

This document describes the protocol and architecture for building a lightweight wallet for the Bitok network based on the actual node implementation (Bitcoin 0.3 fork with Yespower PoW).

## Overview

An SPV client verifies payments without downloading the full blockchain. It downloads only block headers (80 bytes each), verifies proof-of-work on those headers, and requests Merkle proofs for transactions relevant to its wallet. This follows the design described in Section 8 of the Bitcoin whitepaper.

**Security model**: The SPV client trusts that the longest valid header chain represents the honest chain. It cannot fully validate transactions (e.g., double-spend detection) but can verify that a transaction was included in a block accepted by the network.

---

## Critical Architecture Constraint: No Address Indexing

The Bitok node (Bitcoin 0.3 architecture) does **NOT** maintain any address-based index. Balance and UTXO queries only work for keys held in the node's own `wallet.dat`:

- `GetBalance()` (`main.cpp:3595`) iterates `mapWallet` -- an in-memory map of transactions loaded from `wallet.dat`
- `IsMine()` (`script.cpp:1027`) checks script pubkeys against `mapKeys` and `mapPubKeys` -- keys the node operator owns
- `listunspent` (`rpc.cpp:1229`) only returns UTXOs where `txout.IsMine()` is true
- `getbalance` (`rpc.cpp:410`) returns the sum of `mapWallet` credits only
- `getreceivedbyaddress` (`rpc.cpp:1005`) returns 0.0 for any address not in the node's keyring

There is a `ReadOwnerTxes()` function (`bitcoin_db.h:276`) that can look up transactions by Hash160, but it only covers keys the node has in its wallet and is not exposed via RPC.

**Consequence**: A light wallet cannot query a standard Bitok node for "what is the balance of address X" or "give me UTXOs for address X" unless that address's key is imported into the node's wallet.

### Solutions for Light Wallet Balance Discovery

**Option A: Electrum-style indexing server (recommended)**

Build a separate indexing service that:
1. Connects to a Bitok node via RPC (`getblockhash`, `getblock`, `getrawtransaction`)
2. Scans every block and indexes transactions by output address into a database
3. Maintains a UTXO set indexed by address
4. Exposes an API: `GET /balance/:address`, `GET /utxos/:address`, `POST /broadcast`

The light wallet talks to this indexer, not to the node directly.

**Option B: True P2P SPV with bloom filters**

The bloom filter infrastructure exists in the node (`bloom.h`, `merkleblock.h`, `main.cpp:2596-2676`). The light wallet:
1. Syncs all block headers via `getheaders`/`headers`
2. Sends a `filterload` with a bloom filter matching its own addresses
3. Requests blocks via `getdata` with `MSG_FILTERED_BLOCK` (type 3)
4. Receives `merkleblock` + matched `tx` messages
5. Tracks its own UTXOs locally from matched transactions

This is fully decentralized but requires implementing the binary P2P protocol and managing the complete header chain.

**Option C: Import keys into a full node**

Use `importprivkey` RPC (`rpc.cpp:1705`) to import the wallet's keys into a running node. Then `listunspent` and `getbalance` work normally. Simple but:
- Couples the wallet to a specific node
- Requires trusting the node operator with private keys
- Does not scale to many users

---

## P2P Protocol Messages

### Connection Handshake

The node sends a `version` message on connect (`net.h:587`):

| Field | Value |
|-------|-------|
| version | 319 (`serialize.h:22`) |
| services | `NODE_NETWORK` (1) for full nodes, 0 for SPV |
| timestamp | int64 |
| addr_recv | Peer address |
| addr_from | Local address |
| nonce | Random uint64 |
| sub_version | Empty string |
| start_height | Best block height |

Protocol negotiation uses `min(local_version, remote_version)` (`main.cpp:2161`).

### Header Synchronization

#### `getheaders` (client -> node)

Requests a batch of block headers.

**Payload**:
| Field | Type | Description |
|-------|------|-------------|
| locator | CBlockLocator | Block locator object (serialized with version prefix) |
| hash_stop | uint256 | Stop at this hash (0 = send as many as possible) |

**Node behavior** (`main.cpp:2549-2588`):
- If locator is null and hash_stop is set, starts from that specific block
- Otherwise, finds the first locator hash in the main chain, starts from the next block
- Returns up to **2000 headers**
- Stops at hash_stop or when limit is reached

**Block Locator Construction** (`main.h:1337-1352`):
1. Walk back from tip adding hashes
2. After 10 entries, double the step size each iteration
3. Always append the genesis block hash as the last entry

```
[tip, tip-1, tip-2, ..., tip-9, tip-11, tip-15, tip-23, ..., genesis]
```

#### `headers` (node -> client)

Response to `getheaders`. Contains a vector of `CBlock` objects serialized as header-only (80 bytes each, vtx omitted via `SER_BLOCKHEADERONLY` flag).

**Block Header Format** (80 bytes):
| Field | Size | Description |
|-------|------|-------------|
| nVersion | 4 bytes | Block version (currently 1) |
| hashPrevBlock | 32 bytes | SHA256d hash of previous block header |
| hashMerkleRoot | 32 bytes | Root of the Merkle tree of transactions |
| nTime | 4 bytes | Block timestamp (Unix epoch) |
| nBits | 4 bytes | Compact difficulty target |
| nNonce | 4 bytes | Nonce used for proof-of-work |

Note: The block's identity hash (`GetHash()`) is `SHA256d(header_80_bytes)`, but proof-of-work validation uses `GetPoWHash()` which is `Yespower(header_80_bytes)`.

### Bloom Filtering (BIP37-style)

Bloom filters allow SPV clients to request only transactions relevant to their wallet.

#### `filterload` (client -> node)

Sets a bloom filter on the connection (`main.cpp:2596-2616`). After this:
- The node sets `pfrom->fClient = true`
- `getdata` requests with `MSG_FILTERED_BLOCK` will return `merkleblock` + matched transactions
- `mempool` responses are filtered through the bloom filter

**Payload** (CBloomFilter serialization, `bloom.h:65-71`):
| Field | Type | Description |
|-------|------|-------------|
| vData | vector\<uint8\> | The bloom filter bit array |
| nHashFuncs | uint32 | Number of hash functions |
| nTweak | uint32 | Random tweak for hash function seed |
| nFlags | uint8 | Filter update flags |

**Filter flags** (`bloom.h:11-16`):
- `BLOOM_UPDATE_NONE` (0): Filter is not updated on match
- `BLOOM_UPDATE_ALL` (1): Outpoints added for all matched outputs
- `BLOOM_UPDATE_P2PUBKEY_ONLY` (2): Only update for P2PK/multisig outputs

**Hash function** (`bloom.h:30-39`):
The bloom filter uses a custom Murmur-like hash, NOT standard MurmurHash3:
```
Hash(nHashNum, data):
  nIndex = nHashNum * 0xFBA4C795 + nTweak
  for each byte b in data:
    nIndex ^= b
    nIndex += (nIndex << 1) + (nIndex << 4) + (nIndex << 7) + (nIndex << 8) + (nIndex << 24)
  return nIndex % (filter_size_bits)
```

**Constraints**:
- Max filter size: 36,000 bytes (`MAX_BLOOM_FILTER_SIZE`)
- Max hash functions: 50 (`MAX_HASH_FUNCS`)
- Filter exceeding constraints causes disconnect (`main.cpp:2601-2605`)

**Filter sizing** (`bloom.h:49-63`): For `n` elements and false-positive rate `p`:
- Filter size (bytes): `-1.0 / (ln(2)^2) * n * ln(p) / 8.0`
- Number of hash functions: `(filter_size_bits / n) * ln(2)`

#### `filteradd` (client -> node)

Adds a single data element to the existing bloom filter (`main.cpp:2619-2635`).

**Payload**:
| Field | Type | Description |
|-------|------|-------------|
| data | vector\<uint8\> | Data element to add (max 520 bytes, disconnect if exceeded) |

#### `filterclear` (client -> node)

Removes the bloom filter (`main.cpp:2638-2649`). Sets `pfrom->fClient = false`, resuming full block relay.

**Payload**: Empty

### Filtered Block Retrieval

#### `getdata` with `MSG_FILTERED_BLOCK` (client -> node)

Send `getdata` with inventory type `MSG_FILTERED_BLOCK` (3) to request a filtered block.

**Node response** (`main.cpp:2292-2317`):
1. Reads the full block from disk
2. Constructs `CMerkleBlock` by matching transactions against the peer's bloom filter
3. Sends `merkleblock` message with header + partial Merkle tree
4. Sends individual `tx` messages for each matched transaction

**Inventory types** (`net.h:356-360`):
| Value | Name | Description |
|-------|------|-------------|
| 1 | MSG_TX | Transaction |
| 2 | MSG_BLOCK | Full block |
| 3 | MSG_FILTERED_BLOCK | Filtered block (merkleblock + matching txs) |

#### `merkleblock` (node -> client)

Contains a CMerkleBlock (`merkleblock.h:159-214`):
| Field | Type | Description |
|-------|------|-------------|
| header | CBlock (header-only) | 80-byte block header |
| txn | CPartialMerkleTree | Partial Merkle tree proving matched transactions |

CPartialMerkleTree (`merkleblock.h:8-156`):
| Field | Type | Description |
|-------|------|-------------|
| nTransactions | uint32 | Total number of transactions in the block |
| vHash | vector\<uint256\> | Hashes in depth-first order |
| vBits | bit vector | Packed as bytes, bit flags for tree traversal |

**Verification**:
1. Call `ExtractMatches()` to reconstruct the partial Merkle tree
2. Returns the computed Merkle root and a vector of matched transaction hashes
3. Verify the computed root matches `header.hashMerkleRoot`
4. Verify `GetPoWHash(header) <= target_from_nBits`
5. Verify the header is in the validated header chain

Validation bounds in `ExtractMatches()` (`merkleblock.h:124-130`):
- `nTransactions` must not be 0
- `nTransactions` must not exceed `MAX_BLOCK_SIZE / 60` (16,666)
- `vHash.size()` must not exceed `nTransactions`
- `vBits.size()` must be at least `vHash.size()`

### Mempool Discovery

#### `mempool` (client -> node)

Requests mempool transaction inventory (`main.cpp:2652-2676`).

**Payload**: Empty

**Response**: The node sends an `inv` message containing transaction hashes. If a bloom filter is active, only matching transactions are included (checked via `pfilter->IsRelevantAndUpdate()`).

**Filter matching** (`bloom.h:110-167`):
A transaction matches the bloom filter if any of:
- The transaction hash itself is in the filter
- Any data push in any output script matches the filter
- Any serialized outpoint in inputs matches the filter
- Any data push in any input script matches the filter

When `BLOOM_UPDATE_ALL` is set and an output matches, the corresponding outpoint is automatically added to the filter to detect future spends.

---

## RPC Interface

These RPC commands support SPV operations when connecting to a trusted full node.

### Block & Header Queries

#### `getblockcount`
Returns the height of the best chain tip. No parameters.

#### `getblockhash <height>`
Returns the hash of the block at the given height in the best chain.

#### `getbestblockhash`
Returns the hash of the best chain tip. No parameters.

#### `getblockheader <hash>`
Returns header information for a specific block (`rpc.cpp:1466-1508`).

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

The `hex` field contains the raw 80-byte header serialized with `SER_NETWORK | SER_BLOCKHEADERONLY`.

#### `getblock <hash>`
Returns full block information including transaction hashes (`rpc.cpp:151-188`).

### Transaction Queries

#### `getrawtransaction <txid> [verbose=0]`
Returns raw transaction data (`rpc.cpp:1121-1206`). Looks up in mempool first (`mapTransactions`), then on disk via `CTxDB`. Works for ANY transaction the node has seen, not just wallet transactions.

#### `gettransaction <txid>`
Returns detailed wallet transaction info (`rpc.cpp:191-320`). Checks `mapWallet` first (wallet-only), then falls back to `CTxDB` for basic transaction data.

### Transaction Broadcasting

#### `sendrawtransaction <hex>`
Broadcasts a raw signed transaction (`rpc.cpp:1511-1557`). Decodes, validates, accepts to mempool, and relays via `RelayMessage`. Returns txid.

### Merkle Proofs

#### `gettxoutproof <txid> [blockhash]`
Returns a hex-encoded CMerkleBlock proof (`rpc.cpp:1560-1632`). If `blockhash` is not provided, looks up the transaction's block via wallet or txindex.

#### `verifytxoutproof <hex>`
Verifies a Merkle proof and returns the proven transaction IDs (`rpc.cpp:1635-1670`). Checks:
- Merkle root reconstruction succeeds
- Block hash exists in `mapBlockIndex`
- Block is in the main chain
- Reconstructed Merkle root matches the block's stored `hashMerkleRoot`

### Key Management

#### `dumpprivkey <address>`
Exports a private key in WIF format (version byte 128) (`rpc.cpp:1673-1702`). Only works for addresses in the node's wallet.

#### `importprivkey <wif> [label]`
Imports a WIF-encoded private key into the node's wallet (`rpc.cpp:1705-1737`). Triggers rescan of existing blocks for matching transactions.

### Wallet Queries (node-owner only)

These only return data for addresses the node holds keys for:

| Command | Description |
|---------|-------------|
| `getbalance` | Total wallet balance |
| `listunspent [minconf] [maxconf]` | Wallet UTXOs with confirmation range |
| `listtransactions [count] [includegenerated]` | Recent wallet transactions |
| `listreceivedbyaddress [minconf] [includeempty]` | Amounts received per wallet address |
| `listreceivedbylabel [minconf] [includeempty]` | Amounts received per label |
| `getreceivedbyaddress <address> [minconf]` | Total received by a wallet address |
| `validateaddress <address>` | Address info including `ismine` flag |

---

## Network Parameters

| Parameter | Value | Source |
|-----------|-------|--------|
| P2P port | 18333 | `net.h:15` |
| RPC port | 8332 | `rpc.cpp:2039` |
| Protocol version | 319 | `serialize.h:22` |
| Magic bytes | `0xb4 0x0b 0xc0 0xde` | `main.h:20` |
| Genesis block hash | `0x0290400ea28d3fe79d102ca6b7cd11cee5eba9f17f2046c303d92f65d6ed2617` | `main.cpp:40` |
| Max block size | 1,000,000 bytes | `main.h:21` |
| Block time target | 10 minutes (600 seconds) | `main.cpp:930` |
| Difficulty retarget interval | 2016 blocks | `main.cpp:931` |
| Target timespan | 14 days (1,209,600 seconds) | `main.cpp:929` |
| Subsidy halving | Every 210,000 blocks | `main.cpp:922` |
| Initial block reward | 50 BITOK | `main.cpp:920` |
| Coinbase maturity | 100 blocks | `main.h:26` |
| Max money | 21,000,000 BITOK | `main.h:29` |
| PoW algorithm | Yespower 1.0 (N=2048, R=32) | `yespower_hash.h` |
| PoW limit | `~uint256(0) >> 17` | `main.h:35` |
| Address version byte | 0x00 | Standard Bitcoin mainnet |
| WIF version byte | 0x80 (128) | `rpc.cpp:1699` |
| Timewarp fix activation | Block 16,000 | `main.h:33` |

### Message Header Format

All P2P messages use this 24-byte header (`net.h:51-93`):

| Field | Size | Description |
|-------|------|-------------|
| magic | 4 bytes | `0xb4 0x0b 0xc0 0xde` |
| command | 12 bytes | Null-padded ASCII command name |
| length | 4 bytes | Payload size (max 268,435,456 bytes) |
| checksum | 4 bytes | First 4 bytes of SHA256d(payload), only if version >= 209 |

### Checkpoints

| Height | Hash | Source |
|--------|------|--------|
| 0 | `0x0290400ea28d3fe79d102ca6b7cd11cee5eba9f17f2046c303d92f65d6ed2617` | `main.cpp:92` |
| 6666 | `0xe4845bb3b5426ace955dea347359030656921883d8723105e4ab79343c27cdca` | `main.cpp:93` |
| 14000 | `0x10bb78b6ff9825b407f8d30e41f0aee7664759573382875dcf12bb947082c747` | `main.cpp:94` |

---

## Proof-of-Work Verification

Bitok uses **Yespower** as its proof-of-work algorithm.

**Yespower parameters** (from `yespower_hash.h`):
```
Version:  YESPOWER_1_0
N:        2048
R:        32
Pers:     "BitokPoW" (8 bytes)
```

**Block hashing** (`main.h:935-943`):
- `GetHash()` = `SHA256d(header_80_bytes)` -- used as block identifier/inventory hash
- `GetPoWHash()` = `Yespower(header_80_bytes)` -- used for proof-of-work validation

The PoW check is:
```
valid = (GetPoWHash(header) <= target_from_compact_nBits)
```

**Target calculation** from compact `nBits` (OpenSSL BigNum, `bignum.h`):
```
SetCompact(nBits):
  size  = (nBits >> 24) & 0xff
  word  = nBits & 0x007fffff
  target = word << (8 * (size - 3))
```

**PoW limit**: `~uint256(0) >> 17` -- 17 leading zero bits minimum.

In compact form `0x1e7fffff`:
```
0x00007FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
```

### Difficulty Adjustment

(`main.cpp:927-972`)

The node adjusts difficulty every **2016 blocks**:

1. If `(height + 1) % 2016 != 0`, use previous block's nBits unchanged
2. At retarget boundary:
   - `nActualTimespan = last_block.nTime - first_block_of_interval.nTime`
   - Clamp: `nTargetTimespan/4 <= nActualTimespan <= nTargetTimespan*4`
   - `new_target = old_target * nActualTimespan / nTargetTimespan`
   - Cap at `bnProofOfWorkLimit`

### Timewarp Protection

(`main.cpp:1472-1511`)

After block 16,000 (`TIMEWARP_ACTIVATION_HEIGHT`), the node enforces:
- Block timestamp cannot exceed parent timestamp by more than `7200 + 3600` seconds (2h + 1h drift)
- At difficulty adjustment boundaries, the actual timespan cannot be less than `nTargetTimespan / 8`

An SPV client verifying headers should implement the same checks.

---

## Transaction Format

Bitok transactions use standard Bitcoin Satoshi serialization (`main.h:386-648`):

| Field | Type | Description |
|-------|------|-------------|
| nVersion | int32 | Transaction version (currently 1) |
| vin_count | varint | Number of inputs |
| vin | CTxIn[] | Transaction inputs |
| vout_count | varint | Number of outputs |
| vout | CTxOut[] | Transaction outputs |
| nLockTime | uint32 | Lock time |

**CTxIn** (`main.h:222-294`):
| Field | Type | Description |
|-------|------|-------------|
| prevout.hash | uint256 | Previous transaction hash |
| prevout.n | uint32 | Previous output index |
| scriptSig | varint + bytes | Signature script |
| nSequence | uint32 | Sequence number (UINT_MAX = final) |

**CTxOut** (`main.h:303-377`):
| Field | Type | Description |
|-------|------|-------------|
| nValue | int64 | Amount in satoshis (1 BITOK = 100,000,000 satoshis) |
| scriptPubKey | varint + bytes | Public key script |

**Transaction hash**: `SHA256d(serialized_tx)` -- used as txid.

### Fee Structure

(`main.h:547-569`)

- Base fee: 1 CENT (0.01 BITOK) per kilobyte
- Transactions under 60KB are free if block size is under 80KB
- Transactions under 3KB are free if block size is under 200KB
- If any output is less than 0.01 BITOK (dust), minimum 0.01 BITOK fee applies
- 1 CENT = 1,000,000 satoshis

### Script Types

(`script.cpp:912-971`, `rpc.cpp:28-56`)

The node recognizes two standard output script patterns:

**P2PKH** (Pay to Public Key Hash):
```
OP_DUP OP_HASH160 <20-byte-hash160> OP_EQUALVERIFY OP_CHECKSIG
```

**P2PK** (Pay to Public Key):
```
<33-byte-compressed-pubkey> OP_CHECKSIG
<65-byte-uncompressed-pubkey> OP_CHECKSIG
```

### Address Format

- **Version byte**: 0x00 (mainnet)
- **Format**: Base58Check(0x00 + Hash160(pubkey))
- **Hash160**: RIPEMD160(SHA256(pubkey))
- Both compressed (33-byte) and uncompressed (65-byte) public keys are supported

---

## Recommended Architecture for Light Wallet

### Option A: Indexer-Backed Web Wallet (Practical)

Best for reaching the most users with the least friction.

```
+------------------+
|    Web Wallet    |  React + Vite + TypeScript
|    (Browser)     |
+------------------+
         |  HTTPS/JSON API
+------------------+
|  Indexer API     |  Edge Functions / Node.js service
+------------------+
         |  SQL
+------------------+
|  Index Database  |  Postgres (Supabase)
+------------------+  Tables: blocks, transactions, utxos, address_history
         |  JSON-RPC
+------------------+
|  Bitok Full Node |  Standard bitok daemon
+------------------+
```

**Wallet client stack**:
| Component | Technology |
|-----------|-----------|
| UI framework | React + Vite + TypeScript |
| Crypto (signing) | `@noble/secp256k1` |
| Crypto (hashing) | `@noble/hashes` (SHA256, RIPEMD160) |
| Key derivation | `@scure/bip32` + `@scure/bip39` (HD wallet) |
| Address encoding | `bs58check` or custom Base58Check (~50 lines) |
| Header verification | Yespower compiled to WASM via Emscripten |
| Local storage | IndexedDB for header cache |

**Indexer responsibilities**:
1. Poll node via `getblockhash` + `getblock` to discover new blocks
2. For each block, fetch full transactions via `getrawtransaction`
3. Parse outputs to extract addresses, store UTXOs by address
4. Track spent outputs by matching inputs to existing UTXOs
5. Expose REST API: `GET /utxos/:address`, `GET /balance/:address`, `GET /history/:address`
6. Proxy `sendrawtransaction` to the node for broadcasting

### Option B: True P2P SPV Client (Decentralized)

Requires implementing the binary P2P protocol but eliminates the indexer dependency.

```
+------------------+
|    UI Layer      |
+------------------+
         |
+------------------+
|  Wallet Manager  |  Key generation, HD derivation, UTXO tracking
+------------------+
         |
+------------------+
|  TX Builder      |  UTXO selection, transaction construction, signing
+------------------+
         |
+------------------+
|  SPV Engine      |  Header sync, Merkle verification, bloom filter management
+------------------+
         |
+------------------+
|  P2P Network     |  WebSocket bridge (browser) or raw TCP (desktop)
+------------------+
         |
+------------------+
|  Header Store    |  IndexedDB (browser) or SQLite (desktop)
+------------------+
```

**P2P SPV requires a WebSocket-to-TCP bridge** for browser-based wallets since browsers cannot open raw TCP connections to port 18333. Desktop wallets (Rust/Tauri) can connect directly.

### Synchronization Flow (P2P SPV)

1. **Connect**: Send `version` with services=0 (SPV client), receive `verack`

2. **Header sync**:
   - Build block locator from stored headers (or genesis if first sync)
   - Send `getheaders` with locator
   - Receive up to 2000 headers per response
   - Validate each header:
     - `hashPrevBlock` chains correctly
     - `GetPoWHash(header) <= target_from_nBits` (Yespower)
     - Timestamp > median of previous 11 blocks
     - nBits matches expected difficulty at retarget boundaries
     - After height 16000: timewarp protection rules
   - Repeat until < 2000 headers received

3. **Bloom filter setup**:
   - Construct filter containing:
     - All wallet Hash160 values (for P2PKH detection)
     - All wallet public keys (for P2PK detection)
     - All tracked outpoints (for spend detection)
   - Send `filterload`

4. **Transaction discovery**:
   - Send `mempool` to discover unconfirmed matching transactions
   - For historical blocks since last sync, send `getdata` with `MSG_FILTERED_BLOCK`
   - Receive `merkleblock` + matching `tx` messages
   - Verify each merkleblock Merkle proof
   - Build local UTXO set from matched transactions

5. **Ongoing monitoring**:
   - Listen for `inv` messages for new blocks
   - Request new blocks via `getdata` with `MSG_FILTERED_BLOCK`
   - Update local UTXO set
   - When generating new addresses, send `filteradd`

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
- Periodically disconnect and reconnect with fresh filters
- The indexer-backed approach (Option A) has similar privacy tradeoffs -- the indexer sees which addresses the wallet queries

---

## Security Considerations

1. **Eclipse attacks**: Connect to multiple independent peers to reduce risk of being fed a fake chain
2. **Header-only attacks**: Always verify Yespower PoW on headers; reject chains with insufficient cumulative work
3. **Transaction withholding**: A malicious node can omit transactions from merkleblock responses. Connect to multiple peers for redundancy
4. **Bloom filter privacy**: See privacy section above
5. **Minimum confirmations**: Wait for at least 6 confirmations before considering a payment final
6. **Timewarp validation**: Enforce the timewarp protection rules for headers after block 16,000
7. **Indexer trust**: If using an indexer (Option A), the wallet trusts the indexer for balance accuracy. Merkle proofs from `gettxoutproof` can verify individual transactions independently

---

## Files Modified in Node for SPV Support

| File | Changes |
|------|---------|
| `bloom.h` | Bloom filter implementation (`CBloomFilter`) with custom hash function |
| `merkleblock.h` | Partial Merkle tree (`CPartialMerkleTree`) and merkle block (`CMerkleBlock`) |
| `main.h` | `CNode` forward declaration, `CBlockLocator::IsNull()` |
| `main.cpp` | P2P handlers: `getheaders`, `headers`, `filterload`, `filteradd`, `filterclear`, `mempool`; `getdata` support for `MSG_FILTERED_BLOCK` |
| `net.h` | `MSG_FILTERED_BLOCK` inventory type (3); `CBloomFilter* pfilter` and `cs_filter` on `CNode` |
| `rpc.cpp` | RPC commands: `getblockheader`, `sendrawtransaction`, `gettxoutproof`, `verifytxoutproof`, `dumpprivkey`, `importprivkey` |
| `headers.h` | Includes for `bloom.h` and `merkleblock.h` |
