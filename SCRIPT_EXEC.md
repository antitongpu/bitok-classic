# Script Exec

**Status:** Implemented
**Activation:** Block height 18,000 (`SCRIPT_EXEC_ACTIVATION_HEIGHT`)
**Activation model:** Block height gating via `SCRIPT_VERIFY_EXEC` flag

---

## Philosophy

Script Exec completes the missing safety layer from Satoshi's original script engine. The original Bitcoin 0.3.19 codebase shipped all opcodes enabled, zero execution limits, and a concatenated evaluation model that allows scriptSig to manipulate scriptPubKey execution. Between 2010 and 2015, Bitcoin Core addressed these issues by disabling opcodes entirely -- removing functionality to achieve safety.

Bitok takes a different path: **we bound the VM without neutering it**.

Every original opcode remains functional. OP_CAT, OP_MUL, OP_LSHIFT, OP_AND -- all still execute. But they now operate within strict, deterministic resource limits. The script engine becomes adversarial-safe not by restricting expressiveness, but by enforcing execution discipline.

This preserves Satoshi's vision of a programmable transaction system while making it safe to run under adversarial conditions on a public network.

---

## What Changed (File by File)

### script.h

**New constants added:**

| Constant | Value | Purpose |
|----------|-------|---------|
| `MAX_SCRIPT_SIZE` | 10,000 bytes | Maximum script length before execution is rejected |
| `MAX_STACK_SIZE` | 1,000 items | Maximum combined stack + altstack depth |
| `MAX_SCRIPT_ELEMENT_SIZE` | 520 bytes | Maximum size of any single stack element |
| `MAX_OPS_PER_SCRIPT` | 201 | Maximum non-push opcodes per script execution |
| `MAX_PUBKEYS_PER_MULTISIG` | 20 | Maximum public keys in a CHECKMULTISIG operation |
| `MAX_SIGOPS_PER_BLOCK` | 20,000 | Maximum signature operations per block |
| `MAX_BIGNUM_SIZE` | 4 bytes | Maximum size of numeric operands (little-endian signed magnitude) |

**New flag:**

- `SCRIPT_VERIFY_EXEC` -- When set, all Script Exec rules are enforced. When unset, legacy behavior is preserved for pre-activation blocks.

**New error codes:**

A comprehensive `ScriptError` enum was added to provide structured error reporting for every failure mode (disabled opcodes, size violations, encoding errors, etc.).

**New function declarations:**

- `VerifyScript()` -- The new separated evaluation entry point (scriptSig evaluated first, stack passed to scriptPubKey)
- `GetSigOpCount()` -- Counts signature operations in a script for block-level resource limiting
- `CheckSignatureEncoding()` -- Validates DER encoding, low-S, and sighash flags
- `CheckMinimalPush()` -- Validates that push operations use the shortest possible encoding

---

### script.cpp

This is the core of Script Exec. Every change is gated behind `(flags & SCRIPT_VERIFY_EXEC)` so pre-activation blocks validate identically to the original code.

#### 0.0 -- Activation Infrastructure

- `EvalScript()` signature changed: now takes an external `stack` reference and a `flags` parameter instead of managing its own stack and having no flag awareness
- All semantic checks are wrapped in `if (flags & SCRIPT_VERIFY_EXEC)` guards
- Legacy code paths remain functional when flags are zero

#### 0.1 -- Global Script Execution Limits

**0.1.1 Script Size:** Scripts exceeding 10,000 bytes are rejected before execution begins.

**0.1.2 Stack Size:** After every opcode, combined `stack.size() + altstack.size()` is checked against 1,000. Immediate failure on overflow.

**0.1.3 Script Element Size:** Every push operation and every opcode that creates stack elements (OP_CAT, string ops, arithmetic results) validates that the result does not exceed 520 bytes.

**0.1.4 Opcode Count:** A counter (`nOpCount`) increments for every non-push opcode (those with value > OP_16). Execution aborts immediately when 201 is exceeded. CHECKMULTISIG additionally adds its key count to this counter.

**0.1.5 Sigops Per Block:** `GetSigOpCount()` function added that parses a script and counts:
- OP_CHECKSIG / OP_CHECKSIGVERIFY = 1 sigop each
- OP_CHECKMULTISIG / OP_CHECKMULTISIGVERIFY = preceding push value + 1 (or 20 if unparseable)
- Called from `CheckBlock()` in main.cpp to enforce the 20,000 limit

**0.1.6 Multisig Key Limit:** CHECKMULTISIG now rejects key counts exceeding 20. Previously, only negative values were rejected -- an attacker could specify millions of keys.

#### 0.2 -- Stack Element Creation Guards

**OP_CAT:** Before concatenation executes, the combined size of both operands is checked against `MAX_SCRIPT_ELEMENT_SIZE`. Previously, repeated OP_CAT could create arbitrarily large stack elements, causing memory exhaustion. The opcode remains fully functional -- it just cannot produce results larger than 520 bytes.

**OP_SUBSTR, OP_LEFT, OP_RIGHT:** Numeric operands are now validated through `CastToBigNum()` which enforces 4-byte size limits and minimal encoding. Output sizes are checked post-operation. These remain fully functional splice operations.

**Hash operations:** Outputs are inherently fixed-size (20 or 32 bytes) and already within limits. They now count toward stack depth limits.

#### 0.3 -- Arithmetic / Bignum Constraints

This addresses the single largest DoS surface in the original code. The original engine used unbounded OpenSSL BIGNUMs -- an attacker could multiply two 100KB numbers (O(n^2) operation) inside a script, consuming minutes of CPU time.

**`CastToBigNum()` function added:** All numeric operands must:
- Be at most 4 bytes
- Use minimal encoding (no unnecessary leading zero bytes, except for sign bit)
- Non-minimal encodings are rejected

**Result validation:** After every arithmetic operation, the result is serialized and checked against the 4-byte limit.

**Division by zero:** OP_DIV and OP_MOD now explicitly check for zero divisor before calling OpenSSL, instead of relying on OpenSSL's inconsistent error handling.

**Shift limits:** OP_LSHIFT and OP_RSHIFT now reject shift counts greater than 31. Previously, arbitrary shift values were accepted.

**Exception safety:** The entire evaluation loop is wrapped in `try/catch(...)` to prevent `bignum_error` exceptions from crashing the node. Any exception during evaluation converts to script failure.

#### 0.4 -- Disabled Opcodes

**OP_VER:** Now fails immediately under Script Exec. Previously, it pushed the software VERSION constant to the stack, creating a consensus split between nodes running different versions.

**OP_VERIF / OP_VERNOTIF:** Now fail immediately, even inside non-executing conditional branches. This is critical -- these opcodes previously compared `VERSION` against a stack value for conditional execution, meaning nodes on different versions would take different branches. The check is placed before the `fExec` gate to ensure they are invalid regardless of conditional state.

These are the only opcodes that cause unconditional failure. Every other original opcode (including OP_CAT, OP_MUL, OP_LSHIFT, etc.) remains operational.

#### 0.5 -- OP_RETURN and OP_VERIFY

**OP_RETURN (CRITICAL FIX):** Changed from `pc = pend` (jump to end) to `return false` (immediate failure). The original behavior was a severe bug: OP_RETURN did not actually make scripts unspendable. It jumped to the end of execution, and the script result was determined by whatever was on the stack. A scriptSig pushing `OP_TRUE` before a scriptPubKey containing `OP_RETURN` would succeed. OP_RETURN now makes scripts provably unspendable as universally expected.

**OP_VERIFY:** Changed from `pc = pend` (soft exit) to `return false` (hard fail) on false. Functionally equivalent for the false case (the false value would remain on stack causing final failure), but now follows the deterministic failure model.

**All VERIFY variants fixed:**
- OP_EQUALVERIFY: `return false` instead of `pc = pend`
- OP_CHECKSIGVERIFY: `return false` instead of `pc = pend`
- OP_NUMEQUALVERIFY: `return false` instead of `pc = pend`
- OP_CHECKMULTISIGVERIFY: `return false` instead of `pc = pend`

#### 0.6 -- Separated Script Evaluation (ARCHITECTURAL)

**This is the most significant structural change in Script Exec.**

**Before:** `VerifySignature()` concatenated scripts:
```
EvalScript(scriptSig + OP_CODESEPARATOR + scriptPubKey, ...)
```
This allowed scriptSig to directly influence scriptPubKey execution -- push values could be consumed by scriptPubKey opcodes, control flow could be manipulated, and combined with the OP_RETURN bug, any scriptPubKey could be bypassed.

**After:** The new `VerifyScript()` function:
1. Evaluates scriptSig alone, producing a result stack
2. Copies that stack
3. Evaluates scriptPubKey using the copied stack
4. Checks that the final stack top is truthy

**scriptSig push-only enforcement:** After Script Exec activation, scriptSig must contain only push operations (data pushes and OP_1 through OP_16). No control flow, no arithmetic, no cryptographic operations. This is validated before execution via `IsPushOnly()`. This ensures scriptSig can only provide data to scriptPubKey, never manipulate its execution.

#### 0.7 -- Minimal Push Encoding

All push operations must use the shortest possible encoding:
- Empty data: must use `OP_0`
- Single byte 1-16: must use `OP_1` through `OP_16`
- Single byte 0x81: must use `OP_1NEGATE`
- 1-75 bytes: must use direct push (opcode = length)
- 76-255 bytes: must use `OP_PUSHDATA1`
- 256-65535 bytes: must use `OP_PUSHDATA2`

Redundant encodings (like `OP_PUSHDATA2` for a 5-byte push) are rejected. This eliminates transaction malleability via push encoding variation.

#### 0.8 -- Signature Encoding Rules

Three new validation functions enforce strict signature encoding:

**`IsValidSignatureEncoding()`** -- Strict DER validation:
- Validates the ASN.1 DER structure byte-by-byte
- Checks SEQUENCE tag (0x30), correct length, INTEGER tags (0x02)
- Rejects negative R/S values, non-minimal R/S encodings
- Rejects padding bytes
- Size bounds: minimum 9 bytes, maximum 73 bytes (including sighash)

**`IsLowDERSignature()`** -- Low-S enforcement:
- Compares the S value against half the secp256k1 group order
- `halfOrder = 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF5D576E7357A4501DDFE92F46681B20A0`
- S must be <= halfOrder
- Eliminates signature malleability (given valid (r,s), an attacker could create (r, n-s) which is equally valid)

**`IsDefinedHashtypeSignature()`** -- Sighash flag validation:
- Last byte (after stripping ANYONECANPAY) must be SIGHASH_ALL (1), SIGHASH_NONE (2), or SIGHASH_SINGLE (3)
- Rejects undefined hash types

These checks are applied to every signature in OP_CHECKSIG and every signature in OP_CHECKMULTISIG.

**SIGHASH_SINGLE out-of-range fix:** In the original code, when `SIGHASH_SINGLE` is used and the input index exceeds the number of outputs, `SignatureHash()` returns a hash of `1` instead of a proper transaction hash. Any signature valid against hash=1 would be accepted, allowing theft of coins from outputs using SIGHASH_SINGLE in this edge case. Script Exec fixes this in `CheckSig()`: when `(nHashType & 0x1f) == SIGHASH_SINGLE && nIn >= txTo.vout.size()`, the signature check returns `false` immediately, before `SignatureHash()` is ever called. The underlying `SignatureHash()` function still returns 1 for backward compatibility with pre-activation blocks, but the check in `CheckSig()` ensures it is never reached post-activation.

**CHECKMULTISIG NULLDUMMY enforcement (BIP147):** CHECKMULTISIG consumes one extra stack element beyond the keys and signatures due to an off-by-one bug in the original Bitcoin implementation. This dummy element is not checked in Bitcoin prior to BIP147 -- any value is accepted. This allows a third party to mutate the dummy byte of an unconfirmed multisig transaction, changing the txid without invalidating the signatures (third-party malleability). Script Exec enforces that the dummy element must be exactly zero-length (empty byte array, i.e. OP_0). If `stacktop(-i).size() != 0` at the point where the dummy is consumed, the script fails immediately. This is safe to enforce from block 0 because Bitok's strict `isStandard()` rules mean no multisig transactions exist on-chain prior to Script Exec activation -- there are no legacy transactions to break.

#### 0.9 -- Deterministic Failure Model

All failure paths now use `return false` (immediate, atomic failure). The previous pattern of `pc = pend` (jump to script end, let stack determine outcome) has been eliminated throughout. No PC rewinds, no silent successes, no partial completions.

Additionally, the `vfExec` stack (conditional execution tracking) is checked after the main loop -- unbalanced IF/ENDIF causes failure.

#### 0.10 -- Resource Accounting

During execution, the following are tracked and enforced:
- `nOpCount`: incremented for every non-push opcode, checked against 201
- Stack depth: `stack.size() + altstack.size()` checked after every opcode against 1,000
- Element sizes: checked at creation points (push, concatenation, arithmetic result)
- Script size: checked once before execution against 10,000

#### CastToBool Fix

The `CastToBool` function was rewritten to avoid constructing a `CBigNum` just to check if a byte vector is zero. The new implementation iterates bytes directly, handling the negative-zero case (0x80 in the last byte) correctly. This eliminates an unnecessary OpenSSL allocation for every boolean check.

---

### main.h

- Added `SCRIPT_EXEC_ACTIVATION_HEIGHT = 18000` -- The block height at which Script Exec rules activate

---

### main.cpp

**`ConnectInputs()`:** Now computes `nScriptFlags` based on block height and passes to `VerifySignature()`. Before `SCRIPT_EXEC_ACTIVATION_HEIGHT`, flags are `SCRIPT_VERIFY_NONE` (legacy behavior). After, `SCRIPT_VERIFY_EXEC` is set.

**`ClientConnectInputs()`:** Same flag computation for the SPV/light client validation path.

**`CheckBlock()`:** Now counts signature operations across all transactions in the block:
- Iterates all inputs' scriptSig and all outputs' scriptPubKey
- Sums sigops via `GetSigOpCount()`
- Rejects blocks exceeding `MAX_SIGOPS_PER_BLOCK` (20,000)
- This prevents DoS via blocks packed with signature verification operations

**`IsStandard()` -- Standardness Expansion (Relay Policy):**

Before Script Exec, only two script templates were relayable:
- P2PK: `<pubkey> OP_CHECKSIG`
- P2PKH: `OP_DUP OP_HASH160 <hash> OP_EQUALVERIFY OP_CHECKSIG`

This meant every opcode Satoshi implemented (OP_CAT, OP_MUL, OP_CHECKMULTISIG as bare output, OP_AND, etc.) was consensus-valid but unreachable through normal transaction relay. Bitcoin Core's approach: disable the opcodes at consensus. Bitok's approach: make the engine safe, then open the relay policy.

After `SCRIPT_EXEC_ACTIVATION_HEIGHT`, `IsStandard()` accepts any scriptPubKey that:
- Is not empty
- Does not exceed `MAX_SCRIPT_SIZE` (10,000 bytes)
- Parses correctly (all opcodes are well-formed)

This means all live opcodes become practically usable on the network: bare multisig, OP_RETURN data, OP_CAT covenants, arithmetic scripts, bitwise logic, string manipulation -- everything the script engine supports is now standard.

The safety guarantee comes from the consensus layer (Script Exec execution limits), not from the relay policy whitelist. This is the fundamental difference from Bitcoin Core's design.

**`IsStandardTx()` -- Transaction Standardness:**

After Script Exec, scriptSig size limit is raised from the old 500-byte cap to the consensus `MAX_SCRIPT_SIZE` (10,000 bytes). This accommodates larger input scripts needed for complex spending conditions (multi-key multisig, covenant satisfactions, etc.). Push-only enforcement remains in both standardness and consensus.

---

### rpc.cpp

**`getblocktemplate`:** Now reports actual sigop counts per transaction instead of the previous hardcoded zero. Mining software receives accurate resource accounting information.

---

### key.h

**Bug fixes (not Script Exec gated -- immediate safety):**

- `SetPrivKey()`: Added empty vector check before accessing `[0]`. Previously, an empty private key vector caused undefined behavior (accessing element of empty vector).
- `SetPubKey()`: Same fix -- empty public key vector now returns false instead of crashing.
- `Verify()`: Added empty signature check before passing to `ECDSA_verify()`. Prevents undefined behavior from empty signature vectors.

---

## What Script Exec Explicitly Does NOT Do

- No new opcodes
- No new script templates
- No P2SH (pay-to-script-hash)
- No relay-policy shortcuts
- No dedicated RPC helpers for advanced scripts (escrow, marketplace, multisig creation are done via raw script construction)

---

## What This Opens for Bitok

Script Exec is the foundation layer. With a bounded, deterministic script VM and expanded standardness, these capabilities are live and relayable on the network immediately after activation:

### Immediate Capabilities (Live and Relayable Post-Script Exec)

1. **OP_CAT-based covenants** -- OP_CAT is live, bounded, and relayable. Combined with OP_CHECKSIG, this enables basic covenant patterns (restricting how coins can be spent) without any new opcodes. Bitok has OP_CAT where Bitcoin removed it. Transactions using OP_CAT will propagate across the network -- no miner coordination needed.

2. **Bare multisig** -- OP_CHECKMULTISIG outputs are now standard. m-of-n multisig scripts relay and confirm normally. Previously, these were consensus-valid but blocked by the P2PK/P2PKH-only whitelist. Up to 20 keys with bounded sigops counting.

3. **Bitwise computation** -- OP_AND, OP_OR, OP_XOR, OP_INVERT are all live and relayable. On-chain bitfield operations, compact state encoding, and flag-based authorization logic work end-to-end.

4. **Full integer arithmetic** -- OP_MUL, OP_DIV, OP_MOD, OP_LSHIFT, OP_RSHIFT are all live with bounded 4-byte operands. On-chain calculation of fees, percentages, time-based decay, and other numeric logic that Bitcoin cannot express. Transactions using these opcodes propagate normally.

5. **String manipulation** -- OP_SUBSTR, OP_LEFT, OP_RIGHT are live and relayable. Combined with OP_CAT, these enable structured data parsing and construction within scripts.

6. **OP_RETURN data embedding** -- OP_RETURN is now provably unspendable (hard fail) and outputs using it are standard. Safe for data anchoring, commitment schemes, timestamping, and metadata. Nodes will relay transactions with OP_RETURN outputs.

7. **Deterministic multisig** -- CHECKMULTISIG with bounded key counts, strict signature encoding, low-S enforcement, and NULLDUMMY enforcement. Both consensus-safe and relay-standard.

8. **Complex spending conditions** -- Any well-formed script up to 10,000 bytes is now standard. Time-locked contracts, hash-locked swaps, multi-condition authorization -- all relay and confirm without special miner arrangements.

9. **Escrow scripts** -- With expanded standardness, time-locked multisig and escrow scripts are live and relayable. The complete transaction lifecycle works: creation, relay, confirmation. No dedicated RPC helpers yet -- scripts are constructed manually.

10. **Marketplace contracts** -- With arithmetic, string ops, covenants, and open relay policy all available, the building blocks for on-chain marketplace logic are live and practically usable on the network today.

### Future Phase Readiness

11. **P2SH foundation** -- The separated evaluation model (scriptSig evaluated independently, stack passed to scriptPubKey) is the prerequisite for P2SH. Script Exec establishes the execution boundary that P2SH will extend.

12. **Script versioning** -- The `SCRIPT_VERIFY_EXEC` flag system is extensible. Future phases add new flags, enabling clean soft-fork activation of additional rules.

13. **RPC convenience layer** -- Dedicated RPC commands for escrow creation, multisig setup, marketplace templates, and other common patterns. The protocol supports all of these today; future work adds user-friendly interfaces.

---

## Standardness Model: Bitok vs Bitcoin Core

| Aspect | Bitcoin Core | Bitok (Post-Script Exec) |
|--------|-------------|---------------------|
| Safety approach | Disable opcodes at consensus | Bound execution, keep opcodes alive |
| Relay policy | Whitelist of exact templates | Any well-formed script within limits |
| OP_CAT | Disabled (consensus fail) | Live, bounded to 520 bytes, standard |
| OP_MUL | Disabled (consensus fail) | Live, 4-byte operands, standard |
| Bare multisig | Non-standard (relay blocked) | Standard |
| OP_RETURN | Standard but limited | Standard, provably unspendable |
| Custom scripts | Non-standard (relay blocked) | Standard if well-formed |
| Safety guarantee | Template whitelist | Consensus execution limits |

Bitok's philosophy: the script engine itself provides the safety guarantee through execution bounds. The relay policy's job is to reject malformed data, not to restrict expressiveness. If the consensus layer says a script is safe to execute, the relay layer should not second-guess it.

---

## Activation Behavior

| Block Height | Behavior |
|-------------|----------|
| < 18,000 | Legacy: Satoshi rules, no execution limits, concatenated evaluation, P2PK/P2PKH-only relay |
| >= 18,000 | Script Exec: bounded execution, separated evaluation, strict encoding, open standardness |

Existing scripts on the network that comply with standard P2PK and P2PKH templates will work identically before and after activation. The only scripts affected are those that:
- Exceed size or complexity limits (never seen in standard usage)
- Use non-minimal push encodings (transaction malleability vectors)
- Use non-DER or high-S signatures (malleability vectors)
- Rely on OP_VER/OP_VERIF/OP_VERNOTIF (consensus-splitting opcodes)
- Rely on OP_RETURN not failing (a bug, not a feature)

---

## One-Line Summary

Script Exec turns Satoshi's permissive script engine into a bounded, adversarial-safe VM -- without neutering it.
