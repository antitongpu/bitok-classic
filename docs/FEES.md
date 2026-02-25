# Fee Policy

**Status:** Active since block 18,000

---

## Philosophy

Bitok restores Satoshi Nakamoto's original fee design. In the early Bitcoin codebase, fees existed to limit spam -- not to require payment for every transaction.

> "There is no need to require a fee for every transaction." -- Satoshi Nakamoto

Satoshi defined transaction priority as:

> "sum(valuein * age) / txsize"

This formula allowed old, small transactions to be processed without fees when blocks were not full. Fees served only as a secondary sorting mechanism when block space was contested.

Modern Bitcoin abandoned this model entirely, replacing it with a pure fee market where every transaction competes on price. Small payments became impractical.

Bitok restores the original model:

- Priority ordering based on coin age
- Block space reserved for high-priority free transactions
- Fees used only as a secondary sorting mechanism
- Dust outputs still require fees to prevent UTXO bloat

This discourages spam while keeping small payments practical.

---

## Active Fee Rules

Priority-based fees are active (since block 18,000):

1. **Transaction priority is computed using Satoshi's coin-age formula**
2. **Block assembly is priority-sorted with reserved free space**
3. **Mempool relay enforces priority-aware fee rules**

Nothing in this system is consensus-enforced. Block validation does not check minimum fees (same as original Bitcoin). Fee rules are policy only -- they govern relay and mining, not block validity.

---

## Priority Calculation

```
priority = sum(value_in * confirmations) / tx_size
```

For each input in a transaction:
- `value_in` is the amount of the coin being spent (in satoshis)
- `confirmations` is the number of blocks since that coin was confirmed
- `tx_size` is the serialized transaction size in bytes

The sum is taken across all inputs, then divided by transaction size.

### Examples

| Scenario | Value | Age | Tx Size | Priority | Free? |
|----------|-------|-----|---------|----------|-------|
| 1 BITOK held 1 day | 100,000,000 | 144 blocks | 250 bytes | 57,600,000 | Yes |
| 0.1 BITOK held 10 days | 10,000,000 | 1,440 blocks | 250 bytes | 57,600,000 | Yes |
| 0.5 BITOK held 3 days | 50,000,000 | 432 blocks | 250 bytes | 86,400,000 | Yes |
| 0.01 BITOK held 1 day | 1,000,000 | 144 blocks | 250 bytes | 576,000 | No |
| 1 BITOK just received | 100,000,000 | 1 block | 250 bytes | 400,000 | No |
| Spam (tiny, fresh) | 100,000 | 1 block | 250 bytes | 400 | No |

The threshold for free transactions is **57,600,000** (equivalent to 1 BITOK confirmed for 1 day in a 250-byte transaction).

Unconfirmed inputs (still in mempool) contribute zero priority.

---

## Block Assembly

After activation, miners assemble blocks in two phases:

### Phase 1: Priority Space (first 27,000 bytes)

The first 27KB of each block is reserved for high-priority free transactions. Transactions are sorted by priority (highest first) and included if:

- Their priority meets or exceeds the free threshold (57,600,000)
- They fit within the 27KB priority space
- They have no dust outputs (or pay the dust fee if they do)

This guarantees that honest users spending aged coins always have room in blocks, regardless of fee-market congestion.

### Phase 2: Fee Space (remaining block space)

After the priority space is filled, remaining block space is filled by fee-paying transactions sorted by **fee per byte** (highest first). These transactions must meet the standard minimum fee.

This means miners maximize revenue from the fee portion while still serving free high-priority transactions.

---

## Fee Schedule

| Condition | Fee Required |
|-----------|-------------|
| High priority (>= threshold) and tx < 1KB | 0 (free) |
| High priority (>= threshold) and tx >= 1KB | 0.01 BITOK per KB |
| Low priority | 0.01 BITOK per KB |
| Any output < 0.01 BITOK (dust) | 0.01 BITOK minimum, regardless of priority |

The dust rule prevents UTXO set pollution. Creating many tiny outputs is the cheapest form of permanent blockchain spam, so it always requires a fee.

---

## Wallet Behavior

### GUI

When a user sends coins through the GUI:

1. The wallet selects coins and builds the transaction
2. It computes the transaction's priority from the selected inputs
3. If priority is high enough and the transaction is small, the fee is zero
4. If a fee is required, a confirmation dialog appears: "This transaction requires a fee of X. Do you want to pay the fee?"
5. The user confirms or cancels
6. The fee is automatically included in the transaction (deducted from change)

Users do not need to manually calculate or enter fees. The wallet handles everything automatically. Users holding coins for a day or more will typically see zero fees for normal sends.

### Daemon

Same logic as GUI, but the fee confirmation dialog is skipped (daemon auto-accepts). The `-paytxfee` flag can set a voluntary minimum fee above zero.

### Options

Users can set a voluntary transaction fee in the Options dialog. This is a floor -- the wallet will never pay less than GetMinFee requires, but may pay more if the user sets a higher voluntary fee.

---

## Script Transactions

After block 18,000, all original opcodes are re-enabled (see `SCRIPT_EXEC.md`). Script transactions interact with the fee system naturally:

- **Fee is size-based.** Complex scripts produce larger transactions, so they pay proportionally more (0.01 BITOK per KB).
- **Priority penalizes size.** The denominator in `priority = value * age / tx_size` means a 5KB script transaction needs 5x more coin-age than a 1KB simple transaction to qualify for free.
- **Execution limits prevent DoS.** Scripts are bounded by opcode count (201), stack size (1,000), element size (520 bytes), script size (10,000 bytes), and sigops per block (20,000). These limits make it impossible to create a computationally expensive script that is cheap in bytes.

The combination of size-based fees and execution limits ensures scripts cannot be used for spam attacks.

---

## Anti-Spam Properties

The priority system creates a layered defense against spam:

| Attack | Defense |
|--------|---------|
| Many small transactions with fresh coins | Low priority (no age), requires fees |
| Many small transactions with dust outputs | Dust rule forces 0.01 BITOK fee per tx |
| Large script transactions | Size-based fees scale with tx size |
| Flooding mempool with free txs | Priority threshold rejects low-priority free txs at relay |
| Competing with honest users for block space | 27KB reserved for high-priority free txs; spam must outbid in fee space |
| Creating tiny UTXOs to bloat the UTXO set | Dust threshold prevents outputs < 0.01 BITOK without fee |

Honest users spending coins they have held for a reasonable time (a day or more for typical amounts) transact for free. Spammers must pay.

---

## Network Compatibility

### No Consensus Change

Fee enforcement is **not** part of consensus in Bitok (nor was it in original Bitcoin). When a node validates a received block, it checks:

- Each transaction's fees are non-negative
- The coinbase does not claim more than subsidy + total fees

It does **not** check that each transaction meets a minimum fee. This means:

- Blocks mined by new nodes are valid to old nodes
- Blocks mined by old nodes are valid to new nodes
- No chain split is possible from fee changes

### Relay Policy Difference

After block 18,000, new nodes apply priority-aware relay rules. A free transaction with low priority that an old node would relay may be rejected by a new node. However:

- The transaction can still reach old miners through old node relay paths
- If any miner includes it in a block, all nodes accept the block
- This is a policy difference, not a consensus difference

### Mining Pools

The `getblocktemplate` RPC returns per-transaction fee and priority fields after activation. Pool software can use this data for monitoring. The block template itself is already priority-sorted, so pools benefit automatically without software changes.

---

## Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| `DEFAULT_BLOCK_PRIORITY_SIZE` | 27,000 bytes | Block space reserved for high-priority free transactions |
| `FREE_PRIORITY_THRESHOLD` | 57,600,000 | Minimum priority for free relay and block inclusion |
| `DUST_THRESHOLD` | 0.01 BITOK (1,000,000 satoshis) | Outputs below this always require a fee |
| `SCRIPT_EXEC_ACTIVATION_HEIGHT` | 18,000 | Block height at which priority fees activate |

---

## Files Modified

| File | Changes |
|------|---------|
| `main.h` | Added priority constants, updated `GetMinFee()` with `fAllowFree` parameter, declared `ComputePriority()` |
| `main.cpp` | Added `ComputePriority()` function, priority-sorted block assembly in `BitcoinMiner()` and `CreateNewBlock()`, priority-aware relay in `AcceptTransaction()`, priority-aware wallet fee in `CreateTransaction()` |
| `rpc.cpp` | `getblocktemplate` now reports actual fees and priority per transaction |
| `ui.cpp` | Updated fee confirmation dialog text |
