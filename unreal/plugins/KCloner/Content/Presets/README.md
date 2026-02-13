# K-Cloner Custom Modifier Presets

Create your own modifiers using math expressions! No C++ required.

## Creating a Preset

1. Right-click in Content Browser → `K-Cloner` → `Modifier Preset`
2. Fill in the expression fields
3. Add variables for sliders
4. Use in any K-Cloner via the `Preset` modifier

## Expression Variables

| Variable | Description |
|----------|-------------|
| `t` | Current time in seconds |
| `i` | Instance index (0, 1, 2, ...) |
| `n` | Total instance count |
| `x`, `y`, `z` | Current position |
| `rx`, `ry`, `rz` | Current rotation (Pitch, Yaw, Roll in degrees) |
| `sx`, `sy`, `sz` | Current scale |
| `v0`, `v1`, `v2`... | User-defined slider values |
| `pi` | π (3.14159...) |
| `e` | Euler's number (2.71828...) |

## Supported Functions

**Trigonometry**: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sinh`, `cosh`, `tanh`

**Math**: `abs`, `floor`, `ceil`, `round`, `trunc`, `frac`, `mod`

**Power/Log**: `sqrt`, `pow`, `exp`, `log`, `log2`, `log10`

**Comparison**: `min`, `max`, `clamp`

**Interpolation**: `lerp(a, b, t)`

## Example Expressions

### DNA Helix (Position)
```
x := x + sin(t * 2 + i * 0.3) * v0;
z := z + cos(t * 2 + i * 0.3) * v0;
y := y + i * v1;
```
Variables: `v0` = Radius (50), `v1` = Height Step (10)

### Spiral Orbit (Position + Rotation)
Position:
```
x := x + cos(t + i * 0.1) * (100 + i * v0);
z := z + sin(t + i * 0.1) * (100 + i * v0);
```
Rotation:
```
ry := ry + t * 45 + i * v1;
```
Variables: `v0` = Expansion (5), `v1` = Spin (10)

### Heartbeat Pulse (Scale)
```
var amplitude := 1 + sin(t * v0 * 6.28) * 0.3 + sin(t * v0 * 12.56 + i * 0.1) * 0.1;
sx := sx * amplitude;
sy := sy * amplitude;
sz := sz * amplitude;
```
Variables: `v0` = BPM/60 (1.0)

### Matrix Fall (Position)
```
y := y - mod(t * v0 + i * v1, v2);
```
Variables: `v0` = Fall Speed (100), `v1` = Offset (50), `v2` = Reset Height (500)

### Hologram Glitch (Position + Scale)
Position:
```
x := x + (frac(sin(floor(t * 20 + i) * 12.9898) * 43758.5453) - 0.5) * v0 * step(0.9, frac(t * 3 + i * 0.1));
```
Scale:
```
var glitch := step(0.95, frac(t * 5 + i * 0.2));
sx := sx * (1 - glitch * 0.5);
sy := sy * (1 + glitch);
```
Variables: `v0` = Glitch Intensity (20)

## Tips

1. **Use `:=` for assignment, `+=` to add**
2. **Expressions are evaluated per-instance every frame**
3. **Variables are compiled once, then cached for performance**
4. **Test with small instance counts first**
5. **Chain multiple statements with `;`**

## DLC Packs

Modifier presets can be distributed as DLC packs! Simply:
1. Create presets in a separate plugin
2. Package as a .pak file
3. Users drop into their project

---
*K-Cloner Preset System - Powered by ExprTk*
