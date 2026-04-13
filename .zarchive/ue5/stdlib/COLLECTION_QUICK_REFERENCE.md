# KAIN Collection Functions - Quick Reference

## One-Page Cheat Sheet

### Array Length & Capacity
```kain
len(arr)              // Get array length → arr.Num()
is_empty(arr)         // Check if empty → arr.IsEmpty()
reserve(arr, 100)     // Pre-allocate capacity → arr.Reserve(100)
```

### Adding & Removing Elements
```kain
push(arr, value)      // Add to end → arr.Add(value)
pop(arr)              // Remove from end → arr.Pop()
remove(arr, index)    // Remove at index → arr.RemoveAt(index)
clear(arr)            // Remove all → arr.Empty()
```

### Accessing Elements
```kain
first(arr)            // Get first element → arr[0]
last(arr)             // Get last element → arr[arr.Num() - 1]
arr[index]            // Direct access (built-in)
```

### Searching
```kain
contains(arr, value)  // Check if exists → arr.Contains(value)
index_of(arr, value)  // Find index → arr.Find(value) (returns -1 if not found)
```

### Manipulation
```kain
reverse(arr)          // Reverse in-place → Algo::Reverse(arr)
```

---

## Common Patterns

### Safe Access
```kain
if !is_empty(arr):
    let first_item = first(arr)
    let last_item = last(arr)
```

### Pre-allocation
```kain
var items: Array<Int> = []
reserve(items, 1000)
for i in 0..1000:
    push(items, i)
```

### Search & Remove
```kain
if contains(arr, value):
    let index = index_of(arr, value)
    remove(arr, index)
```

### Iterate & Process
```kain
for i in 0..len(arr):
    let item = arr[i]
    println("Item {i}: {item}")
```

---

## Performance Tips

| Operation | Complexity | Tip |
|-----------|-----------|-----|
| `push()` | O(1)* | Use `reserve()` to avoid reallocations |
| `pop()` | O(1) | Fast - no shifting needed |
| `remove()` | O(n) | Shifts all elements after index |
| `contains()` | O(n) | Linear search - consider sorting |
| `reverse()` | O(n) | In-place - no extra memory |

*Amortized O(1) - may reallocate occasionally

---

## Safety Checklist

- [ ] Check `is_empty()` before `first()` or `last()`
- [ ] Validate index before `remove(arr, index)`
- [ ] Use `reserve()` when final size is known
- [ ] Check `index_of()` return value (may be -1)
- [ ] Don't modify array while iterating

---

## Complete Example

```kain
actor ArrayDemo:
    state numbers: Array<Int> = []
    
    on BeginPlay():
        // Pre-allocate
        reserve(numbers, 10)
        
        // Add elements
        for i in 0..10:
            push(numbers, i * 10)
        
        // Access
        println("First: {first(numbers)}")
        println("Last: {last(numbers)}")
        println("Length: {len(numbers)}")
        
        // Search
        if contains(numbers, 50):
            let index = index_of(numbers, 50)
            println("Found 50 at index {index}")
        
        // Manipulate
        reverse(numbers)
        println("Reversed!")
        
        // Remove
        remove(numbers, 0)
        println("Removed first element")
        
        // Clean up
        clear(numbers)
        println("Cleared: {is_empty(numbers)}")
```

---

## Include Requirements

Most functions: `#include "Containers/Array.h"`  
`reverse()`: `#include "Algo/Reverse.h"`

(Automatically added by KAIN compiler)

---

## See Also

- [Full Documentation](COLLECTION_FUNCTIONS.md)
- [Math Functions](STDLIB_MATH_FUNCTIONS.md)
- [Test Plugin](../../testing/stdlib/CollectionTest.kn)
