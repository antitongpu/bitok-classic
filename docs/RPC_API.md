# Bitok RPC API

JSON-RPC 1.0 interface. Run `./bitokd help` to list all commands from the running node.

---

## Table of Contents

1. [Connection & Authentication](#connection--authentication)
2. [General Information](#general-information)
3. [Wallet Operations](#wallet-operations)
4. [Address Management](#address-management)
5. [Transaction Operations](#transaction-operations)
6. [Raw Transaction Construction](#raw-transaction-construction)
7. [Script Tools](#script-tools)
8. [Multisig Operations](#multisig-operations)
9. [Hashlock Preimage Operations](#hashlock-preimage-operations)
10. [Mining Operations](#mining-operations)
11. [Network Operations](#network-operations)
12. [Blockchain Operations](#blockchain-operations)
13. [Address Indexer Operations](#address-indexer-operations)
14. [SPV / Merkle Proof Operations](#spv--merkle-proof-operations)
15. [Key Management](#key-management)
16. [Stealth Address Operations](#stealth-address-operations)
17. [Error Handling](#error-handling)
18. [Client Examples](#client-examples)

---

## Connection & Authentication

### Protocol

| Property | Value |
|----------|-------|
| Protocol | JSON-RPC 1.0 |
| Transport | HTTP POST |
| Default Port | 8332 |
| Authentication | HTTP Basic Auth |
| Content-Type | application/json |

### Configuration

Create `bitok.conf` in the data directory:

| OS | Path |
|----|------|
| Linux | `~/.bitokd/bitok.conf` |
| macOS | `~/Library/Application Support/Bitok/bitok.conf` |
| Windows | `%APPDATA%\Bitok\bitok.conf` |

```ini
server=1
rpcuser=yourusername
rpcpassword=yourpassword
rpcport=8332
rpcallowip=127.0.0.1
```

Secure the file on Linux/macOS:
```bash
chmod 600 ~/.bitokd/bitok.conf
```

### CORS

For browser-based clients (web wallets, dApps):

```ini
cors=1
corsorigin=http://localhost:5173
```

When `cors=1` with no `corsorigin`, the server responds with `Access-Control-Allow-Origin: *`. When `corsorigin` is set, only that origin is allowed.

### cURL

```bash
curl --user yourusername:yourpassword \
     --data-binary '{"jsonrpc":"1.0","id":"1","method":"getinfo","params":[]}' \
     -H 'content-type: application/json;' \
     http://127.0.0.1:8332/
```

---

## General Information

### help

List all available RPC commands.

**Parameters:** None

**Returns:** String

```bash
./bitokd help
```

---

### getinfo

General node and wallet status.

**Parameters:** None

**Returns:**
- `balance` — wallet balance in BITOK
- `blocks` — current chain height
- `connections` — peer count
- `proxy` — proxy address if configured
- `generate` — mining active
- `genproclimit` — mining thread count (-1 = all cores)
- `difficulty` — current network difficulty

```bash
./bitokd getinfo
```

```json
{
  "balance": 150.00000000,
  "blocks": 21400,
  "connections": 8,
  "proxy": "",
  "generate": false,
  "genproclimit": -1,
  "difficulty": 1.25000000
}
```

---

### stop

Gracefully shut down the daemon.

**Parameters:** None

**Returns:** String confirmation

```bash
./bitokd stop
```

---

## Wallet Operations

### getbalance

Total confirmed wallet balance.

**Parameters:** None

**Returns:** Number (BITOK)

```bash
./bitokd getbalance
```

---

### sendtoaddress

Send BITOK to an address. Wallet selects inputs automatically and handles fees.

**Parameters:**
1. `address` (string, required) — destination address
2. `amount` (number, required) — amount in BITOK
3. `comment` (string, optional) — local memo
4. `comment-to` (string, optional) — recipient label

**Returns:** txid (string)

```bash
./bitokd sendtoaddress "1ExampleAddressXXXX" 0.5
```

**Fee behavior:** If your inputs have sufficient coin-age (1+ BITOK held ~1 day), the transaction is free. Otherwise the wallet computes the fee (0.01 BITOK per KB minimum) and shows a confirmation dialog (GUI) or deducts from change automatically (daemon).

---

### listtransactions

Recent wallet transactions.

**Parameters:**
1. `label` (string, optional) — filter by label, `"*"` for all (default: `"*"`)
2. `count` (integer, optional) — number of transactions (default: 10)
3. `from` (integer, optional) — skip this many (default: 0)

**Returns:** Array of transaction objects:
- `account` — label
- `address` — address involved
- `category` — `"send"` | `"receive"` | `"generate"`
- `amount` — BITOK amount (negative for sends)
- `fee` — fee paid (sends only)
- `confirmations` — confirmation count
- `txid` — transaction id
- `time` — Unix timestamp
- `timereceived` — when first seen

```bash
./bitokd listtransactions "*" 20 0
```

---

### listunspent

List unspent outputs (UTXOs) in the wallet.

**Parameters:**
1. `minconf` (integer, optional) — minimum confirmations (default: 1)
2. `maxconf` (integer, optional) — maximum confirmations (default: 999999)

**Returns:** Array of UTXO objects:
- `txid` — transaction id
- `vout` — output index
- `address` — address
- `label` — address label
- `scriptPubKey` — output script hex
- `amount` — BITOK
- `confirmations` — confirmation count

```bash
./bitokd listunspent
```

---

### getreceivedbyaddress

Total received by an address (confirmed).

**Parameters:**
1. `address` (string, required)
2. `minconf` (integer, optional, default: 1)

**Returns:** Number (BITOK)

```bash
./bitokd getreceivedbyaddress "1ExampleAddressXXXX" 6
```

---

### getreceivedbylabel

Total received by a label.

**Parameters:**
1. `label` (string, required)
2. `minconf` (integer, optional, default: 1)

**Returns:** Number (BITOK)

---

### listreceivedbyaddress

List received amounts grouped by address.

**Parameters:**
1. `minconf` (integer, optional, default: 1)
2. `includeempty` (boolean, optional, default: false)

**Returns:** Array of objects with `address`, `label`, `amount`, `confirmations`

---

### listreceivedbylabel

List received amounts grouped by label.

**Parameters:**
1. `minconf` (integer, optional, default: 1)
2. `includeempty` (boolean, optional, default: false)

**Returns:** Array of objects with `label`, `amount`, `confirmations`

---

### rescanwallet

Rescan the blockchain to find wallet transactions. Use after importing a private key.

**Parameters:** None

**Returns:** null (runs in background, logs progress)

```bash
./bitokd rescanwallet
```

---

## Address Management

### getnewaddress

Generate a new address in the wallet.

**Parameters:**
1. `label` (string, optional) — associate a label with the address

**Returns:** Address string

```bash
./bitokd getnewaddress "customer-deposits"
```

---

### validateaddress

Check if an address is valid for Bitok.

**Parameters:**
1. `address` (string, required)

**Returns:**
- `isvalid` — boolean
- `address` — canonical form
- `ismine` — whether the wallet controls this address
- `isscript` — whether this is a script address

```bash
./bitokd validateaddress "1ExampleAddressXXXX"
```

```json
{
  "isvalid": true,
  "address": "1ExampleAddressXXXX",
  "ismine": true,
  "isscript": false
}
```

---

### setlabel

Assign a label to an address.

**Parameters:**
1. `address` (string, required)
2. `label` (string, required)

**Returns:** null

```bash
./bitokd setlabel "1ExampleAddressXXXX" "main-deposit"
```

---

### getlabel

Get the label for an address.

**Parameters:**
1. `address` (string, required)

**Returns:** String (label)

---

### getaddressesbylabel

Get all addresses associated with a label.

**Parameters:**
1. `label` (string, required)

**Returns:** Array of address strings

---

## Transaction Operations

### gettransaction

Get wallet transaction details.

**Parameters:**
1. `txid` (string, required)

**Returns:**
- `amount` — net BITOK change in wallet
- `fee` — fee paid
- `confirmations` — confirmation count
- `blockhash` — block containing the tx
- `blockindex` — position in block
- `blocktime` — block timestamp
- `txid`
- `time` — when first seen
- `timereceived`
- `details` — array of inputs/outputs
- `hex` — raw transaction hex

```bash
./bitokd gettransaction "abc123..."
```

---

### getrawtransaction

Get raw transaction hex. Does not require the transaction to be in the wallet.

**Parameters:**
1. `txid` (string, required)
2. `verbose` (integer, optional) — 0 = hex only (default), 1 = decoded object

```bash
./bitokd getrawtransaction "abc123..."
./bitokd getrawtransaction "abc123..." 1
```

**Note:** Only works for transactions in the mempool or if the node has the full block containing the transaction.

---

### sendrawtransaction

Broadcast a raw transaction to the network.

**Parameters:**
1. `hexstring` (string, required) — signed transaction hex

**Returns:** txid (string)

```bash
./bitokd sendrawtransaction "0100000001..."
```

**Rejection reasons:** Malformed hex, invalid signatures, double-spend, below-dust outputs, non-standard scripts (pre-activation), fee too low.

---

### decoderawtransaction

Decode and display transaction fields from raw hex.

**Parameters:**
1. `hexstring` (string, required)

**Returns:**
- `txid`
- `version`
- `locktime`
- `vin` — array of inputs:
  - `txid`, `vout`, `scriptSig` (asm + hex), `sequence`
- `vout` — array of outputs:
  - `value`, `n`, `scriptPubKey` (asm + hex + type + addresses)

```bash
./bitokd decoderawtransaction "0100000001..."
```

---

### getrawmempool

List txids currently in the mempool.

**Parameters:** None

**Returns:** Array of txid strings

```bash
./bitokd getrawmempool
```

---

## Raw Transaction Construction

Full workflow: create unsigned → sign → broadcast.

See [RAW_TRANSACTIONS.md](RAW_TRANSACTIONS.md) for complete examples and multisig workflows.

### createrawtransaction

Create an unsigned transaction. You specify exactly which UTXOs to spend and what outputs to create.

**Parameters:**
1. `inputs` (array, required) — array of `{"txid":"...", "vout":N}` objects
2. `outputs` (object, required) — `{"address": amount, ...}` map

**Returns:** Unsigned transaction hex

```bash
./bitokd createrawtransaction \
  '[{"txid":"abc123...","vout":0}]' \
  '{"1DestAddr...":0.5,"1ChangeAddr...":0.499}'
```

**Notes:**
- Amounts are in BITOK (floating point)
- No change output is added automatically — you must calculate and include it
- The difference between input sum and output sum is the miner fee
- `nLockTime` defaults to 0 (immediately spendable)

---

### signrawtransaction

Sign inputs of a raw transaction.

**Parameters:**
1. `hexstring` (string, required) — unsigned or partially-signed transaction
2. `prevtxs` (array, optional) — previous outputs providing context:
   `[{"txid":"...","vout":N,"scriptPubKey":"hex","amount":val}, ...]`
3. `privkeys` (array, optional) — WIF private keys to sign with (uses wallet keys if omitted)
4. `sighashtype` (string, optional) — `"ALL"` (default), `"NONE"`, `"SINGLE"`, or any with `"|ANYONECANPAY"` suffix

**Returns:**
- `hex` — signed transaction hex
- `complete` — true if all inputs are signed

```bash
./bitokd signrawtransaction "0100000001..."
```

**Sighash types:**

| Type | What is signed |
|------|---------------|
| `ALL` | All inputs and all outputs |
| `NONE` | All inputs, no outputs (outputs can be changed by anyone) |
| `SINGLE` | All inputs, only the output with the same index as this input |
| `ALL\|ANYONECANPAY` | Only this input and all outputs |
| `NONE\|ANYONECANPAY` | Only this input, no outputs |
| `SINGLE\|ANYONECANPAY` | Only this input and the corresponding output |

---

## Script Tools

### decodescript

Decode and classify a script from hex.

**Parameters:**
1. `hex` (string, required) — script hex

**Returns:**
- `asm` — disassembled script
- `type` — classification (see below)
- `reqSigs` — required signatures
- `addresses` — involved addresses
- `p2sh` — P2SH address (not applicable in Bitok, field may appear empty)

```bash
./bitokd decodescript "76a914...88ac"
```

**Script types:** `pubkeyhash`, `pubkey`, `multisig`, `nulldata`, `nonstandard`

---

### analyzescript

Analyze a script for opcode usage, resource limits, and classification.

**Parameters:**
1. `script_hex` (string, required)

**Returns:**
- `asm` — disassembled
- `type` — classification
- `size` — script size in bytes
- `opcount` — non-push opcode count
- `sigops` — signature operation count
- `has_checksig` — boolean
- `has_multisig` — boolean
- `has_data` — boolean (OP_RETURN)
- `within_limits` — boolean (passes all execution limits)
- `errors` — array of limit violations if any

```bash
./bitokd analyzescript "76a914...88ac"
```

**Extended classifications (post-activation):**

| Type | Pattern |
|------|---------|
| `pubkeyhash` | OP_DUP OP_HASH160 \<20 bytes\> OP_EQUALVERIFY OP_CHECKSIG |
| `pubkey` | \<pubkey\> OP_CHECKSIG |
| `multisig` | OP_M \<key1\> ... \<keyN\> OP_N OP_CHECKMULTISIG |
| `nulldata` | OP_RETURN \<data\> |
| `hashlock` | OP_HASH160 \<hash\> OP_EQUALVERIFY OP_CHECKSIG |
| `hashlock-sha256` | OP_SHA256 \<hash\> OP_EQUALVERIFY OP_CHECKSIG |
| `arithmetic` | script containing OP_ADD/OP_SUB/OP_MUL/etc |
| `bitwise` | script containing OP_AND/OP_OR/OP_XOR/etc |
| `cat-covenant` | script using OP_CAT for covenant patterns |
| `splice` | script using OP_SUBSTR/OP_LEFT/OP_RIGHT |
| `nonstandard` | valid but does not match known patterns |

---

### validatescript

Run a script in a sandbox with test inputs. Verify script logic before committing coins.

**Parameters:**
1. `script_hex` (string, required) — the scriptPubKey to test
2. `inputs` (array, required) — array of input hex strings (scriptSig candidates)

**Returns:**
- `valid` — boolean
- `results` — per-input results: `{"input": hex, "result": bool, "error": string}`
- `stack_trace` — execution trace (if available)

```bash
./bitokd validatescript "51935187" '["01"]'
```

---

### buildscript

Assemble a script from a list of opcode names and data pushes.

**Parameters:**
1. `items` (array, required) — mixed array of:
   - Opcode name strings: `"OP_DUP"`, `"OP_HASH160"`, `"OP_CHECKSIG"`, etc. (also without `OP_` prefix)
   - Hex data strings: `"76a914..."` (pushed as data)
   - Number strings: `"5"` (pushed as OP_5)

**Returns:** Script hex

```bash
./bitokd buildscript '["OP_DUP","OP_HASH160","76a914aabbccdd...","OP_EQUALVERIFY","OP_CHECKSIG"]'
```

**Numeric shorthands:** Items like `"0"` through `"16"` push OP_0 through OP_16. Item `"-1"` pushes OP_1NEGATE.

---

### setscriptsig

Inject a custom scriptSig into an input of a raw transaction. Use for manual signing or custom unlocking scripts.

**Parameters:**
1. `rawtx` (string, required) — transaction hex
2. `vin_index` (integer, required) — input index (0-based)
3. `scriptsig` (string or array, required) — one of:
   - Raw hex string of the scriptSig
   - Space-separated opcode/data string: `"OP_0 <sig1hex> <sig2hex>"`
   - Array of items (same format as `buildscript`)

**Returns:** Modified transaction hex

```bash
./bitokd setscriptsig "0100000001..." 0 "OP_0 <sig1> <sig2>"
```

---

### getscriptsighash

Compute the signature hash for an input. Use for external signing (hardware wallets, offline signing).

**Parameters:**
1. `rawtx` (string, required) — transaction hex
2. `vin_index` (integer, required) — input index
3. `scriptpubkey` (string, required) — the scriptPubKey of the output being spent (hex)
4. `sighashtype` (integer, optional) — sighash flag (default: 1 = SIGHASH_ALL)

**Returns:**
- `sighash` — the 32-byte hash to sign (hex)
- `sighashtype` — the type used

```bash
./bitokd getscriptsighash "0100000001..." 0 "76a914...88ac" 1
```

**Sighash type values:** 1=ALL, 2=NONE, 3=SINGLE, 0x81=ALL|ANYONECANPAY, 0x82=NONE|ANYONECANPAY, 0x83=SINGLE|ANYONECANPAY

---

### decodescriptsig

Decode a scriptSig in context of a scriptPubKey, labeling each element's role.

**Parameters:**
1. `scriptsig_hex` (string, required)
2. `scriptpubkey_hex` (string, optional) — context script for role labeling

**Returns:**
- `asm` — disassembled scriptSig
- `elements` — array of `{"hex": "...", "role": "signature|pubkey|preimage|data"}`
- `type` — inferred scriptSig type

```bash
./bitokd decodescriptsig "483045...014104..." "76a914...88ac"
```

---

### verifyscriptpair

Full script verification with real transaction context. Tests whether a scriptSig successfully unlocks a scriptPubKey.

**Parameters:**
1. `scriptsig_hex` (string, required)
2. `scriptpubkey_hex` (string, required)
3. `rawtx` (string, required) — the spending transaction hex
4. `vin_index` (integer, required) — which input to verify

**Returns:**
- `valid` — boolean
- `error` — failure reason if not valid
- `flags` — verification flags applied

```bash
./bitokd verifyscriptpair "483045..." "76a914...88ac" "0100000001..." 0
```

---

## Multisig Operations

See [RAW_TRANSACTIONS.md](RAW_TRANSACTIONS.md) for full multisig workflows.

### createmultisig

Create an m-of-n multisig address and the corresponding redeemScript.

**Parameters:**
1. `nrequired` (integer, required) — m (signatures required)
2. `keys` (array, required) — array of public key hex strings (up to 20 keys)

**Returns:**
- `address` — the P2PKH-style multisig address
- `redeemScript` — the bare multisig script hex (needed for spending)

```bash
./bitokd createmultisig 2 '["pubkey1hex","pubkey2hex","pubkey3hex"]'
```

**Note:** Bitok uses bare multisig (no P2SH). The redeemScript IS the scriptPubKey. Save it — you need it to spend.

---

### addmultisigaddress

Add a multisig address to the wallet for tracking and signing.

**Parameters:**
1. `nrequired` (integer, required)
2. `keys` (array, required) — public key hex strings or wallet addresses

**Returns:** Address string

```bash
./bitokd addmultisigaddress 2 '["pubkey1hex","pubkey2hex"]'
```

---

## Hashlock Preimage Operations

These manage a local store of hash preimages used for hashlock contract spending.

### addpreimage

Store a preimage for a hash. Used when spending a hashlock output.

**Parameters:**
1. `preimage` (string, required) — preimage hex
2. `hash_type` (string, optional) — `"hash160"` (default), `"sha256"`, `"hash256"`

**Returns:**
- `hash` — computed hash hex
- `hash_type` — type used
- `preimage` — stored preimage hex

```bash
./bitokd addpreimage "deadbeef..."
```

---

### listpreimages

List all stored preimages.

**Parameters:** None

**Returns:** Array of `{"hash": "...", "preimage": "...", "hash_type": "..."}` objects

```bash
./bitokd listpreimages
```

---

## Mining Operations

### getmininginfo

Current mining status and network stats.

**Parameters:** None

**Returns:**
- `blocks` — current height
- `currentblocksize` — last block size in bytes
- `currentblocktx` — transactions in last block
- `difficulty` — current difficulty
- `errors` — any alerts
- `generate` — mining active
- `genproclimit` — thread count
- `hashespersec` — current hashrate
- `networkhashps` — estimated network hashrate
- `pooledtx` — mempool size
- `testnet` — false (mainnet)

```bash
./bitokd getmininginfo
```

---

### setgenerate

Start or stop mining.

**Parameters:**
1. `generate` (boolean, required) — true to start, false to stop
2. `genproclimit` (integer, optional) — thread count (-1 = all cores, default)

```bash
./bitokd setgenerate true 4
./bitokd setgenerate false
```

---

### getgenerate

Check if mining is active.

**Parameters:** None

**Returns:** Boolean

```bash
./bitokd getgenerate
```

---

### getblocktemplate

Get a block template for external mining software. Implements BIP 22 (getblocktemplate protocol).

**Parameters:**
1. `jsonrequestobject` (object, optional) — capabilities request (e.g. `{"capabilities":["coinbasetxn"]}`)

**Returns:**
- `version` — block version
- `previousblockhash` — previous block hash
- `transactions` — array of candidate transactions:
  - `data` — tx hex
  - `hash` — txid
  - `fee` — fee in satoshis
  - `sigops` — sigop count
  - `priority` — transaction priority
- `coinbaseaux` — data for coinbase
- `coinbasevalue` — block reward + fees in satoshis
- `target` — difficulty target
- `mintime` — minimum timestamp
- `mutable` — `["time","transactions"]`
- `noncerange` — `"00000000ffffffff"`
- `sigoplimit` — 20000
- `sizelimit` — 1000000
- `curtime` — current time
- `bits` — compact target
- `height` — block height

```bash
./bitokd getblocktemplate
```

---

### submitblock

Submit a solved block.

**Parameters:**
1. `hexdata` (string, required) — full block hex including header and all transactions

**Returns:** null on success, error string on failure

```bash
./bitokd submitblock "0100000020df..."
```

---

### getwork

Legacy getwork protocol for simple miners.

**Parameters:**
1. `data` (string, optional) — block header solution to submit

**Returns (no data):**
- `midstate` — precomputed SHA256 midstate
- `data` — block header data to hash
- `hash1` — SHA256 padding
- `target` — hash target

**Returns (with data):** Boolean (accepted/rejected)

---

## Network Operations

### getconnectioncount

Number of connected peers.

**Parameters:** None

**Returns:** Integer

```bash
./bitokd getconnectioncount
```

---

### getpeerinfo

Detailed information about each connected peer.

**Parameters:** None

**Returns:** Array of peer objects:
- `addr` — peer IP:port
- `services` — service flags
- `lastsend` / `lastrecv` — timestamps
- `conntime` — connection duration
- `version` — protocol version
- `subver` — client string
- `inbound` — true if peer connected to us
- `releasetime` — software version
- `height` — peer's reported height

```bash
./bitokd getpeerinfo
```

---

### getdifficulty

Current network difficulty.

**Parameters:** None

**Returns:** Number

```bash
./bitokd getdifficulty
```

---

## Blockchain Operations

### getblockcount

Current blockchain height (best chain tip).

**Parameters:** None

**Returns:** Integer

```bash
./bitokd getblockcount
```

`getblocknumber` is a synonym.

---

### getbestblockhash

Hash of the current best chain tip.

**Parameters:** None

**Returns:** Hash hex string

```bash
./bitokd getbestblockhash
```

---

### getblockhash

Get block hash at a given height.

**Parameters:**
1. `index` (integer, required) — block height

**Returns:** Hash hex string

```bash
./bitokd getblockhash 18000
```

---

### getblock

Get full block data by hash.

**Parameters:**
1. `hash` (string, required)

**Returns:**
- `hash`
- `confirmations`
- `size`
- `height`
- `version`
- `merkleroot`
- `tx` — array of txids
- `time`
- `nonce`
- `bits`
- `difficulty`
- `previousblockhash`
- `nextblockhash`

```bash
./bitokd getblock "0000abc..."
```

---

### getblockheader

Get block header only (no transactions).

**Parameters:**
1. `hash` (string, required)

**Returns:** Same fields as `getblock` but without the `tx` array.

```bash
./bitokd getblockheader "0000abc..."
```

---

## Address Indexer Operations

The address indexer provides per-address UTXO and transaction history for any address on the chain, not just those in the wallet.

Enable with `-indexer` flag or `indexer=1` in `bitok.conf`. The indexer auto-syncs on startup.

### getindexerinfo

Status of the address indexer.

**Parameters:** None

**Returns:**
- `enabled` — boolean
- `synced` — boolean
- `height` — indexed height
- `bestheight` — chain tip height
- `entries` — number of UTXO entries indexed

```bash
./bitokd getindexerinfo
```

---

### getaddressbalance

Confirmed spendable balance for any address.

**Parameters:**
1. `address` (string, required)

**Returns:**
- `balance` — confirmed balance in satoshis
- `received` — total received (satoshis)

```bash
./bitokd getaddressbalance "1ExampleAddressXXXX"
```

---

### getaddressutxos

Unspent outputs for any address.

**Parameters:**
1. `address` (string, required) — OR `{"addresses": ["addr1","addr2"]}` object

**Returns:** Array of UTXO objects:
- `address`
- `txid`
- `outputIndex` — vout index
- `script` — scriptPubKey hex
- `satoshis` — value in satoshis
- `height` — block height

```bash
./bitokd getaddressutxos "1ExampleAddressXXXX"
```

---

### getaddresstxids

Full transaction id history for an address.

**Parameters:**
1. `address` (string, required) — OR `{"addresses": ["addr1","addr2"]}` object

**Returns:** Array of txid strings (chronological order)

```bash
./bitokd getaddresstxids "1ExampleAddressXXXX"
```

---

## SPV / Merkle Proof Operations

### gettxoutproof

Get a Merkle proof that a transaction is included in a block.

**Parameters:**
1. `txids` (array, required) — array of txid strings
2. `blockhash` (string, optional) — block to prove inclusion in (auto-detected if omitted)

**Returns:** Serialized Merkle proof hex (compact block proof format)

```bash
./bitokd gettxoutproof '["txid1","txid2"]' "blockhash..."
```

---

### verifytxoutproof

Verify a Merkle proof.

**Parameters:**
1. `proof` (string, required) — Merkle proof hex from `gettxoutproof`

**Returns:** Array of txids proven to be in the block

```bash
./bitokd verifytxoutproof "0300000..."
```

---

## Key Management

### dumpprivkey

Export the private key for an address in WIF format.

**Parameters:**
1. `address` (string, required) — must be in wallet

**Returns:** WIF key string

```bash
./bitokd dumpprivkey "1ExampleAddressXXXX"
```

**Warning:** Keep this secret. Anyone with this key controls the coins.

---

### importprivkey

Import a private key into the wallet.

**Parameters:**
1. `privkey` (string, required) — WIF private key
2. `label` (string, optional)
3. `rescan` (boolean, optional, default: true) — whether to rescan the chain

**Returns:** null

```bash
./bitokd importprivkey "5KExampleWIFKeyXXXX" "imported" true
```

Triggers a full blockchain rescan if `rescan` is true. This takes time proportional to chain length.

---

## Stealth Address Operations

Stealth addresses implement Satoshi's "key blinding" concept: the receiver publishes a single stealth address, and each payment to it creates a unique one-time destination address that only the receiver can detect and spend. Outside observers cannot link payments to the stealth address or to each other.

Stealth addresses use **compressed public keys** (33 bytes, `0x02`/`0x03` prefix) for both the scan and spend components. This is the only part of Bitok that uses compressed keys -- all other operations use uncompressed keys. Stealth addresses are encoded with the `ok` prefix (e.g., `ok1A2b3C...`).

See [ANON_PRIVACY.md](ANON_PRIVACY.md) for the full cryptographic design and motivation.

### getnewstealthaddress

Generate a new stealth address for receiving unlinkable payments.

**Parameters:**
1. `label` (string, optional) — label for the address

**Returns:**
- `stealthaddress` — the new stealth address (share this publicly)
- `label` — address label
- `scan_pubkey` — scan public key hex
- `spend_pubkey` — spend public key hex

```bash
./bitokd getnewstealthaddress "donations"
```

```json
{
  "stealthaddress": "ok1A2b3C...",
  "label": "donations",
  "scan_pubkey": "02abc...",
  "spend_pubkey": "03def..."
}
```

Public keys are returned in compressed format (33 bytes hex, starting with `02` or `03`).

---

### liststealthaddresses

List all stealth addresses in the wallet.

**Parameters:** None

**Returns:** Array of stealth address objects:
- `stealthaddress` — the stealth address
- `label` — address label
- `scan_pubkey` — scan public key hex
- `spend_pubkey` — spend public key hex

```bash
./bitokd liststealthaddresses
```

---

### sendtostealthaddress

Send coins to a stealth address. The recipient can detect the payment, but outside observers cannot link it to the stealth address.

**Parameters:**
1. `stealthaddress` (string, required) — destination stealth address
2. `amount` (number, required) — amount in BITOK
3. `comment` (string, optional) — local memo
4. `comment-to` (string, optional) — recipient label

**Returns:**
- `txid` — transaction id
- `dest_address` — the one-time destination address (unique to this payment)
- `ephemeral_pubkey` — ephemeral public key hex (embedded in OP_RETURN)

```bash
./bitokd sendtostealthaddress "ok1A2b3C..." 5.0
```

The transaction contains two relevant outputs: a standard pay-to-pubkey-hash output to the derived one-time address, and an OP_RETURN output containing the compressed ephemeral public key (34 bytes: 1-byte stealth prefix `0x06` + 33-byte compressed key) so the receiver's wallet can detect the payment.

---

### decodestealthaddress

Decode a stealth address and show its component public keys. Works for any stealth address, not just those in your wallet.

**Parameters:**
1. `stealthaddress` (string, required)

**Returns:**
- `valid` — boolean
- `scan_pubkey` — scan public key hex
- `spend_pubkey` — spend public key hex

```bash
./bitokd decodestealthaddress "ok1A2b3C..."
```

Public keys are returned in compressed format.

---

### exportstealthaddress

Export the private keys (scan and spend secrets) for a stealth address. Use to back up or transfer a stealth address to another wallet.

**Parameters:**
1. `stealthaddress` (string, required) — stealth address from your wallet

**Returns:**
- `stealthaddress` — the stealth address
- `label` — address label
- `scan_secret` — WIF-encoded scan private key
- `spend_secret` — WIF-encoded spend private key

```bash
./bitokd exportstealthaddress "ok1A2b3C..."
```

**Warning:** Keep both secrets safe. Anyone with these keys can detect and spend stealth payments sent to this address.

---

### importstealthaddress

Import a stealth address from its private keys (as returned by `exportstealthaddress`). The stealth address is reconstructed from the two secrets, written to the wallet, and the blockchain is rescanned to detect past stealth payments.

**Parameters:**
1. `scan_secret` (string, required) — WIF-encoded scan private key
2. `spend_secret` (string, required) — WIF-encoded spend private key
3. `label` (string, optional) — label for the address
4. `rescan` (boolean, optional, default: true) — whether to rescan the chain for stealth payments

**Returns:**
- `stealthaddress` — the reconstructed stealth address
- `label` — address label
- `scan_pubkey` — scan public key hex
- `spend_pubkey` — spend public key hex
- `rescanned` — whether rescan was performed

```bash
./bitokd importstealthaddress "5KScanSecretWIF..." "5KSpendSecretWIF..." "imported-stealth" true
```

The stealth address is reconstructed from the secrets using compressed public keys.

Triggers a full blockchain rescan if `rescan` is true. The rescan detects all past stealth payments to this address and adds the derived one-time keys to the wallet. Pass `false` to skip the rescan when importing multiple addresses (then run `rescanwallet` once at the end).

---

## Error Handling

### Error Response Format

```json
{
  "result": null,
  "error": {
    "code": -32600,
    "message": "Invalid request"
  },
  "id": "1"
}
```

### Common Error Codes

| Code | Meaning |
|------|---------|
| -1 | General application error |
| -5 | Invalid address |
| -6 | Insufficient funds |
| -7 | Out of memory |
| -8 | Invalid parameter |
| -13 | Wallet locked |
| -14 | Wrong passphrase |
| -22 | Invalid transaction |
| -25 | Rejected (non-standard, fee, double-spend) |
| -26 | Transaction already in chain |
| -27 | Transaction already in mempool |
| -32600 | Invalid JSON-RPC request |
| -32601 | Method not found |
| -32700 | Parse error |

---

## Client Examples

### Python

```python
import requests
import json

class BitokRPC:
    def __init__(self, user, password, host='127.0.0.1', port=8332):
        self.url = f'http://{host}:{port}/'
        self.auth = (user, password)
        self.headers = {'content-type': 'application/json'}
        self.id = 0

    def call(self, method, params=[]):
        self.id += 1
        payload = {
            'jsonrpc': '1.0',
            'id': self.id,
            'method': method,
            'params': params
        }
        response = requests.post(
            self.url,
            data=json.dumps(payload),
            headers=self.headers,
            auth=self.auth
        )
        result = response.json()
        if result.get('error'):
            raise Exception(result['error'])
        return result['result']

rpc = BitokRPC('user', 'pass')
print(rpc.call('getinfo'))
print(rpc.call('getbalance'))
print(rpc.call('getnewaddress', ['deposits']))
```

### PHP

```php
class BitokRPC {
    private $url, $user, $pass, $id = 0;

    public function __construct($user, $pass, $host = '127.0.0.1', $port = 8332) {
        $this->url = "http://{$host}:{$port}/";
        $this->user = $user;
        $this->pass = $pass;
    }

    public function call($method, $params = []) {
        $this->id++;
        $request = json_encode(['jsonrpc'=>'1.0','id'=>$this->id,'method'=>$method,'params'=>$params]);
        $ch = curl_init($this->url);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($ch, CURLOPT_HTTPHEADER, ['Content-Type: application/json']);
        curl_setopt($ch, CURLOPT_POSTFIELDS, $request);
        curl_setopt($ch, CURLOPT_USERPWD, "{$this->user}:{$this->pass}");
        $response = curl_exec($ch);
        curl_close($ch);
        $data = json_decode($response, true);
        if ($data['error']) throw new Exception($data['error']['message']);
        return $data['result'];
    }
}
```

### Node.js

```javascript
const http = require('http');

class BitokRPC {
    constructor(user, pass, host = '127.0.0.1', port = 8332) {
        this.options = {
            hostname: host, port, path: '/',
            method: 'POST',
            auth: `${user}:${pass}`,
            headers: {'Content-Type': 'application/json'}
        };
        this.id = 0;
    }

    call(method, params = []) {
        return new Promise((resolve, reject) => {
            this.id++;
            const body = JSON.stringify({jsonrpc:'1.0',id:this.id,method,params});
            const req = http.request(this.options, res => {
                let data = '';
                res.on('data', c => data += c);
                res.on('end', () => {
                    const r = JSON.parse(data);
                    r.error ? reject(r.error) : resolve(r.result);
                });
            });
            req.on('error', reject);
            req.write(body);
            req.end();
        });
    }
}

const rpc = new BitokRPC('user', 'pass');
rpc.call('getinfo').then(info => console.log(info));
```
