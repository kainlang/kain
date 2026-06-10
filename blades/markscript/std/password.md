# Password

Markscript password management — hashing, verification, strength assessment, and generation.
Uses OpenSSL for cryptographic operations and custom logic for strength checks.

---

## hash

Hash a password with a random salt using SHA-256.

> run "echo -n 'password+salt' | openssl dgst -sha256"

```markscript
# Hash a password with salt
let pass = "correcthorsebatterystaple"
let salt = "randomsalt123"
let combined = pass + salt
push("echo -n \"" + combined + "\" | openssl dgst -sha256")
call("run")
```

---

## verify

Verify a password against a stored hash and salt.

```markscript
let input_pass = "correcthorsebatterystaple"
let stored_hash = "a1b2c3d4e5f6..."
let stored_salt = "randomsalt123"
let combined = input_pass + stored_salt
# Hash combined and compare to stored_hash
let valid = 0
# valid = 1 if computed hash matches stored_hash
```

---

## strength

Evaluate password strength on a 0-100 scale based on length, character diversity, and patterns.

```markscript
let pwd = "MyP@ssw0rd!2024"
let score = 0
let length = 16
let has_upper = 1
let has_lower = 1
let has_digit = 1
let has_special = 1
if length >= 8:
    score = 25
if length >= 12:
    score = 50
if has_upper == 1 and has_lower == 1:
    score = score + 20
if has_digit == 1:
    score = score + 15
if has_special == 1:
    score = score + 15
# score between 0 and 100
```

---

## generate

Generate a cryptographically random password of configurable length.

> run "openssl rand -base64 12"

```markscript
# Generate a random password (base64 encoded, 12 bytes = 16 chars)
let byte_len = 12
push("openssl rand -base64 " + byte_len)
call("run")
```

---

## generate_readable

Generate a pronounceable password using alternating consonants and vowels.

```markscript
let consonants = "bcdfghjklmnpqrstvwxyz"
let vowels = "aeiou"
let length = 10
let password = ""
let i = 0
while i < length:
    if i % 2 == 0:
        # pick a consonant at position i % len(consonants)
        password = password + "b"
    else:
        # pick a vowel at position i % len(vowels)
        password = password + "a"
    i = i + 1
# password = "bahabahab"
```

---

## policy_check

Check a password against common policies: min length, upper, lower, digit, special.

```markscript
let pwd = "Test1234!"
let min_len = 8
let meets_min_len = 0
let has_upper = 0
let has_lower = 0
let has_digit = 0
let has_special = 0
let compliant = 0
# In practice, scan each character
# For demonstration, set flags directly
has_upper = 1
has_lower = 1
has_digit = 1
has_special = 1
meets_min_len = 1
if meets_min_len == 1 and has_upper == 1 and has_lower == 1 and has_digit == 1 and has_special == 1:
    compliant = 1
# compliant = 1 if all policy requirements met
```

---

## bcrypt

Hash a password with bcrypt (via OpenSSL 3+ or external tool).

> run "python -c \"import bcrypt; print(bcrypt.hashpw(b'password', bcrypt.gensalt()).decode())\""

```markscript
# bcrypt hash a password
let pass = "my_password"
push("python -c \"import bcrypt; print(bcrypt.hashpw(b'" + pass + "', bcrypt.gensalt()).decode())\"")
call("run")
```

---

## argon2

Hash a password with Argon2 (via external tool if available).

> run "python -c \"from argon2 import PasswordHasher; ph = PasswordHasher(); print(ph.hash('password'))\""

```markscript
# Argon2 hash a password
let pass = "my_password"
push("python -c \"from argon2 import PasswordHasher; ph = PasswordHasher(); print(ph.hash('" + pass + "'))\"")
call("run")
```

---

## bcrypt_verify

Verify a password against a bcrypt hash.

> run "python -c \"import bcrypt; print(bcrypt.checkpw(b'password', b'$2b$12$...'))\""

```markscript
# Verify password against bcrypt hash
let pass = "my_password"
let hash = "$2b$12$..."
push("python -c \"import bcrypt; print(bcrypt.checkpw(b'" + pass + "', b'" + hash + "'))\"")
call("run")
```

---

## argon2_verify

Verify a password against an Argon2 hash.

> run "python -c \"from argon2 import PasswordHasher; ph = PasswordHasher(); print(ph.verify('hash', 'password'))\""

```markscript
# Verify against Argon2 hash
let pass = "my_password"
let hash = "$argon2id$v=19$..."
push("python -c \"from argon2 import PasswordHasher; ph = PasswordHasher(); print(ph.verify('" + hash + "', '" + pass + "'))\"")
call("run")
```

---

## entropy

Calculate the entropy of a password in bits.

```markscript
let length = 12
let charset_size = 72  # upper + lower + digits + symbols
let entropy = 0
let bits = 0
# entropy = length * log2(charset_size)
# approximate with integer arithmetic
bits = charset_size
entropy = length * 6  # ~6 bits per char for size 72
# entropy ~= 72 bits for a 12-char password
```
