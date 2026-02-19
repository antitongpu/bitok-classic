# Bitok

Bitok runs Bitcoin v0.3.19 under its original assumptions, finishing parts of the early design that were left incomplete.

## What Is This

Bitcoin did not start as "digital gold." That framing came later, after a series of deliberate technical decisions narrowed what Bitcoin was allowed to be. Satoshi designed a peer-to-peer electronic cash system with a full programmable script engine, a priority system where ordinary transactions move for free, and mining that anyone with a CPU could do.

Then the script opcodes were disabled. The priority system was removed. Mining moved to GPUs, then ASICs. Replace-By-Fee broke zero-confirmation payments. The block size war turned a temporary spam limit into permanent doctrine. What remained was a settlement layer for large holders -- not the system Satoshi built.

Bitok starts from Satoshi's last release (Bitcoin v0.3.19, December 2010) and finishes what was left undone:

- **The script engine works.** Every opcode Satoshi wrote -- OP_CAT, OP_MUL, OP_AND, OP_SUBSTR, OP_CHECKMULTISIG, all of them -- runs inside a bounded VM with strict execution limits. Bitcoin Core disabled them. Bitok made them safe.
- **Transactions are free for real users.** Coins held for a day move at no cost. Fees exist only to limit spam. This is Satoshi's priority system, implemented as he designed it.
- **You can mine with your laptop.** Yespower proof-of-work. Memory-hard, CPU-optimized. GPUs and ASICs don't help much.
- **Lightweight wallets work.** SPV header sync, bloom filters, Merkle proofs. Section 8 of the whitepaper, implemented.
- **Mining RPC API with BIP 22 support.** For mining pools integrations.

New genesis block. Separate network. Same 21M cap, same halving schedule, same 10-minute blocks.

## Quick Start

```bash
./bitokd                    # start a node
./bitokd -gen               # start mining
./bitokd getinfo            # check status
```

Mining uses all CPU cores by default. Limit with `-genproclimit=4`.

GUI: `./bitok` -- point and click. Mining toggle in Settings > Options.

## Specifications

| Parameter | Value |
|-----------|-------|
| Algorithm | Yespower 1.0 (CPU-friendly, memory-hard) |
| Block time | 10 minutes |
| Block reward | 50 BITOK, halving every 210,000 blocks |
| Max supply | 21,000,000 |
| Coinbase maturity | 100 blocks |
| P2P port | 18333 |
| RPC port | 8332 |

## Script Engine

In August 2010, Bitcoin Core disabled most scripting opcodes, citing denial-of-service risk. The practical result was the removal of native programmability. Ethereum later rebuilt an entire ecosystem around capabilities Bitcoin already had. OP_CAT alone is still one of the most debated proposals in Bitcoin, more than a decade later.

Bitok keeps every opcode. The VM is bounded by execution limits so it is safe under adversarial conditions:

| Limit | Value |
|-------|-------|
| Max script size | 10,000 bytes |
| Max stack depth | 1,000 items |
| Max element size | 520 bytes |
| Max opcodes per script | 201 |
| Max sigops per block | 20,000 |
| Max multisig keys | 20 |

What this means in practice:

- **OP_CAT** is live. Covenant patterns work on-chain.
- **OP_MUL, OP_DIV, OP_MOD, OP_LSHIFT, OP_RSHIFT** are live. On-chain arithmetic.
- **OP_AND, OP_OR, OP_XOR, OP_INVERT** are live. Bitwise computation.
- **OP_SUBSTR, OP_LEFT, OP_RIGHT** are live. String manipulation.
- **OP_CHECKMULTISIG** is standard. Bare m-of-n multisig relays and confirms. Full wallet support: `createmultisig`, `addmultisigaddress`, partial signing workflows.
- **All sighash types** work. ALL, NONE, SINGLE, each with ANYONECANPAY modifier. Enables crowdfunding, blank checks, modular transaction assembly.
- **OP_RETURN** is provably unspendable (the original was bugged). Safe for data embedding.
- Any well-formed script up to 10KB is standard and relayable.
- **Raw transaction tooling.** `createrawtransaction`, `signrawtransaction`, `decoderawtransaction`, `decodescript` -- full manual transaction construction with multisig and multi-party signing support.
- **Script analysis and testing.** `analyzescript` reports opcode usage, sigop count, limit compliance, and category breakdown. `validatescript` runs scripts in a sandbox with test inputs -- verify your script logic works before committing coins.

Signatures are strict DER with low-S enforcement. Scripts use separated evaluation (scriptSig cannot manipulate scriptPubKey). Minimal push encoding eliminates malleability.

See [SCRIPT_EXEC.md](SCRIPT_EXEC.md) for the full technical specification. See [RAW_TRANSACTIONS.md](RAW_TRANSACTIONS.md) for raw transaction and multisig workflows. See [SCRIPT_TEMPLATES.md](SCRIPT_TEMPLATES.md) for script analysis and testing tools.

## Fee Policy

Bitcoin's original fee system was based on priority, not bidding. Satoshi designed it so that coins held for a day or more move for free, and fees exist only as spam protection. Bitcoin Core removed this system entirely in favor of a pure fee market.

Bitok restores it:

```
priority = sum(input_value * confirmations) / tx_size
```

- Coins held ~1 day at typical amounts qualify for free transactions
- First 27KB of each block reserved for high-priority free transactions
- Remaining space sorted by fee-per-byte
- When fees apply: 0.01 BITOK per KB
- Dust outputs (< 0.01 BITOK) always require a fee

The wallet handles everything automatically. If your coins have been sitting for a day, your transaction is free. If not, the wallet includes the fee and shows a confirmation.

See [FEES.md](FEES.md) for details.

## SPV / Lightweight Wallets

The Bitcoin whitepaper dedicated an entire section to simplified payment verification -- lightweight clients that verify payments without downloading the full chain. Bitok implements this:

- **Header sync** -- `getheaders`/`headers` messages, 2,000 headers per batch
- **Bloom filters** -- `filterload`/`filteradd`/`filterclear` for transaction matching
- **Filtered blocks** -- `merkleblock` messages with Merkle proofs for matched transactions
- **Merkle proofs via RPC** -- `gettxoutproof`/`verifytxoutproof` for independent verification
- **Transaction broadcast** -- `sendrawtransaction` for pre-signed transactions

See [SPV_CLIENT.md](SPV_CLIENT.md) for the full protocol specification and architecture options.

## Address Indexer

An optional per-address UTXO and transaction index for web3, dApps, exchange deposit tracking, and any service that needs history for arbitrary addresses.

Enable with `-indexer` (or `indexer=1` in `bitok.conf`). The indexer is fully automatic — on first start, or whenever it falls behind the chain tip, it rebuilds itself without any manual intervention.

The index stores **only unspent outputs**. Spent outputs are removed immediately when a transaction is confirmed, which keeps the database size very small regardless of chain history length.

RPC methods available when indexer is active:

| Method | Description |
|--------|-------------|
| `getindexerinfo` | Indexer status, height, and sync state |
| `getaddressbalance <addr>` | Confirmed spendable balance of any address |
| `getaddressutxos <addr>` | Unspent outputs for any address |
| `getaddresstxids <addr>` | Full transaction history for any address |

See [RPC_API.md](RPC_API.md) (Address Indexer Operations) for full reference.

## Key Management

**RPC:**
```bash
./bitokd dumpprivkey <address>          # export private key (WIF)
./bitokd importprivkey <wif> [label]    # import key, triggers rescan
```

**GUI:**
- Right-click any address in the address book to export its private key
- Import via File menu or address book with WIF input dialog
- Import triggers a progress-tracked blockchain rescan

Back up `wallet.dat`. If you lose it, coins are gone. There is no recovery.

## Building

### Ubuntu 24.04

```bash
sudo apt-get install build-essential libssl-dev libdb5.3-dev libboost-all-dev

# Daemon only
make -f makefile.unix

# With GUI
sudo apt-get install libwxgtk3.2-dev libgtk-3-dev
make -f makefile.unix gui
```

### Other Platforms

- [BUILD_UNIX.md](BUILD_UNIX.md) -- Linux/BSD
- [BUILD_MACOS.md](BUILD_MACOS.md) -- macOS
- [BUILD_WINDOWS_MINGW.md](BUILD_WINDOWS_MINGW.md) -- Windows MinGW/MSYS2
- [BUILD_WINDOWS_VC.md](BUILD_WINDOWS_VC.md) -- Windows Visual Studio

## Running

**Daemon:**
```bash
./bitokd                          # run node
./bitokd -gen                     # run node + mine
./bitokd -daemon                  # background mode
./bitokd stop                     # stop daemon
./bitokd -indexer                 # enable address/UTXO indexer (auto-syncs on start)
```

**Configuration file location:**

| OS | Path |
|----|------|
| Linux | `~/.bitokd/bitok.conf` |
| macOS | `~/Library/Application Support/Bitok/bitok.conf` |
| Windows | `%APPDATA%\Bitok\bitok.conf` |

Example config:
```ini
server=1
rpcuser=user
rpcpassword=pass
gen=1
addnode=1.2.3.4
```

For browser-based clients (web3 dApps, web wallets, dashboards), enable CORS:
```ini
cors=1
corsorigin=http://localhost:5173
```

See [RPC_API.md](RPC_API.md) for full CORS configuration details.

**RPC:**
```bash
./bitokd getinfo                  # node status
./bitokd getbalance               # wallet balance
./bitokd getnewaddress            # new receiving address
./bitokd sendtoaddress <addr> <amount>
./bitokd listtransactions         # recent transactions
./bitokd listunspent              # wallet UTXOs
./bitokd help                     # list all commands
```

See [RPC_API.md](RPC_API.md) for the full API reference.

## Mining

Satoshi wrote in 2009: *"It's nice how anyone with just a CPU can compete fairly equally right now."* That stopped being true a year later. Bitok makes it true again.

```bash
./bitokd -gen                     # all cores
./bitokd -gen -genproclimit=4     # 4 cores
```

Yespower uses ~128KB of memory per hash. This is what makes GPUs inefficient. SSE2/AVX/AVX2 auto-detected, no configuration needed.

Pool operators: see [POOL_INTEGRATION.md](POOL_INTEGRATION.md). The node supports `getblocktemplate` (BIP 22) and `getwork`.

See [BITOKPOW.md](BITOKPOW.md) for algorithm details.

## Data Directory

| OS | Path |
|----|------|
| Linux | `~/.bitokd/` |
| macOS | `~/Library/Application Support/Bitok/` |
| Windows | `%APPDATA%\Bitok\` |

## Peer Discovery

IRC bootstrap via `irc.libera.chat` channel #bitok, same mechanism as original Bitcoin.

Manual peer addition:
```bash
./bitokd -addnode=<ip>
```

## Security

**Satoshi-era security fixes (included from launch):**
- Value overflow protection (184B coin bug)
- Blockchain checkpoints (blocks 0, 6666, 14000, 16000)
- DoS limits (connection/message size/rate limiting)

**Modern network security:**
- **Time warp attack protection** - validates timestamps at difficulty boundaries
- **DNS seed infrastructure** - reliable peer discovery via seed1/2/3.bitokd.run with hardcoded fallback
- **External IP detection** - learned from peers via P2P, no HTTP dependencies
- **Anchor connections** - saves 2 longest-lived peers, reconnects on restart (eclipse attack resistance)
- **Network group diversity** - groups peers by /16 subnet, limits 2 outbound + 8 inbound per group (sybil resistance)

**Script execution hardening:**
- Strict DER signature validation
- Low-S signature enforcement (anti-malleability)
- SIGHASH_SINGLE out-of-range fix (prevents coin theft vector)
- CHECKMULTISIG NULLDUMMY enforcement (anti-malleability)
- OP_RETURN hard failure (original was bugged)
- Separated evaluation (scriptSig cannot manipulate scriptPubKey)
- Bounded execution (prevents CPU/memory exhaustion via scripts)

**Script standardness:** Any well-formed script up to 10KB is standard and relayable. All opcodes are available for use.

## Documentation

### Protocol
- [SCRIPT_EXEC.md](SCRIPT_EXEC.md) -- Script engine: execution limits, separated evaluation, signature rules
- [FEES.md](FEES.md) -- Priority-based fee policy
- [SPV_CLIENT.md](SPV_CLIENT.md) -- SPV lightweight client protocol specification
- [SECURITY_FIXES.md](SECURITY_FIXES.md) -- Security hardening details

### Mining
- [BITOKPOW.md](BITOKPOW.md) -- Yespower proof-of-work
- [SOLO_MINING.md](SOLO_MINING.md) -- Solo mining with cpuminer
- [POOL_INTEGRATION.md](POOL_INTEGRATION.md) -- Mining pool integration
- [MINING_OPTIMIZATIONS.md](MINING_OPTIMIZATIONS.md) -- Performance tuning

### API
- [RPC_API.md](RPC_API.md) -- Complete JSON-RPC reference
- [RAW_TRANSACTIONS.md](RAW_TRANSACTIONS.md) -- Raw transactions, multisig, and signing workflows
- [SCRIPT_TEMPLATES.md](SCRIPT_TEMPLATES.md) -- Script analysis and sandbox testing
- [RPC_MINING_IMPLEMENTATION.md](RPC_MINING_IMPLEMENTATION.md) -- Mining RPC internals

### Building
- [BUILD_UNIX.md](BUILD_UNIX.md) -- Linux/BSD
- [BUILD_MACOS.md](BUILD_MACOS.md) -- macOS
- [BUILD_WINDOWS.md](BUILD_WINDOWS.md) -- Windows overview
- [BUILD_WINDOWS_MINGW.md](BUILD_WINDOWS_MINGW.md) -- Windows MinGW/MSYS2
- [BUILD_WINDOWS_VC.md](BUILD_WINDOWS_VC.md) -- Windows Visual Studio

### Development
- [BITOK_DEVELOPMENT.md](BITOK_DEVELOPMENT.md) -- Design choices and development notes
- [GENESIS.md](GENESIS.md) -- Genesis block details
- [CHANGELOG.md](CHANGELOG.md) -- Version history

## License

MIT, same as original Bitcoin. See [license.txt](license.txt).

## Author

Tom Elvis Jedusor
