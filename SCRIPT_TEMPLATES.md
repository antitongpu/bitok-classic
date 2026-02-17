# Script Analysis & Testing RPCs

Tools for analyzing and testing Bitok scripts offline. These RPCs give developers visibility into script structure, opcode usage, execution limits, and the ability to test script logic in a sandbox before committing transactions on-chain.

## Commands

### analyzescript

Static analysis of a hex-encoded script. Reports opcodes, sigops, size, type, classification, and limit usage.

```
analyzescript <hex>
```

**Parameters:**

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | hex | string | Hex-encoded script |

**Returns:**

```json
{
  "asm": "OP_DUP OP_HASH160 89abcd... OP_EQUALVERIFY OP_CHECKSIG",
  "hex": "76a914...88ac",
  "size": 25,
  "type": "pubkeyhash",
  "reqSigs": 1,
  "addresses": ["1A1zP1..."],
  "opcodes": {
    "counted": 4,
    "pushes": 1,
    "distinct": ["OP_DUP", "OP_HASH160", "OP_EQUALVERIFY", "OP_CHECKSIG"],
    "categories": {
      "crypto": ["OP_CHECKSIG", "OP_HASH160"],
      "stack": ["OP_DUP"],
      "flow": [],
      "arithmetic": [],
      "splice": [],
      "bitwise": ["OP_EQUALVERIFY"]
    }
  },
  "sigops": 1,
  "maxPushSize": 20,
  "limits": {
    "size": "25/10000",
    "opcodes": "4/201",
    "sigops": "1/20000",
    "maxPushSize": "20/520",
    "withinLimits": true
  }
}
```

**Output fields:**

| Field | Description |
|-------|-------------|
| `asm` | Human-readable script disassembly |
| `hex` | Hex-encoded script |
| `size` | Script size in bytes |
| `type` | Script classification (pubkeyhash, pubkey, multisig, nulldata, nonstandard) |
| `reqSigs` | Number of signatures required (if applicable) |
| `addresses` | Associated addresses (if applicable) |
| `opcodes.counted` | Number of opcodes counted toward the 201 limit (excludes pushes and OP_1-OP_16) |
| `opcodes.pushes` | Number of push operations (data pushes + number pushes) |
| `opcodes.distinct` | Unique opcode names used |
| `opcodes.categories` | Distinct opcodes grouped by functional category |
| `sigops` | Signature operations count (CHECKSIG=1, CHECKMULTISIG=n or 20) |
| `maxPushSize` | Largest data push in bytes |
| `limits` | Usage vs. consensus limits |
| `limits.withinLimits` | Whether all limits are satisfied |

**Opcode categories:**

| Category | Opcodes |
|----------|---------|
| crypto | RIPEMD160, SHA1, SHA256, HASH160, HASH256, CHECKSIG, CHECKSIGVERIFY, CHECKMULTISIG, CHECKMULTISIGVERIFY, CODESEPARATOR |
| stack | TOALTSTACK, FROMALTSTACK, 2DROP, 2DUP, 3DUP, 2OVER, 2ROT, 2SWAP, IFDUP, DEPTH, DROP, DUP, NIP, OVER, PICK, ROLL, ROT, SWAP, TUCK |
| flow | NOP, IF, NOTIF, ELSE, ENDIF, VERIFY, RETURN |
| arithmetic | 1ADD, 1SUB, 2MUL, 2DIV, NEGATE, ABS, NOT, 0NOTEQUAL, ADD, SUB, MUL, DIV, MOD, LSHIFT, RSHIFT, BOOLAND, BOOLOR, NUMEQUAL, NUMEQUALVERIFY, NUMNOTEQUAL, LESSTHAN, GREATERTHAN, LESSTHANOREQUAL, GREATERTHANOREQUAL, MIN, MAX, WITHIN |
| splice | CAT, SUBSTR, LEFT, RIGHT, SIZE |
| bitwise | INVERT, AND, OR, XOR, EQUAL, EQUALVERIFY |

**Examples:**

Analyze a P2PKH script:
```bash
bitokd analyzescript "76a91489abcdefabbaabbaabbaabbaabbaabbaabbaabba88ac"
```

Analyze a complex script using arithmetic and splice opcodes:
```bash
bitokd analyzescript "935587"
# Reports: OP_ADD OP_5 OP_EQUAL, type=nonstandard, arithmetic=[OP_ADD], bitwise=[OP_EQUAL]
```

Analyze a 2-of-3 multisig:
```bash
bitokd analyzescript "5221<key1>21<key2>21<key3>53ae"
# Reports: type=multisig, reqSigs=2, sigops=3
```

Check if a script exceeds limits:
```bash
bitokd analyzescript "<very large script hex>"
# limits.withinLimits will be false if any limit exceeded
```

---

### validatescript

Execute a script in sandbox mode and report the result. Runs the script with pre-loaded stack values to test logic without a real transaction.

```
validatescript <script hex> [stack] [flags]
```

**Parameters:**

| # | Name | Type | Description |
|---|------|------|-------------|
| 1 | script | string | Hex-encoded script to execute |
| 2 | stack | array | Optional. Hex-encoded values to pre-load on the stack (simulates scriptSig output) |
| 3 | flags | string | Optional. `"exec"` (default, post-activation rules) or `"legacy"` (pre-activation rules) |

**Returns:**

```json
{
  "valid": true,
  "success": true,
  "finalStack": ["01"],
  "stackSize": 1
}
```

**Output fields:**

| Field | Description |
|-------|-------------|
| `valid` | `true` if script executed successfully AND top of stack is true (same check as VerifyScript) |
| `success` | `true` if EvalScript completed without error (script logic ran, but result might be false) |
| `finalStack` | Hex-encoded stack values after execution (bottom to top) |
| `stackSize` | Number of items on the stack after execution |
| `warnings` | Present on failure: hints about why the script failed |

**Result interpretation:**

| success | valid | Meaning |
|---------|-------|---------|
| true | true | Script passes verification |
| true | false | Script ran without error but left false/empty on top of stack |
| false | false | Script hit an execution error (stack underflow, opcode limit, etc.) |

**Limitations:**
- `OP_CHECKSIG` and `OP_CHECKMULTISIG` will always fail because there is no real transaction to sign against. Use this RPC for testing script logic (arithmetic, flow control, string operations, hash checks), not signature verification.
- If a script fails due to signature checking, the `warnings` field will note this.

**Examples:**

Test OP_EQUAL (1 == 1):
```bash
bitokd validatescript "87" '["01","01"]'
# valid: true -- stack items are equal
```

Test OP_EQUAL (1 != 2):
```bash
bitokd validatescript "87" '["01","02"]'
# valid: false, success: true -- script ran but items differ
```

Test arithmetic (3 + 2 == 5):
```bash
bitokd validatescript "935587" '["03","02"]'
# valid: true -- OP_ADD(3,2)=5, OP_5=5, OP_EQUAL(5,5)=true
```

Test hash lock (SHA256 preimage verification):
```bash
# Script: OP_SHA256 <expected_hash> OP_EQUAL
# Stack: [preimage]
bitokd validatescript "a820<32-byte-hash>87" '["<preimage-hex>"]'
# valid: true if SHA256(preimage) matches expected hash
```

Test OP_CAT (concatenation):
```bash
# Script: OP_CAT OP_EQUAL
# Stack: ["hello", "world", "helloworld"]
bitokd validatescript "7e87" '["68656c6c6f","776f726c64","68656c6c6f776f726c64"]'
# valid: true -- CAT("hello","world") = "helloworld"
```

Test with legacy (pre-activation) rules:
```bash
bitokd validatescript "935587" '["03","02"]' "legacy"
```

Test OP_IF branching:
```bash
# Script: OP_IF OP_1 OP_ELSE OP_0 OP_ENDIF
# Stack: [true_value] -> takes IF branch, pushes 1
bitokd validatescript "635167006868" '["01"]'
# valid: true, finalStack: ["01"]

# Stack: [false_value] -> takes ELSE branch, pushes 0
bitokd validatescript "635167006868" '[""]'
# valid: false, finalStack: [""]
```

Test multiplication:
```bash
# Script: OP_MUL OP_8 OP_EQUAL
# Stack: [4, 2] -> 4*2=8, 8==8 -> true
bitokd validatescript "955887" '["04","02"]'
# valid: true
```

---

## Typical Workflows

### 1. Verify a script fits within consensus limits

Before sending coins to a custom scriptPubKey, check that it will be accepted by the network:

```bash
# Build your script, then analyze it
bitokd analyzescript "<your script hex>"

# Check: limits.withinLimits should be true
# Check: size, opcode count, and push sizes are under limits
```

### 2. Test hash lock logic offline

```bash
# Create a preimage
PREIMAGE="secret_data_hex"

# Compute the expected hash (use any SHA256 tool)
HASH=$(echo -n "$PREIMAGE" | xxd -r -p | sha256sum | cut -d' ' -f1)

# Build the script: OP_SHA256 <32-byte hash> OP_EQUAL
SCRIPT="a820${HASH}87"

# Test with correct preimage
bitokd validatescript "$SCRIPT" "[\"$PREIMAGE\"]"
# valid: true

# Test with wrong preimage
bitokd validatescript "$SCRIPT" '["deadbeef"]'
# valid: false (hash mismatch)
```

### 3. Test arithmetic conditions

```bash
# Script that checks: input * 7 > 100
# OP_7 OP_MUL OP_PUSHDATA(100) OP_GREATERTHAN
SCRIPT="5795016460"

# Test with 15 (15*7=105 > 100 -> true)
bitokd validatescript "$SCRIPT" '["0f"]'
# valid: true

# Test with 14 (14*7=98 < 100 -> false)
bitokd validatescript "$SCRIPT" '["0e"]'
# valid: false
```

### 4. Inspect unknown scripts

When you encounter an unfamiliar scriptPubKey on-chain:

```bash
# Decode the transaction first
bitokd decoderawtransaction "<tx hex>"
# Note the scriptPubKey hex from the output

# Analyze it
bitokd analyzescript "<scriptPubKey hex>"
# See: type, opcodes used, categories, limit usage
```

### 5. Test OP_CAT covenant patterns

```bash
# Script: DUP CAT <expected_doubled> EQUAL
# Tests that the input, when concatenated with itself, matches the expected value
SCRIPT="767e20<64-byte-expected>87"

bitokd validatescript "$SCRIPT" '["<32-byte-input>"]'
```

---

## Files Modified

- **`rpc.cpp`** -- Added `analyzescript` and `validatescript` RPC commands. Registered in call table. Added CLI parameter type conversions for validatescript JSON array argument.
