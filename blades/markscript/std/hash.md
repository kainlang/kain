# Hash

Markscript hashing — message digests, checksums, and HMAC operations.
Dispatches through the IVT to OpenSSL or native crypto bridge for computation.

---

## md5

Compute an MD5 hex digest of a string.

> run "openssl md5 -quiet <<< \"hello\""

```markscript
# MD5 hash a string
let msg = "hello"
push("openssl md5 -quiet <<< \"" + msg + "\"")
call("run")
```

---

## sha1

Compute a SHA-1 hex digest of a string.

> run "openssl sha1 -quiet <<< \"hello\""

```markscript
# SHA-1 hash a string
let msg = "hello"
push("openssl sha1 -quiet <<< \"" + msg + "\"")
call("run")
```

---

## sha256

Compute a SHA-256 hex digest of a string.

> run "openssl sha256 -quiet <<< \"hello\""

```markscript
# SHA-256 hash a string
let msg = "hello"
push("openssl sha256 -quiet <<< \"" + msg + "\"")
call("run")
```

---

## sha512

Compute a SHA-512 hex digest of a string.

> run "openssl sha512 -quiet <<< \"hello\""

```markscript
# SHA-512 hash a string
let msg = "hello world"
push("openssl sha512 -quiet <<< \"" + msg + "\"")
call("run")
```

---

## blake2

Compute a BLAKE2b hex digest (via OpenSSL 3+ or native tool).

> run "openssl dgst -blake2b512 <<< \"hello\""

```markscript
# BLAKE2b hash a string
let msg = "hello"
push("openssl dgst -blake2b512 <<< \"" + msg + "\"")
call("run")
```

---

## file_hash

Compute a SHA-256 hash of a file on disk.

> run "certutil -hashfile path/to/file SHA256"

```markscript
# Hash a file's contents
let fpath = "path/to/file.bin"
push("certutil -hashfile \"" + fpath + "\" SHA256")
call("run")
```

---

## hmac

Compute an HMAC-SHA256 for a message and secret key.

> run "openssl dgst -sha256 -hmac \"secret\" <<< \"message\""

```markscript
# HMAC-SHA256 with a key
let key = "supersecret"
let msg = "authenticate me"
push("openssl dgst -sha256 -hmac \"" + key + "\" <<< \"" + msg + "\"")
call("run")
```

---

## compare

Compare two hex digests for equality (constant-time in real impl).

```markscript
let a = "abc123def456"
let b = "abc123def456"
let match = 0
if a == b:
    match = 1
# match = 1 if digests are equal
```

---

## sha256_multi

Hash multiple strings and verify against known digests.

> run "echo string1 string2 string3 | openssl sha256"

```markscript
let s1 = "data1"
let s2 = "data2"
let s3 = "data3"
let combined = s1 + s2 + s3
push("openssl sha256 -quiet <<< \"" + combined + "\"")
call("run")
```

---

## ripemd160

Compute a RIPEMD-160 hex digest.

> run "openssl dgst -ripemd160 <<< \"hello\""

```markscript
# RIPEMD-160 hash a string
let msg = "hello"
push("openssl dgst -ripemd160 <<< \"" + msg + "\"")
call("run")
```
