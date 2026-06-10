# JWT

Markscript JSON Web Tokens — encode, decode, verify, and inspect JWT tokens.
Uses OpenSSL for HMAC and RSA signing.

---

## encode

Create a signed JWT token with a header, claims, and HMAC-SHA256 signature.

> run "echo -n 'header.payload' | openssl dgst -sha256 -hmac 'secret' -binary | openssl base64"

```markscript
# Encode a JWT
let header = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}"
let payload = "{\"sub\":\"123\",\"name\":\"Alice\",\"iat\":1700000000}"
let secret = "my_secret_key"
let header_b64 = header
let payload_b64 = payload
let signing_input = header_b64 + "." + payload_b64
push("echo -n \"" + signing_input + "\" | openssl dgst -sha256 -hmac \"" + secret + "\" -binary | openssl base64")
call("run")
```

---

## decode

Decode and parse a JWT token into its header, payload, and signature parts.

> print "Decoded JWT parts"

```markscript
let jwt = "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxMjMifQ.signature"
let parts = jwt
# In practice, splits on '.' character
# header = parts[0], payload = parts[1], signature = parts[2]
push("Decoded JWT: header and payload extracted")
call("print")
```

---

## verify

Verify a JWT token's HMAC-SHA256 signature against a secret.

> run "openssl dgst -sha256 -hmac 'secret' -binary | openssl base64"

```markscript
let jwt = "token.value.signature"
let secret = "my_secret_key"
let expected_sig = "expected_base64_sig"
let actual_sig = "computed_base64_sig"
let valid = 0
if expected_sig == actual_sig:
    valid = 1
# valid = 1 if signature matches
```

---

## claims

Extract and display the claims payload from a JWT token.

> print "Decoding JWT claims"

```markscript
let jwt = "header.payload.signature"
let payload_b64 = "payload"
# base64-decode the payload to get JSON claims
push("Decoding JWT claims payload")
call("print")
```

---

## header

Extract and display the JWT header (algorithm and token type).

> print "JWT header decoded"

```markscript
let jwt = "header.payload.signature"
let header_b64 = "header"
# base64-decode the header
push("JWT header decoded")
call("print")
```

---

## sign

Sign a JWT using RSA-SHA256 with a private key.

> run "openssl dgst -sha256 -sign private.pem -out signature.bin signing_input.txt"

```markscript
# Sign JWT payload with RSA private key
let input = "signing_input.txt"
let key = "private.pem"
let sig = "signature.bin"
push("openssl dgst -sha256 -sign \"" + key + "\" -out \"" + sig + "\" \"" + input + "\"")
call("run")
```

---

## expiry

Check whether a JWT token has expired by inspecting the exp claim.

```markscript
let exp = 1700100000
let now = 1700000000
let expired = 0
if now > exp:
    expired = 1
# expired = 1 if token is past its expiry time
```

---

## rsa_verify

Verify a JWT's RSA signature using a public key.

> run "openssl dgst -sha256 -verify pubkey.pem -signature signature.bin signing_input.txt"

```markscript
# Verify RSA signature on a JWT
let input = "signing_input.txt"
let pubkey = "pubkey.pem"
let sig = "signature.bin"
push("openssl dgst -sha256 -verify \"" + pubkey + "\" -signature \"" + sig + "\" \"" + input + "\"")
call("run")
```

---

## es256_sign

Sign a JWT using ECDSA P-256 (ES256 algorithm).

> run "openssl dgst -sha256 -sign ecdsa.pem -out signature.bin payload.txt"

```markscript
# ES256 sign with ECDSA P-256 key
let payload = "payload.txt"
let key = "ecdsa.pem"
let sig = "signature.bin"
push("openssl dgst -sha256 -sign \"" + key + "\" -out \"" + sig + "\" \"" + payload + "\"")
call("run")
```
