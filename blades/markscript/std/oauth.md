# Oauth

Markscript OAuth 2.0 — authorization flows, token management, and client credentials.
Dispatches through the IVT to curl for HTTP interactions with identity providers.

---

## authorize

Initiate the OAuth 2.0 authorization code flow. Builds the authorization URL.

> print "Redirect user to: https://auth.example.com/authorize?response_type=code&client_id=abc&redirect_uri=https://app.com/callback&scope=openid+profile"

```markscript
# Build authorization URL
let client_id = "abc123"
let redirect = "https://app.com/callback"
let scope = "openid+profile"
let auth_url = "https://auth.example.com/authorize?response_type=code&client_id=" + client_id + "&redirect_uri=" + redirect + "&scope=" + scope
push("Redirect user to: " + auth_url)
call("print")
```

---

## token

Exchange an authorization code for an access token.

> run "curl -s -X POST https://auth.example.com/token -d \"grant_type=authorization_code&code=AUTH_CODE&redirect_uri=https://app.com/callback&client_id=abc&client_secret=secret\""

```markscript
# Exchange code for token
let code = "AUTH_CODE_XYZ"
let client_id = "abc123"
let client_secret = "secret123"
let redirect = "https://app.com/callback"
let payload = "grant_type=authorization_code&code=" + code + "&redirect_uri=" + redirect + "&client_id=" + client_id + "&client_secret=" + client_secret
push("curl -s -X POST https://auth.example.com/token -d \"" + payload + "\"")
call("run")
```

---

## refresh

Refresh an expiring access token using a refresh token.

> run "curl -s -X POST https://auth.example.com/token -d \"grant_type=refresh_token&refresh_token=REFRESH_TOKEN&client_id=abc&client_secret=secret\""

```markscript
# Refresh access token
let refresh_token = "REFRESH_TOKEN_XYZ"
let client_id = "abc123"
let client_secret = "secret123"
let payload = "grant_type=refresh_token&refresh_token=" + refresh_token + "&client_id=" + client_id + "&client_secret=" + client_secret
push("curl -s -X POST https://auth.example.com/token -d \"" + payload + "\"")
call("run")
```

---

## revoke

Revoke an access or refresh token, invalidating it immediately.

> run "curl -s -X POST https://auth.example.com/revoke -d \"token=ACCESS_TOKEN&client_id=abc&client_secret=secret\""

```markscript
# Revoke a token
let token = "ACCESS_TOKEN_XYZ"
let client_id = "abc123"
let client_secret = "secret123"
let payload = "token=" + token + "&client_id=" + client_id + "&client_secret=" + client_secret
push("curl -s -X POST https://auth.example.com/revoke -d \"" + payload + "\"")
call("run")
```

---

## client_credentials

Use the OAuth 2.0 client credentials flow for server-to-server auth.

> run "curl -s -X POST https://auth.example.com/token -d \"grant_type=client_credentials&client_id=abc&client_secret=secret&scope=api\""

```markscript
# Client credentials grant
let client_id = "abc123"
let client_secret = "secret123"
let scope = "api"
let payload = "grant_type=client_credentials&client_id=" + client_id + "&client_secret=" + client_secret + "&scope=" + scope
push("curl -s -X POST https://auth.example.com/token -d \"" + payload + "\"")
call("run")
```

---

## pkce

Generate a PKCE (Proof Key for Code Exchange) code verifier and challenge.

> run "python -c \"import base64, hashlib, os; v=base64.urlsafe_b64encode(os.urandom(32)).decode().rstrip('='); c=base64.urlsafe_b64encode(hashlib.sha256(v.encode()).digest()).decode().rstrip('='); print(f'verifier={v} challenge={c}')\""

```markscript
# Generate PKCE verifier and challenge
push("python -c \"import base64, hashlib, os; v=base64.urlsafe_b64encode(os.urandom(32)).decode().rstrip('='); c=base64.urlsafe_b64encode(hashlib.sha256(v.encode()).digest()).decode().rstrip('='); print(f'verifier={v} challenge={c}')\"")
call("run")
```

---

## implicit

OAuth 2.0 implicit flow (deprecated, but documented for legacy use).

> print "Warning: Implicit flow is deprecated. Use authorization code with PKCE instead."

```markscript
let client_id = "abc123"
let redirect = "https://app.com/callback"
let scope = "openid+profile"
let auth_url = "https://auth.example.com/authorize?response_type=token&client_id=" + client_id + "&redirect_uri=" + redirect + "&scope=" + scope
push("Warning: Implicit flow is deprecated. Use authorization code with PKCE instead.")
call("print")
```

---

## token_introspect

Introspect an access token to check its validity and metadata.

> run "curl -s -X POST https://auth.example.com/introspect -d \"token=ACCESS_TOKEN&client_id=abc&client_secret=secret\""

```markscript
# Check token validity and metadata
let token = "ACCESS_TOKEN_XYZ"
let client_id = "abc123"
let client_secret = "secret123"
let payload = "token=" + token + "&client_id=" + client_id + "&client_secret=" + client_secret
push("curl -s -X POST https://auth.example.com/introspect -d \"" + payload + "\"")
call("run")
```

---

## device_code

Start the OAuth 2.0 device authorization flow (for headless devices).

> run "curl -s -X POST https://auth.example.com/device_authorization -d \"client_id=abc&scope=openid\""

```markscript
# Initiate device code flow
let client_id = "abc123"
let payload = "client_id=" + client_id + "&scope=openid"
push("curl -s -X POST https://auth.example.com/device_authorization -d \"" + payload + "\"")
call("run")
```

---

## device_token

Poll for the access token in the device authorization flow.

> run "curl -s -X POST https://auth.example.com/token -d \"grant_type=urn:ietf:params:oauth:grant-type:device_code&device_code=DEVICE_CODE&client_id=abc\""

```markscript
# Poll for device grant token
let device_code = "DEVICE_CODE_XYZ"
let client_id = "abc123"
let payload = "grant_type=urn:ietf:params:oauth:grant-type:device_code&device_code=" + device_code + "&client_id=" + client_id
push("curl -s -X POST https://auth.example.com/token -d \"" + payload + "\"")
call("run")
```

---

## validate_token

Validate a JWT access token locally (check signature and expiry).

```markscript
let token = "eyJhbGciOiJSUzI1NiJ9.payload.signature"
let current_time = 1700000000
let exp = 1700100000
let valid = 0
if current_time < exp:
    valid = 1
# valid = 1 if token is not expired
```
