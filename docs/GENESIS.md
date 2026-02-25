# The Genesis Block

## Overview

The Bitok genesis block (block 0) was mined on January 3, 2009 and serves as the immutable foundation of the entire chain. Like all genesis blocks, it is hardcoded and not subject to normal validation rules.

## Parameters

| Field | Value |
|-------|-------|
| nVersion | 1 |
| nTime | 1231006505 |
| nBits | `0x1effffff` |
| nNonce | 37137 |
| Hash | `0x0290400ea28d3fe79d102ca6b7cd11cee5eba9f17f2046c303d92f65d6ed2617` |

## A Historical Footnote

Attentive readers may notice that the genesis block's `nBits` value (`0x1effffff`) does not exactly match the canonical compact encoding of `bnProofOfWorkLimit`.

The proof-of-work limit is defined as `~uint256(0) >> 17`, which expands to:

```
0x00007FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
```

The canonical compact encoding of this value is `0x1e7fffff`. The genesis block instead carries `0x1effffff`. The difference is in the high bit of the mantissa: in OpenSSL's MPI format (used by `SetCompact`/`GetCompact` in `bignum.h`), a set high bit on the leading byte is interpreted as a sign flag. This means `0x1effffff` decodes to a slightly different target than intended.

In practice, the genesis block was mined at a marginally easier difficulty than the nominal limit -- a minor artifact of the very first block being assembled before the difficulty engine took over.

## Why It Doesn't Matter

- The genesis block is hardcoded. It is never re-validated against difficulty rules.
- `GetNextWorkRequired()` derives all subsequent difficulty from `bnProofOfWorkLimit`, not from the genesis `nBits`.
- Every block from height 1 onward uses the correct canonical compact value `0x1e7fffff`.
- No consensus rules are affected. No chain integrity is compromised.

The genesis nBits encoding is non-canonical. Subsequent blocks derive difficulty correctly. This has no effect on consensus.

## Historical Authenticity

Every blockchain carries the fingerprints of its origin. Bitcoin's genesis block embeds a Times headline. Bitok's genesis block carries a slightly non-canonical difficulty encoding -- a quiet reminder that block 0 was mined before the engine was fully in the loop.

This is not a flaw. It is not a consensus issue. It is simply how the chain was born, and it remains exactly as it was.
