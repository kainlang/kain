# KAIN Collection Functions

## Overview

KAIN provides 12 essential collection functions that map directly to UE5 TArray operations. These functions enable efficient array manipulation with clean, Pythonic syntax.

**Total Functions:** 12  
**UE5 Type:** `TArray<T>`  
**Primary Include:** `Containers/Array.h`  
**Additional Include:** `Algo/Reverse.h` (for `reverse()`)

---

## Function Reference

### 1. `len(arr)` - Get Array Length

**Signature:** `len(arr: Array<T>) -> Int`  
**UE5 Mapping:** `arr.Num()`  
**Include:** `Containers/Array.h`

Returns the number of elements in the array.

**KAIN Example:**
```kain
var items: Array<Int> = [10, 20, 30]
let size = len(items)  // 3
println("Array has {size} elements")
```

**Generated C++:**
```cpp
TArray<int32> Items = {10, 20, 30};
int32 Size = Items.Num();  // 3
UE_LOG(LogTemp, Log, TEXT("Array has %d elements"), Size);
```

---

### 2. `push(arr, value)` - Add Element

**Signature:** `push(arr: Array<T>, value: T) -> Int`  
**UE5 Mapping:** `arr.Add(value)`  
**Include:** `Containers/Array.h`

Adds an element to the end of the array. Returns the new index.

**KAIN Example:**
```kain
var items: Array<Int> = []
push(items, 10)
push(items, 20)
push(items, 30)
// items = [10, 20, 30]
```

**Generated C++:**
```cpp
TArray<int32> Items;
Items.Add(10);
Items.Add(20);
Items.Add(30);
// Items = {10, 20, 30}
```

---

### 3. `pop(arr)` - Remove Last Element

**Signature:** `pop(arr: Array<T>) -> T`  
**UE5 Mapping:** `arr.Pop()`  
**Include:** `Containers/Array.h`

Removes and returns the last element from the array.

**KAIN Example:**
```kain
var items: Array<Int> = [10, 20, 30]
let last = pop(items)  // 30
// items = [10, 20]
```

**Generated C++:**
```cpp
TArray<int32> Items = {10, 20, 30};
int32 Last = Items.Pop();  // 30
// Items = {10, 20}
```

**Note:** Calling `pop()` on an empty array is undefined behavior in UE5.

---

### 4. `first(arr)` - Get First Element

**Signature:** `first(arr: Array<T>) -> T`  
**UE5 Mapping:** `arr[0]`  
**Include:** `Containers/Array.h`

Returns the first element of the array.

**KAIN Example:**
```kain
var items: Array<Int> = [10, 20, 30]
let first_item = first(items)  // 10
```

**Generated C++:**
```cpp
TArray<int32> Items = {10, 20, 30};
int32 FirstItem = Items[0];  // 10
```

**Note:** Accessing `first()` on an empty array causes a crash. Check with `is_empty()` first.

---

### 5. `last(arr)` - Get Last Element

**Signature:** `last(arr: Array<T>) -> T`  
**UE5 Mapping:** `arr[arr.Num() - 1]`  
**Include:** `Containers/Array.h`

Returns the last element of the array.

**KAIN Example:**
```kain
var items: Array<Int> = [10, 20, 30]
let last_item = last(items)  // 30
```

**Generated C++:**
```cpp
TArray<int32> Items = {10, 20, 30};
int32 LastItem = Items[Items.Num() - 1];  // 30
```

**Note:** Accessing `last()` on an empty array causes a crash. Check with `is_empty()` first.

---

### 6. `reverse(arr)` - Reverse Array In-Place

**Signature:** `reverse(arr: Array<T>) -> Void`  
**UE5 Mapping:** `Algo::Reverse(arr)`  
**Include:** `Algo/Reverse.h`

Reverses the order of elements in the array in-place.

**KAIN Example:**
```kain
var items: Array<Int> = [1, 2, 3, 4, 5]
reverse(items)
// items = [5, 4, 3, 2, 1]
```

**Generated C++:**
```cpp
TArray<int32> Items = {1, 2, 3, 4, 5};
Algo::Reverse(Items);
// Items = {5, 4, 3, 2, 1}
```

---

### 7. `contains(arr, value)` - Check If Value Exists

**Signature:** `contains(arr: Array<T>, value: T) -> Bool`  
**UE5 Mapping:** `arr.Contains(value)`  
**Include:** `Containers/Array.h`

Returns `true` if the array contains the specified value.

**KAIN Example:**
```kain
var items: Array<Int> = [10, 20, 30]
let has_20 = contains(items, 20)  // true
let has_99 = contains(items, 99)  // false
```

**Generated C++:**
```cpp
TArray<int32> Items = {10, 20, 30};
bool Has20 = Items.Contains(20);  // true
bool Has99 = Items.Contains(99);  // false
```

---

### 8. `index_of(arr, value)` - Find Index of Value

**Signature:** `index_of(arr: Array<T>, value: T) -> Int`  
**UE5 Mapping:** `arr.Find(value)`  
**Include:** `Containers/Array.h`

Returns the index of the first occurrence of the value, or `INDEX_NONE` (-1) if not found.

**KAIN Example:**
```kain
var items: Array<Int> = [10, 20, 30, 20]
let index = index_of(items, 20)  // 1 (first occurrence)
let not_found = index_of(items, 99)  // -1
```

**Generated C++:**
```cpp
TArray<int32> Items = {10, 20, 30, 20};
int32 Index = Items.Find(20);  // 1
int32 NotFound = Items.Find(99);  // INDEX_NONE (-1)
```

---

### 9. `remove(arr, index)` - Remove Element at Index

**Signature:** `remove(arr: Array<T>, index: Int) -> Void`  
**UE5 Mapping:** `arr.RemoveAt(index)`  
**Include:** `Containers/Array.h`

Removes the element at the specified index. Shifts all subsequent elements down.

**KAIN Example:**
```kain
var items: Array<Int> = [10, 20, 30, 40]
remove(items, 1)  // Remove 20
// items = [10, 30, 40]
```

**Generated C++:**
```cpp
TArray<int32> Items = {10, 20, 30, 40};
Items.RemoveAt(1);  // Remove 20
// Items = {10, 30, 40}
```

**Note:** Removing an invalid index causes a crash. Validate with `len()` first.

---

### 10. `clear(arr)` - Remove All Elements

**Signature:** `clear(arr: Array<T>) -> Void`  
**UE5 Mapping:** `arr.Empty()`  
**Include:** `Containers/Array.h`

Removes all elements from the array, setting its size to 0.

**KAIN Example:**
```kain
var items: Array<Int> = [10, 20, 30]
clear(items)
// items = []
let size = len(items)  // 0
```

**Generated C++:**
```cpp
TArray<int32> Items = {10, 20, 30};
Items.Empty();
// Items = {}
int32 Size = Items.Num();  // 0
```

---

### 11. `is_empty(arr)` - Check If Array Is Empty

**Signature:** `is_empty(arr: Array<T>) -> Bool`  
**UE5 Mapping:** `arr.IsEmpty()`  
**Include:** `Containers/Array.h`

Returns `true` if the array has no elements.

**KAIN Example:**
```kain
var items: Array<Int> = []
if is_empty(items):
    println("Array is empty")

push(items, 10)
if !is_empty(items):
    println("Array has elements")
```

**Generated C++:**
```cpp
TArray<int32> Items;
if (Items.IsEmpty())
{
    UE_LOG(LogTemp, Log, TEXT("Array is empty"));
}

Items.Add(10);
if (!Items.IsEmpty())
{
    UE_LOG(LogTemp, Log, TEXT("Array has elements"));
}
```

---

### 12. `reserve(arr, capacity)` - Reserve Capacity

**Signature:** `reserve(arr: Array<T>, capacity: Int) -> Void`  
**UE5 Mapping:** `arr.Reserve(capacity)`  
**Include:** `Containers/Array.h`

Pre-allocates memory for the specified number of elements. Does not change the array size.

**KAIN Example:**
```kain
var items: Array<Int> = []
reserve(items, 1000)  // Pre-allocate for 1000 elements

// Now adding elements won't reallocate until 1000
for i in 0..1000:
    push(items, i)
```

**Generated C++:**
```cpp
TArray<int32> Items;
Items.Reserve(1000);  // Pre-allocate for 1000 elements

// Now adding elements won't reallocate until 1000
for (int32 i = 0; i < 1000; ++i)
{
    Items.Add(i);
}
```

**Performance Tip:** Use `reserve()` when you know the final size to avoid multiple reallocations.

---

## Complete Example: Inventory System

```kain
struct Item:
    id: Int
    name: String
    quantity: Int

actor InventoryManager:
    state items: Array<Item> = []
    
    on BeginPlay():
        // Pre-allocate for 100 items
        reserve(items, 100)
        
        // Add some items
        add_item(1, "Health Potion", 5)
        add_item(2, "Mana Potion", 3)
        add_item(3, "Sword", 1)
        
        print_inventory()
    
    fn add_item(id: Int, name: String, quantity: Int):
        var item: Item
        item.id = id
        item.name = name
        item.quantity = quantity
        
        push(items, item)
        println("Added: {name} x{quantity}")
    
    fn remove_item_by_id(id: Int) -> Bool:
        let index = find_item_index(id)
        if index >= 0:
            let item = items[index]
            println("Removing: {item.name}")
            remove(items, index)
            return true
        return false
    
    fn find_item_index(id: Int) -> Int:
        for i in 0..len(items):
            if items[i].id == id:
                return i
        return -1
    
    fn has_item(id: Int) -> Bool:
        return find_item_index(id) >= 0
    
    fn print_inventory():
        println("=== Inventory ===")
        
        if is_empty(items):
            println("Empty")
            return
        
        println("Total items: {len(items)}")
        
        for i in 0..len(items):
            let item = items[i]
            println("{i}: {item.name} x{item.quantity}")
        
        let first_item = first(items)
        let last_item = last(items)
        println("First: {first_item.name}, Last: {last_item.name}")
    
    fn clear_inventory():
        clear(items)
        println("Inventory cleared")
```

---

## Performance Characteristics

| Function | Time Complexity | Notes |
|----------|----------------|-------|
| `len()` | O(1) | Constant time |
| `push()` | O(1) amortized | May reallocate |
| `pop()` | O(1) | Constant time |
| `first()` | O(1) | Direct access |
| `last()` | O(1) | Direct access |
| `reverse()` | O(n) | In-place reversal |
| `contains()` | O(n) | Linear search |
| `index_of()` | O(n) | Linear search |
| `remove()` | O(n) | Shifts elements |
| `clear()` | O(1) | Deallocates |
| `is_empty()` | O(1) | Constant time |
| `reserve()` | O(n) | Pre-allocation |

---

## Best Practices

### 1. Pre-allocate with `reserve()`
```kain
// ❌ Bad: Multiple reallocations
var items: Array<Int> = []
for i in 0..1000:
    push(items, i)

// ✅ Good: Single allocation
var items: Array<Int> = []
reserve(items, 1000)
for i in 0..1000:
    push(items, i)
```

### 2. Check before accessing
```kain
// ❌ Bad: May crash
let first_item = first(items)

// ✅ Good: Safe access
if !is_empty(items):
    let first_item = first(items)
```

### 3. Use `contains()` before `index_of()`
```kain
// ❌ Bad: Check for -1
let index = index_of(items, value)
if index != -1:
    // Use index

// ✅ Good: More readable
if contains(items, value):
    let index = index_of(items, value)
    // Use index
```

### 4. Clear vs. Empty
```kain
// Both do the same thing
clear(items)        // KAIN stdlib
items.Empty()       // Direct UE5 call
```

---

## Testing

Run the test plugin to verify all collection functions:

```bash
cd testing/stdlib
kain build --ue5
```

The `CollectionTest.kn` plugin exercises all 12 functions with comprehensive tests.

---

## Summary

KAIN's collection functions provide a clean, Pythonic interface to UE5's TArray operations:

- **12 functions** covering all essential array operations
- **Type-safe** - works with any `Array<T>`
- **Zero overhead** - direct mapping to UE5 TArray methods
- **Production-ready** - used in all KAIN plugins

For advanced operations (sorting, filtering, mapping), see the upcoming **Algorithms** stdlib module.
