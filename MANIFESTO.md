# BITOK Manifesto

## Bitcoin After 0.3.19

Notes on What Changed, and Why BITOK Continues from There

---

People sometimes ask what "Bitcoin was supposed to be."

That question usually comes up long after the original design constraints have been forgotten. Bitcoin was not designed as a finished product. It was designed as a working system that could be adjusted once it was running in the real world.

Version 0.3.19 was the last release where Satoshi was still directly involved. By that point, Bitcoin had real users, real attacks, and real tradeoffs. The system was already working, but it was still early.

BITOK starts from that point.

---

## On Limits (Block Size, Script, Policy)

Many limits in Bitcoin were added for safety, not ideology.

### Block Size

The 1 MB block size limit was introduced to prevent denial-of-service attacks while the network was small. It was not meant to be permanent.

Satoshi explained this clearly at the time:

> "It can be phased in, like:
> if (blocknumber > 115000) maxblocksize = largerlimit.
> It can start being in versions way ahead, so by the time it becomes an issue, the older versions are already obsolete."

The idea was simple: when usage approaches the limit and hardware has improved, the limit can be raised. Old software upgrades. This is normal engineering.

Here is the relevant code style from that era (simplified):

```cpp
// Temporary anti-DoS block size limit
if (nBlockSize > MAX_BLOCK_SIZE)
    return error("block too large");
```

This was a guardrail, not a constitution.

BITOK treats block size the same way: as a parameter that exists to protect the network, not to freeze it forever.

### Script and Opcodes

Bitcoin Script was designed to be deliberately limited. It is not Turing complete. That was intentional. At the same time, it was not meant to be useless.

Early versions included more operations than are commonly usable today, including bitwise and splice operations. Some were disabled later because they were risky in an immature network.

For example, early Script defined operations such as:

```
OP_CAT
OP_SUBSTR
OP_LEFT
OP_RIGHT
OP_AND
OP_OR
OP_XOR
OP_INVERT
```

These were disabled because they made it easier to create scripts that consumed excessive resources or crashed nodes. Disabling them was a safety decision, not a statement that Script should never evolve.

Script was meant to be simple, bounded, and expandable when safe.

BITOK preserves this posture. It does not assume that "disabled once" means "forbidden forever." It assumes that safety comes from careful limits, not from permanent paralysis.

---

## On Fees

Transaction fees were added to prevent spam, not to require payment for every transaction.

Satoshi said this directly:

> "There is no need to require a fee for every transaction."

In early Bitcoin, transactions could be relayed and mined without fees if blocks were not full. Priority was determined by age and size, not bidding wars.

The relevant logic looked like this:

```cpp
// Allow free transactions if they are small and old enough
if (tx.nFee < MIN_TX_FEE && priority < COIN * 144 / 250)
    return error("insufficient priority");
```

Fees were meant to become relevant only when necessary.

BITOK keeps fees optional by design. With low congestion and simple relay rules, fees return to their original purpose: spam resistance, not gatekeeping.

---

## On Transaction Replacement

One thing Satoshi explicitly did not want was transaction replacement.

> "We should not allow replacing transactions. That would make transactions unreliable."

Once a transaction is broadcast, users should be able to rely on it either confirming or failing cleanly. If transactions can be replaced freely, the system becomes harder to reason about, especially for merchants.

BITOK keeps the earlier assumption intact: broadcasting a transaction is a commitment, not the first move in a negotiation.

---

## On Relay and Mempool Policy

Early Bitcoin nodes relayed valid transactions with minimal filtering. The idea was that consensus rules define validity, not a growing list of local policies.

Satoshi cautioned against over-engineering rules:

> "You don't need to have rules for every scenario."

Over time, modern Bitcoin implementations added many non-consensus relay rules: ancestor limits, fee-based eviction, replacement logic, and package policies. These are not consensus rules, but they strongly shape network behavior.

BITOK keeps relay behavior closer to validity-first principles. If a transaction is valid and safe to process, it should propagate.

---

## On Mining and Participation

Mining was originally designed to be accessible.

> "It's nice that anyone with a CPU can mine."

This was not about performance. It was about participation. A system where many people can contribute hash power is easier to understand, easier to join, and harder to ossify socially.

Satoshi knew GPU mining would eventually dominate SHA-256:

> "We should have a gentleman's agreement to postpone the GPU arms race as long as we can for the good of the network. It's much easier to get new users up to speed if they don't have to worry about GPU drivers and compatibility."

And:

> "GPUs are much less evenly distributed, so the generated coins only go towards rewarding 20% of the people for joining the network instead of 100%."

He understood the problem. He just didn't have a solution at the time.

BITOK uses Yespower, a CPU-friendly proof-of-work, to keep mining approachable. This matches the early assumptions better than an ecosystem dominated by specialized hardware and large pools.

---

## On Usefulness

The title of the paper was:

> "Bitcoin: A Peer-to-Peer Electronic Cash System"

That does not mean Bitcoin cannot also be a store of value. It means that usefulness in everyday transactions was a core design goal.

Small payments, experimentation, and direct use were expected to happen on-chain, especially while the system was young.

BITOK continues with that expectation.

---

## Why BITOK Exists

BITOK is not an attempt to "fix" Bitcoin.

It is an attempt to continue it from a specific point in time: Bitcoin 0.3.19, when the system was live, the assumptions were explicit, and the future was still open.

Many modern changes improved security and robustness. Some also turned temporary decisions into permanent doctrine.

BITOK keeps the original engineering attitude:

- limits are tools
- safety matters
- participation matters
- usefulness matters
- nothing is sacred except the rules that make the system work

Bitcoin was never meant to be finished in 2010.

BITOK exists to continue the work from there.

---

## Technical Specifications

```
Algorithm:          Yespower 1.0 (N=2048, r=32)
Block time:         10 minutes
Block reward:       50 BITOK (halves every 210,000 blocks)
Max supply:         21,000,000 BITOK
Difficulty adjust:  Every 2016 blocks
Coinbase maturity:  100 blocks
P2P port:           18333
RPC port:           8332
```

## Security Fixes Included

All security fixes from Bitcoin v0.3.19 are present:

- Value overflow protection (the 184 billion coin bug)
- Blockchain checkpoints
- DoS limits
- IsStandard() transaction filtering

Bitcoin forked once in August 2010 to fix the overflow bug. BITOK launches with all fixes in place. No forks needed.

---

## Closing Note

Software that never changes dies.
Software that cannot be changed thoughtfully becomes a monument.

BITOK is an experiment in continuing Bitcoin as a living system, using the same cautious, practical mindset that built it in the first place.

---

Run the code. That is the manifesto.
