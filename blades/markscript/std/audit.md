# Audit

Markscript security audit — log monitoring, compliance checking, vulnerability scanning.
Dispatches through the IVT to system tools and custom logic.

---

## log

Write a security audit event with timestamp and severity.

> print "[AUDIT] 2026-06-10T12:00:00Z INFO User login: admin from 192.168.1.1"

```markscript
# Write audit log entry
let timestamp = "2026-06-10T12:00:00Z"
let severity = "INFO"
let user = "admin"
let ip = "192.168.1.1"
let msg = "[AUDIT] " + timestamp + " " + severity + " User login: " + user + " from " + ip
push(msg)
call("print")
```

---

## watch

Monitor a log file for new entries matching a pattern.

> run "powershell -Command \"Get-Content C:\\logs\\security.log -Tail 0 -Wait\""

```markscript
# Tail and watch a log file
let logpath = "C:\\logs\\security.log"
push("powershell -Command \"Get-Content '" + logpath + "' -Tail 0 -Wait\"")
call("spawn")
```

---

## report

Generate a security audit report summarizing events by severity.

> run "findstr /R /C:\"ERROR\\|WARN\\|CRIT\" C:\\logs\\security.log"

```markscript
# Extract critical events from log
let logpath = "C:\\logs\\security.log"
push("findstr /R /C:\"ERROR\\|WARN\\|CRIT\" \"" + logpath + "\"")
call("run")
```

---

## alert

Send a security alert notification for a critical event.

> print "[ALERT] CRITICAL: Unauthorized access attempt detected from 10.0.0.99"

```markscript
# Raise a security alert
let ip = "10.0.0.99"
let event = "Unauthorized access attempt"
let alert_msg = "[ALERT] CRITICAL: " + event + " detected from " + ip
push(alert_msg)
call("print")
```

---

## compliance_check

Check that the system meets a specific compliance requirement.

> run "python -c \"# Check for encryption at rest policy\""

```markscript
# Run a compliance check
let policy = "encryption_at_rest"
let passed = 0
# In practice, check BitLocker, filevault, dm-crypt status
let drive_encrypted = 1
if drive_encrypted == 1:
    passed = 1
# passed = 1 if compliance requirement met
```

---

## scan

Run a security scan for open ports on a target host.

> run "netstat -an | findstr LISTENING"

```markscript
# Scan for listening ports on localhost
push("netstat -an | findstr LISTENING")
call("run")
```

---

## scan_remote

Run an external port scan against a remote host using nmap.

> run "nmap -sS -sV -p 1-1000 target.example.com"

```markscript
# Port scan remote host
let target = "target.example.com"
let ports = "1-1000"
push("nmap -sS -sV -p " + ports + " " + target)
call("run")
```

---

## vulnerability_check

Check the system for known vulnerabilities (requires vuln database).

> run "python -c \"print('Vuln check: 0 critical, 2 high, 5 medium')\""

```markscript
# Run vulnerability assessment
push("python -c \"print('Vuln check: 0 critical, 2 high, 5 medium')\"")
call("run")
```

---

## file_integrity

Compute file hashes to detect unauthorized modifications.

> run "certutil -hashfile C:\\system\\important.dll SHA256"

```markscript
# Hash a critical system file
let fpath = "C:\\system\\important.dll"
push("certutil -hashfile \"" + fpath + "\" SHA256")
call("run")
```

---

## baseline_compare

Compare current file hashes against a stored baseline to detect changes.

```markscript
let current_hash = "a1b2c3d4e5"
let baseline_hash = "a1b2c3d4e5"
let modified = 0
if current_hash != baseline_hash:
    modified = 1
# modified = 1 if file has changed since baseline
```

---

## failed_login_monitor

Count failed login attempts from audit logs within a time window.

> run "findstr /C:\"Failed login\" C:\\logs\\security.log"

```markscript
# Count recent failed logins
let logpath = "C:\\logs\\security.log"
push("findstr /C:\"Failed login\" \"" + logpath + "\"")
call("run")
```

---

## log_rotation

Rotate the audit log file, archiving the current one.

> run "move C:\\logs\\security.log C:\\logs\\security-2026-06-10.log"

```markscript
# Rotate audit log
let current = "C:\\logs\\security.log"
let archive = "C:\\logs\\security-2026-06-10.log"
push("move \"" + current + "\" \"" + archive + "\"")
call("run")
# Create fresh empty log
push("echo '' > \"" + current + "\"")
call("run")
```
