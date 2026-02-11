# Bitok

Bitok runs Bitcoin v0.3.19 under its original assumptions and completes design elements present in the early codebase that were never fully finished.

It is not a fork of modern Bitcoin Core.  
It is a continuation of the last release developed with Satoshi’s direct participation, updated to operate safely on a modern network.

---

## Project Direction

Bitcoin v0.3.19 contained a full script engine, a priority-based fee model, early SPV infrastructure, payment channel primitives, and publish/subscribe networking code.  
Over time, many of these capabilities were disabled, removed, or redefined.

Bitok’s approach is simple:

- Preserve the economic model (21M supply, 10-minute blocks, halving schedule)
- Preserve the original scripting surface
- Preserve zero-confirmation and priority semantics
- Bound and secure the system rather than remove functionality

Where risks existed (unbounded script execution, malleability vectors, consensus inconsistencies), Bitok adds deterministic limits and security fixes — without neutering the original design.

---

# Core Features

## Script Engine (SCRIPT_EXEC)

All original Satoshi-era opcodes remain operational, including:

- OP_CAT
- OP_MUL / OP_DIV / OP_MOD
- OP_LSHIFT / OP_RSHIFT
- OP_AND / OP_OR / OP_XOR / OP_INVERT
- OP_SUBSTR / OP_LEFT / OP_RIGHT
- OP_CHECKMULTISIG

The virtual machine is now bounded and hardened.

### Execution Limits

| Limit | Value |
|-------|-------|
| Max script size | 10,000 bytes |
| Max stack depth | 1,000 |
| Max element size | 520 bytes |
| Max opcodes per script | 201 |
| Max sigops per block | 20,000 |
| Max multisig keys | 20 |

### Security Hardening

- Strict DER signature enforcement
- Low-S normalization
- Minimal push encoding
- Separated evaluation (scriptSig cannot affect scriptPubKey execution)
- CHECKMULTISIG NULLDUMMY enforcement
- SIGHASH_SINGLE out-of-range fix
- OP_RETURN provably unspendable fix
- OP_VER / OP_VERIF / OP_VERNOTIF disabled (consensus safety)

Full specification:  
[SCRIPT_EXEC.md](SCRIPT_EXEC.md)

---

## Priority-Based Fee Model

Restores Satoshi’s original transaction priority system:

```
priority = sum(input_value * confirmations) / tx_size
```

- First 27KB of each block reserved for high-priority transactions
- Remaining block space sorted by fee-per-byte
- Dust (< 0.01 BITOK) requires fee
- Fee rate: 0.01 BITOK/KB when applicable

Fee rules are policy, not consensus.  
See: [FEES.md](FEES.md)

---

## SPV Support (Section 8 Whitepaper)

Full node-side support for lightweight clients:

- getheaders / headers
- Bloom filters (filterload, filteradd, filterclear)
- Filtered blocks (merkleblock)
- MSG_FILTERED_BLOCK inventory type
- gettxoutproof / verifytxoutproof RPC
- sendrawtransaction RPC
- getblockheader RPC

Specification:  
[SPV_CLIENT.md](SPV_CLIENT.md)

---

## Mining

### Proof of Work

- Algorithm: Yespower 1.0
- Memory-hard, CPU-oriented
- ~128KB per hash
- SSE2 / AVX / AVX2 auto-detected

See: [BITOKPOW.md](BITOKPOW.md)

### Mining RPC API

- getblocktemplate (BIP 22 compliant)
- getwork (legacy support)
- Full mining pool integration support

See:
- [POOL_INTEGRATION.md](POOL_INTEGRATION.md)
- [RPC_MINING_IMPLEMENTATION.md](RPC_MINING_IMPLEMENTATION.md)

---

## Network & Security Enhancements

Bitok integrates critical security and reliability improvements while preserving original protocol semantics.

### Consensus & Chain Safety

- Value overflow protection (184B bug mitigation)
- Time Warp attack protection
- Blockchain checkpoints
- Deterministic script limits
- Consensus malleability fixes

See: [SECURITY_FIXES.md](SECURITY_FIXES.md)

### Network Improvements

- DNS seed peer discovery
- IRC bootstrap support
- Privacy-preserving IP detection
- Network group diversity enforcement
- Connection limiting and DoS protections

---

## Key Management

RPC:

```
dumpprivkey <address>
importprivkey <wif> [label] [rescan]
rescanwallet
```

GUI:

- Private key export from address book
- Private key import with progress-tracked rescan
- Wallet.dat fully compatible
- No wallet migration required

---

# Specifications

| Parameter | Value |
|-----------|-------|
| Block time | 10 minutes |
| Block reward | 50 BITOK |
| Halving interval | 210,000 blocks |
| Max supply | 21,000,000 |
| Coinbase maturity | 100 blocks |
| P2P port | 18333 |
| RPC port | 8332 |

New genesis block. Separate network.

---

# Building

See platform-specific guides:

- [BUILD_UNIX.md](BUILD_UNIX.md)
- [BUILD_MACOS.md](BUILD_MACOS.md)
- [BUILD_WINDOWS_MINGW.md](BUILD_WINDOWS_MINGW.md)
- [BUILD_WINDOWS_VC.md](BUILD_WINDOWS_VC.md)

---

# Running

Daemon:

```
./bitokd
./bitokd -gen
./bitokd -daemon
./bitokd stop
```

GUI:

```
./bitok
```

RPC reference:  
[RPC_API.md](RPC_API.md)

---

# Documentation Index

### Protocol
- [SCRIPT_EXEC.md](SCRIPT_EXEC.md)
- [SPV_CLIENT.md](SPV_CLIENT.md)
- [FEES.md](FEES.md)
- [SECURITY_FIXES.md](SECURITY_FIXES.md)

### Mining
- [BITOKPOW.md](BITOKPOW.md)
- [SOLO_MINING.md](SOLO_MINING.md)
- [POOL_INTEGRATION.md](POOL_INTEGRATION.md)
- [MINING_OPTIMIZATIONS.md](MINING_OPTIMIZATIONS.md)

### Development
- [BITOK_DEVELOPMENT.md](BITOK_DEVELOPMENT.md)
- [GENESIS.md](GENESIS.md)
- [CHANGELOG.md](CHANGELOG.md)

---

# License

MIT License (same as original Bitcoin).  
See: license.txt

---

Author: Tom Elvis Jedusor