# Tick Optimizer - Quick Usage Guide

## Installation (5 minutes)

1. **Build the plugin:**
   ```bash
   cd Factory/TickOptimizer
   rebuild.bat
   ```

2. **Copy to your UE5 project:**
   ```bash
   xcopy /E /I Factory\TickOptimizer YourProject\Plugins\TickOptimizer
   ```

3. **Regenerate project files** (right-click .uproject → Generate Visual Studio project files)

4. **Compile in UE5** (Build → Build Solution in Visual Studio)

5. **Enable the plugin** (Edit → Plugins → search "Tick Optimizer" → check Enabled → restart)

## Quick Start (2 minutes)

### Option 1: Use the Dashboard (Recommended)

1. Open: `Tools > Tick Optimizer > Open Dashboard`
2. Click the **power button** to enable optimization
3. Select preset: **Balanced** (default)
4. Done! Your actors are now optimized.

### Option 2: Blueprint Setup

1. Add `TickOptimizerSubsystem` actor to your level
2. Set **Is Enabled** to `true`
3. Set **Optimization Mode** to `Balanced`
4. Play the game - optimization runs automatically

## Configuration Presets

### Conservative (Minimal Impact)
- Near: Every frame
- Medium: 20 FPS
- Far: 5 FPS
- Very Far: 2 FPS
- **Best for:** High-fidelity games, VR

### Balanced (Recommended)
- Near: Every frame
- Medium: 10 FPS
- Far: 2 FPS
- Very Far: Disabled
- **Best for:** Most games

### Aggressive (Maximum Performance)
- Near: 20 FPS
- Medium: 5 FPS
- Far: 1 FPS
- Very Far: Disabled
- **Best for:** Large open worlds, mobile

## Whitelist Critical Actors

Some actors should never be throttled:

```cpp
// In Blueprint or C++
ATickOptimizerSubsystem* Subsystem = GetWorld()->GetSubsystem<ATickOptimizerSubsystem>();

// Whitelist player and enemies
Subsystem->Server_AddToWhitelist("APlayerCharacter");
Subsystem->Server_AddToWhitelist("AEnemy");
Subsystem->Server_AddToWhitelist("AProjectile");
```

Or use the dashboard:
1. Open dashboard
2. Go to "Whitelist" tab
3. Add actor classes

## Monitor Performance

### Real-Time Stats
Open the dashboard to see:
- **Actors Optimized**: How many actors are being throttled
- **Actors Whitelisted**: How many actors are exempt
- **CPU Time Saved**: Milliseconds saved per frame
- **Performance Graph**: Before/after comparison

### Profile Mode
Enable profile mode to see detailed logs:
1. Open dashboard
2. Enable "Profile Mode" toggle
3. Check Output Log every 5 seconds

Example output:
```
=== TICK OPTIMIZER PROFILE ===
Total Actors: 1523
Optimized: 1245
Whitelisted: 278
CPU Time Saved: 8.42ms
==============================
```

## DataTable Configuration (Advanced)

### Create Optimization Rules

1. Create DataTable from `TickOptimizationRule`
2. Add rows:

| Distance Band | Tick Interval | Screen Visibility | Enabled |
|---------------|---------------|-------------------|---------|
| Near          | 0.0           | false             | true    |
| Medium        | 0.1           | true              | true    |
| Far           | 0.5           | true              | true    |
| VeryFar       | -1.0          | true              | true    |

### Create Actor Class Overrides

1. Create DataTable from `ActorClassOverride`
2. Add rows:

| Actor Class Name | Force Always Tick | Custom Interval | Profile Category |
|------------------|-------------------|-----------------|------------------|
| APlayerCharacter | true              | -1.0            | Gameplay         |
| AEnemy           | true              | -1.0            | Gameplay         |
| ADecoration      | false             | 1.0             | Decoration       |

## Blueprint Functions

### Get Stats
```cpp
// Get optimization stats
int32 ActorsOptimized = Subsystem->GetActorsOptimized();
int32 TotalActors = Subsystem->GetTotalActorsTracked();
float CPUTimeSaved = Subsystem->GetCPUTimeSaved();
```

### Control Optimization
```cpp
// Enable/disable
Subsystem->Server_SetEnabled(true);

// Load preset
Subsystem->Server_LoadPreset(ETickOptimizationMode::Balanced);

// Enable profile mode
Subsystem->Server_SetProfileMode(true);
```

### Utility Functions
```cpp
// Get distance band for a distance
ETickDistanceBand Band = UTickOptimizerBlueprintLibrary::GetDistanceBandForDistance(3500.0);
// Returns: Medium

// Get tick interval for a band
float Interval = UTickOptimizerBlueprintLibrary::GetTickIntervalForBand(Band, Mode);

// Get color for visualization
FVector Color = UTickOptimizerBlueprintLibrary::GetDistanceBandColor(Band);
// Returns: Yellow (1.0, 1.0, 0.0)

// Format tick interval for UI
FString Display = UTickOptimizerBlueprintLibrary::FormatTickInterval(0.1);
// Returns: "10 FPS"
```

## Expected Performance Gains

### Small Worlds (< 500 actors)
- Conservative: 5-10% CPU reduction
- Balanced: 10-20% CPU reduction
- Aggressive: 20-30% CPU reduction

### Medium Worlds (500-2000 actors)
- Conservative: 10-20% CPU reduction
- Balanced: 20-40% CPU reduction
- Aggressive: 40-60% CPU reduction

### Large Worlds (> 2000 actors)
- Conservative: 20-30% CPU reduction
- Balanced: 40-60% CPU reduction
- Aggressive: 60-80% CPU reduction

## Troubleshooting

### Actors not being optimized
- ✓ Check if optimization is enabled
- ✓ Verify actor has `bCanEverTick = true`
- ✓ Check if actor class is whitelisted
- ✓ Ensure actor is within optimization distance

### Gameplay feels sluggish
- Switch to Conservative preset
- Whitelist more actor classes
- Increase tick intervals (lower throttling)
- Disable screen visibility checks

### Performance not improving
- Enable profile mode to see which actors tick most
- Check if most actors are whitelisted
- Verify actors are actually being throttled (check dashboard)
- Consider Aggressive preset

## Best Practices

### ✅ Do:
- Whitelist gameplay-critical actors (player, enemies, projectiles)
- Use Balanced preset as starting point
- Enable screen visibility for visual-only actors
- Monitor profile mode to identify bottlenecks
- Test in your target environment

### ❌ Don't:
- Throttle actors with time-sensitive logic
- Use Aggressive preset without testing
- Forget to whitelist networked actors in multiplayer
- Disable optimization in shipping builds

## Support

- Email: support@kainlabs.com
- Discord: discord.gg/kainlabs
- Documentation: docs.kainlabs.com/tick-optimizer

---

**Built with KAIN** - Production-ready in < 10 minutes
