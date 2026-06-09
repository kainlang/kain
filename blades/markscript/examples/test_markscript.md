# Markscript Mini-Language Test

## Variable Declaration

```markscript
let x = 5
let y = x + 3
let z = y * 2
```

## Arithmetic

```markscript
let a = 10
let b = a / 2
let c = a - b
```

## While Loop (Countdown)

```markscript
let counter = 5
while counter > 0:
    counter = counter - 1
```

## Conditional

```markscript
let score = 10
if score > 5:
    score = score + 1
else:
    score = 0
```

## Assignment

```markscript
let value = 0
value = 42
```

## Function Call

```markscript
graphics_session_create("Pong", 1024, 768)
```

## Comments

```markscript
# Initialize position
let pos_x = 100
# Initialize velocity
let vel_x = 5
```

## Non-markscript blocks (unchanged behavior)

```kain
let frame = frame + 1
```

```c
free(particles);
```
