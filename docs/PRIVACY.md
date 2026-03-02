# **Native Blind**
# Bitok Privacy Protocol: Finishing What Satoshi Started
---

## Why I Built This

On August 13, 2010, Satoshi Nakamoto posted what I consider the most important unfinished blueprint in Bitcoin's history. In the "Not a suggestion" thread on BitcoinTalk, he wrote:

> **"What we need is a way to generate additional blinded variations of a public key. The blinded variations would have the same properties as the root public key, such that the private key could generate a signature for any one of them. Others could not tell if a blinded key is related to the root key, or other blinded keys from the same root key. These are the properties of blinding. Blinding, in a nutshell, is x = (x * large_random_int) mod m."**
>
> **"When paying to a bitcoin address, you would generate a new blinded key for each use."**
>
> -- Satoshi Nakamoto, August 13, 2010
> [BitcoinTalk Post #356](https://bitcointalk.org/index.php?topic=770.msg9074#msg9074)
> [Nakamoto Institute Archive](https://satoshi.nakamotoinstitute.org/posts/bitcointalk/356/)

He described it. He researched it. He pointed to the cryptographic primitives that would make it work. Then he disappeared, and nobody built it into Bitcoin.

Two days earlier, in the same thread, he had described something even more ambitious -- a scheme for network-level unlinkability:

> **"The network keeps the outpoint and the first valid inpoint that proves it spent. The inpoint signs a hash of its associated next outpoint and a salt, so it can privately be shown that the signature signs a particular next outpoint if you know the salt, but publicly the network doesn't know what the next outpoint is."**
>
> -- Satoshi Nakamoto, August 11, 2010
> [BitcoinTalk Post #342](https://bitcointalk.org/index.php?topic=770.msg8798#msg8798)
> [Nakamoto Institute Archive](https://satoshi.nakamotoinstitute.org/posts/bitcointalk/342/)

The Bitcoin whitepaper itself, in Section 10, laid out the privacy model explicitly:

> "As an additional firewall, a new key pair should be used for each transaction to keep them from being linked to a common owner."
>
> -- Bitcoin: A Peer-to-Peer Electronic Cash System, October 31, 2008

And earlier on the forum:

> **"For greater privacy, it's best to use bitcoin addresses only once."**
>
> -- Satoshi Nakamoto, November 25, 2009
> [BitcoinTalk Post #11](https://bitcointalk.org/index.php?topic=8)

The problem is clear: Satoshi designed Bitcoin with the expectation that each address would be used once, that users would generate fresh keys for every payment. But the ecosystem went in the opposite direction. People reuse addresses. Exchanges publish deposit addresses. Block explorers link everything together. The privacy model Satoshi described in the whitepaper has been completely undermined in practice.

I am building Bitok to fix this. Not by changing the consensus rules that Satoshi designed, but by implementing the privacy features he described but never had time to code.

---

## What I Have Built: Stealth ok-Addresses (Key Blinding)

This is the first feature, and it is the one Satoshi described most concretely. I have implemented it exactly as he envisioned: blinded variations of a public key, where each payment goes to a unique, unlinkable address, and only the intended recipient can detect and spend the funds.

### How It Works

The protocol uses Elliptic Curve Diffie-Hellman (ECDH) -- the same cryptographic primitive Satoshi pointed to when he mentioned "key blinding" and linked to research on blinding schemes.

**Setup (Receiver):**

1. I generate two keypairs: a **scan keypair** and a **spend keypair**.
2. Both public keys are stored in **compressed format** (33 bytes, `0x02`/`0x03` prefix). This is the only place Bitok uses compressed keys -- all other wallet operations use standard uncompressed keys.
3. I publish a **stealth address**, which encodes both compressed public keys with the `ok` prefix. This is the only address I ever need to share publicly.

**Sending (Sender):**

1. The sender generates a random **ephemeral keypair** (used once, then discarded).
2. Using ECDH, the sender computes a **shared secret** from their ephemeral private key and the receiver's scan public key.
3. The sender derives a **one-time destination public key** by tweaking the receiver's spend public key with the shared secret: `P_dest = P_spend + Hash(shared_secret) * G`
4. The sender creates a normal transaction paying to the address derived from `P_dest`.
5. The sender embeds the **compressed ephemeral public key** (33 bytes) in an OP_RETURN output so the receiver can find it.

**Receiving (Receiver):**

1. My wallet automatically scans every transaction for OP_RETURN outputs containing our stealth prefix (`0x06`) followed by exactly 33 bytes of compressed public key.
2. For each one found, I compute the same shared secret using my scan private key and the embedded ephemeral public key.
3. I derive the expected one-time destination public key and check if any output in that transaction pays to it.
4. If it matches, I compute the corresponding private key (`p_dest = p_spend + Hash(shared_secret)`) and add it to my wallet. The funds are now spendable.

### What This Achieves

- **Unlinkable payments**: Every payment to the same stealth address produces a different on-chain destination. No outside observer can tell that two payments went to the same person.
- **No address reuse**: Satoshi's original requirement -- "a new key pair should be used for each transaction" -- is enforced automatically by the protocol. The sender cannot reuse a destination even if they try.
- **No interaction required**: The receiver does not need to be online. The sender derives everything from the published stealth address alone.
- **Compact on-chain footprint**: Stealth addresses use compressed public keys (33 bytes instead of 65), reducing the OP_RETURN payload to 34 bytes (1-byte prefix + 33-byte ephemeral key). This keeps stealth transactions small and efficient.
- **No consensus changes**: This operates entirely within the existing transaction format. The OP_RETURN output is standard. The pay-to-pubkey-hash output is standard. No fork required.

### RPC Commands

RPC commands provide full stealth address lifecycle management:

**Creating & Listing:**

- `getnewstealthaddress [label]` -- Generate a new stealth address for receiving private payments.
- `liststealthaddresses` -- List all stealth addresses in the wallet with their public keys.
- `decodestealthaddress <stealthaddress>` -- Inspect the scan and spend public keys encoded in a stealth address (works for any stealth address, not just your own).

**Sending:**

- `sendtoaddress <address> <amount> [comment] [comment-to]` -- Automatically detects whether the address is a regular address or an ok-address (stealth). For ok-addresses, the transaction will appear on-chain as a normal payment to a fresh one-time address that cannot be linked to the stealth address.

**Backup & Migration:**

- `dumpprivkey <address>` -- Works for both regular and stealth addresses. For ok-addresses, returns the `SK...` combined stealth secret.
- `importprivkey <privkey> [label] [rescan=true]` -- Works for both WIF keys and `SK...` stealth secrets. For stealth secrets, reconstructs the stealth address and imports it into the wallet.

### Address Format

Stealth addresses use a distinct encoding to avoid confusion with regular Bitok addresses:

- **Prefix**: `ok` (human-readable prefix identifying a stealth address)
- **Version byte**: `0x01`
- **Payload**: two 33-byte compressed public keys (scan + spend)
- **Checksum**: 4-byte Base58Check checksum

Example: `ok1A2b3C4d...` (the full address is ~80 characters)

### Deterministic Change Keys

When spending from a stealth-derived address, the wallet needs a change address. Using a random new key would be dangerous -- if the wallet were restored from the SK secret alone, that random change key would be lost, and the change funds with it.

Bitok solves this with **deterministic change key derivation**. When a stealth transaction needs change:

1. The wallet identifies which stealth address owns the input being spent.
2. It derives a change key from the spend secret using a deterministic scheme: `change_priv = SHA256d(spend_secret || "stealth_change" || index)`, where `index` is an incrementing counter per stealth address.
3. The change output pays to this derived key. Since the key can be re-derived from the spend secret alone, it survives backup/restore.
4. The counter (`index`) is persisted in the wallet database and advanced after each use.

During an SK import with rescan, the wallet recovers these change keys by:

1. Scanning the entire blockchain to collect all on-chain address hashes.
2. Deriving change keys sequentially (index 0, 1, 2, ...) and checking which ones appear on-chain.
3. Importing each found change key into the wallet and advancing the counter.
4. Stopping at the first index that has no on-chain match.

This ensures that all change from previous stealth spends is recovered automatically.

### Wallet Integration

Stealth scanning is integrated directly into the wallet's transaction processing pipeline. When new blocks arrive or during a wallet rescan, every transaction is checked for stealth payments. Detected payments are automatically added to the wallet with the derived private key. There is no manual step required.

Stealth address keys (scan key, spend key) are persisted in the wallet database in compressed form alongside regular keys. They survive wallet restarts and are included in the standard wallet rescan process.

### Backup & Portability

Stealth addresses use the same `dumpprivkey`/`importprivkey` commands as regular addresses. The commands auto-detect the address/key type and route accordingly. Although a stealth address internally uses two keypairs (scan and spend), the export produces a single combined key (`SK...` format) for simplicity.

**Backup workflow:**

1. `liststealthaddresses` -- see all your stealth addresses.
2. `dumpprivkey <ok-address>` -- for each one, export the combined `SK...` key.
3. Store the key securely (same precautions as regular private key backups).

**Restore workflow:**

1. `importprivkey "SK..." "label"` -- import on the new wallet.
2. The rescan runs automatically in two passes:
   - **Pass 1**: Scans the blockchain for stealth payments (ECDH detection) while simultaneously collecting all on-chain address hashes.
   - **Between passes**: Recovers deterministic change keys by checking derived addresses against the collected on-chain set.
   - **Pass 2**: Re-scans to pick up transactions belonging to the recovered change keys.
3. `liststealthaddresses` -- verify the address was imported correctly.

Note: Importing an SK key takes roughly twice as long as importing a regular private key, because it requires two full blockchain scans instead of one.

---

## What Comes Next: Dandelion++ (Network-Level Unlinkability)

Satoshi identified the second layer of the problem on August 11, 2010:

> "The challenge is, how do you prove that no other spends exist? It seems a node must know about all transactions to be able to verify that."
>
> -- Satoshi Nakamoto, August 11, 2010
> [BitcoinTalk Post #338](https://bitcointalk.org/index.php?topic=770.msg8637#msg8637)

And earlier:

> **"You could use TOR if you don't want anyone to know you're even using Bitcoin."**
>
> -- Satoshi Nakamoto, February 9, 2010
> [BitcoinTalk Post #45](https://satoshi.nakamotoinstitute.org/posts/bitcointalk/45/)

Stealth addresses solve the on-chain linkability problem. But there is a second problem: when you broadcast a transaction, the network nodes that first see it can correlate your IP address with your transaction. Satoshi's salt-based outpoint scheme was one approach. Dandelion++ is a simpler, proven approach that achieves a similar goal.

**Dandelion++** works by changing how transactions propagate through the network:

1. **Stem phase**: Instead of immediately broadcasting a transaction to all peers, the originating node sends it to a single randomly selected peer. That peer forwards it to another single peer, and so on, along a random path (the "stem").
2. **Fluff phase**: After a random number of hops, the transaction enters the normal broadcast ("fluff") phase and propagates to all nodes as usual.

The result: by the time the network sees the transaction, it appears to originate from a node that is several hops away from the true sender. An adversary monitoring the network cannot reliably determine which node created the transaction.

This is the next feature I will implement in Bitok.

---

## The Philosophy

I am not inventing new cryptography. I am not changing Bitcoin's consensus rules. I am building what Satoshi described but did not have time to finish.

The privacy model in the Bitcoin whitepaper was always meant to be stronger than what the network delivered in practice. Satoshi knew this. He researched key blinding. He sketched out network-level unlinkability. He recommended Tor. He said addresses should be used only once.

Bitok takes these ideas and makes them real, in working code, in a way that is compatible with the original protocol design. No forks. No new trust assumptions. Just the privacy that was always supposed to be there.

---

## References

1. Satoshi Nakamoto, "Bitcoin: A Peer-to-Peer Electronic Cash System," Section 10: Privacy, October 31, 2008.
   https://bitcoin.org/bitcoin.pdf

2. Satoshi Nakamoto, BitcoinTalk Post #11, "How anonymous are bitcoins?", November 25, 2009.
   https://bitcointalk.org/index.php?topic=8
   https://satoshi.nakamotoinstitute.org/posts/bitcointalk/11/

3. Satoshi Nakamoto, BitcoinTalk Post #45, "Make this anonymous?", February 9, 2010.
   https://satoshi.nakamotoinstitute.org/posts/bitcointalk/45/

4. Satoshi Nakamoto, BitcoinTalk Post #338, "Not a suggestion" thread, August 11, 2010.
   https://bitcointalk.org/index.php?topic=770.msg8637#msg8637
   https://satoshi.nakamotoinstitute.org/posts/bitcointalk/338/

5. Satoshi Nakamoto, BitcoinTalk Post #342, "Not a suggestion" thread (salt-based outpoints), August 11, 2010.
   https://bitcointalk.org/index.php?topic=770.msg8798#msg8798
   https://satoshi.nakamotoinstitute.org/posts/bitcointalk/342/

6. Satoshi Nakamoto, BitcoinTalk Post #356, "Not a suggestion" thread (key blinding), August 13, 2010.
   https://bitcointalk.org/index.php?topic=770.msg9074#msg9074
   https://satoshi.nakamotoinstitute.org/posts/bitcointalk/356/
