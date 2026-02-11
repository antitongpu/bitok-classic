# Notes on BITOK development

These notes describe design choices and implementation details for BITOK, starting from Bitcoin v0.3.19.
The goal is to preserve the original operating assumptions and extend the system conservatively, without altering its basic transaction model.

---

## On the Purpose of the System

The system was described as a **peer-to-peer electronic cash system**.
This implies direct transactions between users, without reliance on intermediaries or external systems for routine payments.

The design assumes:
- on-chain transactions are normal
- fees are optional when capacity is available
- verification is possible without specialized hardware
- complexity is minimized where possible

BITOK follows these assumptions.

---

## Notes on Transaction Replacement

> "We should not allow replacing transactions. That would make transactions unreliable.", - SN.

The intended behavior was first-seen acceptance: once a transaction is broadcast and relayed, it should either confirm or fail, but not be superseded by another version.

BITOK enforces this directly:

```cpp
if (pool.exists(tx.GetHash()))
    return error("transaction already in mempool");
```

This simplifies reasoning for users and merchants and allows zero-confirmation transactions to be meaningful for small payments.

---

## Notes on Block Size Limits

The block size limit was introduced as a precaution against denial-of-service attacks.

It was not intended to be permanent.

> "It can be phased in, like: if (blocknumber > 115000) maxblocksize = largerlimit.", - SN.

The original idea was to schedule increases far in advance, so that older versions would naturally be obsolete by the time a change activated.

BITOK treats block size as a configurable parameter, adjustable by coordinated upgrade when usage and hardware warrant it.

---

## Notes on Transaction Structure

The original transaction format includes signatures as part of the input script, and the transaction ID is defined as the hash of the entire transaction.

```
txid = hash(serialized transaction)
```

This property is preserved in BITOK.

No separate witness data, alternate hashes, or weight accounting are introduced.
There is a single transaction identifier and a single serialization format.

This keeps transaction handling simple and auditable.

---

## Notes on Fees and Priority

Fees were introduced to limit spam, not to require payment for every transaction.

> "There is no need to require a fee for every transaction.", - SN.

Priority was defined as:

> "sum(valuein * age) / txsize", - SN.

This allowed old, small transactions to be processed without fees when blocks were not full.

BITOK restores this model:
- priority ordering based on coin age
- block space reserved for high-priority free transactions
- fees used only as a secondary sorting mechanism

This discourages spam while keeping small payments practical.

---

## Notes on Mining Participation

Mining was expected to be accessible initially.

> "It’s nice how anyone with just a CPU can compete fairly equally right now.", - SN.

This was not a performance claim, but a participation assumption.

BITOK uses a memory-hard, CPU-oriented proof-of-work (Yespower) to preserve this property and reduce dependence on specialized hardware.

---

## Notes on Script and Opcodes

Early Script included additional operations, such as string manipulation and bitwise operators:

```cpp
OP_CAT
OP_SUBSTR
OP_LEFT
OP_RIGHT
OP_AND
OP_OR
OP_XOR
OP_INVERT
```

These were disabled as a safety precaution, not because they were fundamentally unsound.

BITOK plans enable these operations and explicit resource limits to prevent abuse:
- maximum operation count
- maximum stack size
- maximum element size

This follows the original approach: **capability with bounds**.

---

## Notes on Network Policy

Early relay policy was intentionally simple. Valid transactions were relayed with minimal filtering.

> "You don’t need to have rules for every scenario.", - SN.

BITOK keeps relay behavior close to consensus validity, avoiding complex policy layers that affect transaction propagation independently of consensus.

---

## Notes on Lightweight Clients

The original design includes simplified payment verification.

> "The design outlines a lightweight client that does not need the full block chain.", - SN.

BITOK plans support for header-only synchronization and Merkle proofs, allowing users to verify payments without storing the full chain, while merchants run full nodes.

---

## Notes on Escrow and Multi-Signature

Script directly supports escrow transactions.

> "The script actually directly expresses the concept of an escrow transaction.", - SN.

BITOK includes standard templates and wallet support for multi-signature transactions, enabling trust-minimized commerce without intermediaries.

---

## Notes on Cryptography and Complexity

The original system uses ECDSA over secp256k1 for transaction signatures.
This is preserved. No additional signature schemes, script versions, or transaction encodings are introduced.

The Proof-of-Work algorithm has been changed from SHA256d to Yespower v1.0,
a memory-hard function designed to resist ASIC and GPU advantage.
This is the one new cryptographic assumption introduced by the project.

Simplicity and auditability are preferred over unnecessary complexity.

---

## Notes on Governance

The system was designed so that consensus is defined by running software, not by process documents.

Changes were expected to be:
- rare
- conservative
- implemented in code
- adopted voluntarily

BITOK follows this approach. There is no external governance mechanism beyond software release and adoption.

---

## Summary of Design Choices

- Original transaction format preserved  
- First-seen transaction relay  
- Adjustable block size via planned upgrades  
- Optional fees with priority system  
- CPU-oriented mining  
- Simple relay policy  
- Bounded Script with useful operations  
- On-chain transactions as the default

These choices are not innovations. They are continuations.


