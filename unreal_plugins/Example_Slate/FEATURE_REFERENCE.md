# KAIN Editor Feature Reference

**Complete catalog of all editor features supported by the ue5-editor crate**

This document provides a comprehensive reference for every attribute, widget type, property, and pattern available in KAIN's editor system. All features are production-tested and extracted from 4,500+ lines of codegen source code.

## Table of Contents

1. [Slate Widgets](#slate-widgets)
2. [Slate Properties](#slate-properties)
3. [Slate Delegates](#slate-delegates)
4. [Details Panel Customization](#details-panel-customization)
5. [Viewport Features](#viewport-features)
6. [Asset Editor Toolkit](#asset-editor-toolkit)
7. [Editor Module](#editor-module)
8. [Style System](#style-system)
9. [Reactive Optimization](#reactive-optimization)
10. [Advanced Patterns](#advanced-patterns)

---

## Slate Widgets

### Layout Containers

#### SVerticalBox
**Source:** `slate.rs:52`
```kain
VerticalBox()
    .Add(child_widget)
```
Vertical stack layout. Children are arranged top-to-bottom.

#### SHorizontalBox
**Source:** `slate.rs:53`
```kain
HorizontalBox()
    .Add(child_widget)
```
Horizontal stack layout. Children are arranged left-to-right.

#### SGridPanel
**Source:** `slate.rs:54`
```kain
GridPanel()
    .Add(widget.Column(0).Row(0))
    .Add(widget.Column(1).Row(0))
```
Grid layout with explicit column/row positioning.

#### SUniformGridPanel
**Source:** `slate.rs:55`
```kain
UniformGridPanel()
    .Add(widget)
```
Grid with uniform cell sizes.

#### SScrollBox
**Source:** `slate.rs:56`
```kain
ScrollBox()
    .Orientation(Orient_Vertical)
    .Content(child_widget)
```
Scrollable container for overflow content.
