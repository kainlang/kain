# Csrf

Markscript CSRF protection — cross-site request forgery tokens, validation, and mitigation.
Uses OpenSSL for cryptographic token generation.

---

## generate

Generate a cryptographically random CSRF token.

> run "openssl rand -hex 32"

```markscript
# Generate a CSRF token
push("openssl rand -hex 32")
call("run")
# Returns 64-character hex token
```

---

## validate

Validate a CSRF token against the stored session token. Tokens must match.

```markscript
let stored = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2"
let submitted = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2"
let valid = 0
if stored == submitted:
    valid = 1
# valid = 1 if tokens match
```

---

## token_per_form

Generate a unique CSRF token for each form, tied to the form action.

> run "openssl rand -hex 16"

```markscript
# Generate form-specific CSRF token
let form_id = "login_form"
push("openssl rand -hex 16")
call("run")
# Store: form_id -> generated_token for later validation
```

---

## double_submit

Double-submit cookie pattern — send token in both cookie and request header.

> run "openssl rand -hex 16"

```markscript
# Generate token for double-submit pattern
push("openssl rand -hex 16")
call("run")
# Token is set as a cookie AND added to a custom header
# Server validates that both values match
```

---

## embed_hidden

Create a hidden form field containing the CSRF token for server-side templates.

> print "<input type=\"hidden\" name=\"csrf_token\" value=\"a1b2c3d4e5f6\">"

```markscript
let token = "a1b2c3d4e5f6"
let field = "<input type=\"hidden\" name=\"csrf_token\" value=\"" + token + "\">"
push(field)
call("print")
```

---

## header_check

Validate the CSRF token submitted via a custom HTTP header.

```markscript
let expected = "stored_token_value"
let header_val = "stored_token_value"
let valid = 0
if expected == header_val:
    valid = 1
# valid = 1 if header token matches stored token
```

---

## same_site_check

Check if the request origin matches the site origin (same-site validation).

```markscript
let request_origin = "https://example.com"
let site_origin = "https://example.com"
let same_site = 0
if request_origin == site_origin:
    same_site = 1
# same_site = 1 if request came from the same origin
```

---

## expiry_check

Check whether a CSRF token has expired based on its creation timestamp.

```markscript
let created_at = 1700000000
let now = 1700003600
let max_age = 3600
let expired = 0
let age = now - created_at
if age > max_age:
    expired = 1
# expired = 1 if token is older than max_age seconds
```

---

## regenerate

Invalidate the current CSRF token and generate a new one (post-login).

> run "openssl rand -hex 32"

```markscript
# Regenerate CSRF token after state change
push("openssl rand -hex 32")
call("run")
# Old token is discarded, new token becomes active
```

---

## verify_request

Complete CSRF verification pipeline: validate token presence, match, origin, and expiry.

```markscript
let has_token = 1
let token_matches = 1
let same_origin = 1
let not_expired = 1
let passed = 0
if has_token == 1 and token_matches == 1 and same_origin == 1 and not_expired == 1:
    passed = 1
# passed = 1 if all CSRF checks succeed
```
