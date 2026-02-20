# KAIN Standard Library Implementation Guide

> **Last Updated:** Feb 19, 2026  
> **Purpose:** Complete reference for implementing KAIN stdlib across all backends  
> **Status:** Research complete — 80+ functions cataloged, UE5 mappings identified

---

## Executive Summary

KAIN has a comprehensive standard library with **80+ built-in functions** spanning math, collections, strings, I/O, HTTP, JSON, async, and Python FFI. Currently:

- ✅ **Interpreter (runtime.rs):** All 80+ functions fully implemented
- ⚠️ **UE5 Backend:** Only `print`/`println` mapped, math functions partially mapped in codegen
- ❌ **WASM Backend:** No stdlib support
- ❌ **LLVM Backend:** No stdlib support
- ❌ **Rust Backend:** No stdlib support

**This document provides the roadmap to achieve full stdlib parity across all backends.**

---

## Table of Contents

1. [Function Inventory](#1-function-inventory)
2. [Backend Mapping Tables](#2-backend-mapping-tables)
3. [Implementation Priority](#3-implementation-priority)
4. [Code Generation Strategy](#4-code-generation-strategy)
5. [UE5 Deep Dive](#5-ue5-deep-dive)
6. [Runtime Linking Strategy](#6-runtime-linking-strategy)
7. [Testing Strategy](#7-testing-strategy)

---

## 1. Function Inventory

### 1.1 Math Functions (20 functions)

| Function | Signature | Description | Runtime Status |
|----------|-----------|-------------|----------------|
| `abs` | `(Int\|Float) -> Int\|Float` | Absolute value | ✅ |
| `sqrt` | `(Float) -> Float` | Square root | ✅ |
| `pow` | `(Float, Float) -> Float` | Power | ❌ |
| `sin` | `(Float) -> Float` | Sine | ✅ |
| `cos` | `(Float) -> Float` | Cosine | ✅ |
| `tan` | `(Float) -> Float` | Tangent | ✅ |
| `asin` | `(Float) -> Float` | Arcsine | ❌ |
| `acos` | `(Float) -> Float` | Arccosine | ❌ |
| `atan` | `(Float) -> Float` | Arctangent | ❌ |
| `atan2` | `(Float, Float) -> Float` | Two-argument arctangent | ❌ |
| `floor` | `(Float) -> Int` | Floor | ❌ |
| `ceil` | `(Float) -> Int` | Ceiling | ❌ |
| `round` | `(Float) -> Int` | Round to nearest | ❌ |
| `min` | `(Int, Int) -> Int` | Minimum of two values | ✅ |
| `max` | `(Int, Int) -> Int` | Maximum of two values | ✅ |
