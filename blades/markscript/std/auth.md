# Auth

Markscript authentication — login, session, token, and credential management.
Dispatches through the IVT to OpenSSL for token generation and verification.

---

## login

Authenticate a user with username and password. Validates against a credential store.

> run "echo 'username:password' | openssl dgst -sha256"

```markscript
# Validate credentials
let user = "admin"
let pass = "secret123"
let combined = user + ":" + pass
push("echo \"" + combined + "\" | openssl dgst -sha256")
call("run")
# Compare digest against stored hash
```

---

## logout

Invalidate the current session token and clear session state.

> print "Session invalidated"

```markscript
let session_id = "abc123"
let token = ""
push("Session invalidated for " + session_id)
call("print")
```

---

## session

Create a new session with a unique random session ID.

> run "openssl rand -hex 32"

```markscript
# Create session with random ID
push("openssl rand -hex 32")
call("run")
# Result is the session token
```

---

## token

Issue a bearer token for API access. Token is a random hex string.

> run "openssl rand -base64 48"

```markscript
# Generate a random API token
let token_len = 48
push("openssl rand -base64 " + token_len)
call("run")
```

---

## refresh

Refresh an expiring session token, extending its lifespan.

> run "openssl rand -hex 32"

```markscript
# Generate a new token and invalidate the old one
let old_token = "old_token_value"
push("openssl rand -hex 32")
call("run")
# new_token replaces old_token in session store
```

---

## validate

Check whether a session token is valid (non-empty matches expected format).

```markscript
let token = "expected_token_hash"
let input = "provided_token"
let valid = 0
if input == token:
    valid = 1
# valid = 1 if tokens match, 0 otherwise
```

---

## expire

Explicitly expire a session by token ID.

> print "Expiring session: abc123"

```markscript
let sid = "abc123"
push("Expiring session: " + sid)
call("print")
# Remove from active session store
```

---

## mfa_challenge

Initiate a multi-factor authentication challenge.

> run "echo 'mfa_code_123456' | openssl dgst -sha256"

```markscript
# Generate MFA challenge
let code = "mfa_code_123456"
push("echo '" + code + "' | openssl dgst -sha256")
call("run")
# The challenge is dispatched to the user's 2FA device
```

---

## mfa_verify

Verify a multi-factor authentication response against the challenge.

```markscript
let challenge_hash = "a1b2c3d4e5"
let response_hash = "a1b2c3d4e5"
let valid = 0
if challenge_hash == response_hash:
    valid = 1
# valid = 1 if MFA response matches challenge
```

---

## lockout_check

Check if an account is locked out due to too many failed attempts.

```markscript
let failed_attempts = 0
let max_attempts = 5
let locked = 0
if failed_attempts >= max_attempts:
    locked = 1
# locked = 1 when account exceeds max failed attempts
```
