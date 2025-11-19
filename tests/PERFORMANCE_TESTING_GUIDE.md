# Zero Ground Performance Testing Guide

## Overview

This guide provides step-by-step instructions for conducting performance tests on the Zero Ground multiplayer shooter. The game includes comprehensive performance monitoring that tracks all critical metrics in real-time.

## Prerequisites

- Visual Studio 2022 with C++ development tools
- SFML 2.6.1 (already configured in project)
- Windows 10/11
- Recommended: Intel i3-8100 or equivalent (8GB RAM)

## Quick Start

### Option 1: Automated Script
```cmd
cd tests
run_performance_tests.bat
```

### Option 2: Manual Execution
```cmd
# Start server
Zero_Ground\x64\Debug\Zero_Ground.exe

# In another terminal, start client
Zero_Ground_client\x64\Debug\Zero_Ground_client.exe
```

## Performance Metrics Explained

### 1. Map Generation Performance

**What it measures:**
- Time to generate a valid playable map
- Number of attempts required
- Wall count and coverage percentage

**Target:** < 100ms

**When measured:** Once at server startup

**Sample output:**
```
=== MAP GENERATION PERFORMANCE ===
Total Time: 45ms (target: <100ms)
Attempts: 1
Walls Generated: 18
Coverage: 28.5%
[SUCCESS] Map generation within target time
===================================
```

**What to check:**
- ✓ Generation time should be under 100ms
- ✓ Should succeed within 1-3 attempts typically
- ✓ Wall count should be 15-25
- ✓ Coverage should be 25-35%

### 2. Real-Time Performance Metrics

**What it measures:**
- FPS (Frames Per Second)
- Frame time in milliseconds
- Player count
- Wall count
- Average collision detection time
- Network bandwidth (sent/received)
- Estimated CPU usage

**Target:** Logged every 1 second during gameplay

**Sample output:**
```
=== PERFORMANCE METRICS ===
FPS: 60.2 (target: 55+)
Frame Time: 16.6ms
Players: 2
Walls: 18
Avg Collision Detection: 0.15ms (target: <1ms)
Network Bandwidth Sent: 1280 bytes/sec
Network Bandwidth Received: 640 bytes/sec
Estimated CPU Usage: 28.5% (target: <40%)
==========================
```

**What to check:**
- ✓ FPS should be 55 or higher
- ✓ Frame time should be ~16-18ms (60 FPS = 16.67ms)
- ✓ Collision detection should be under 1ms
- ✓ CPU usage should be under 40% with 2 players
- ✓ Network bandwidth should be reasonable (~1-2KB/sec)

### 3. Performance Warnings

The system automatically warns when targets are not met:

**FPS Warning:**
```
[WARNING] Performance degradation detected!
  FPS: 52.3 (target: 55+)
  Players: 2
  Walls: 18
  Avg Collision Time: 0.18ms (target: <1ms)
  Avg Frame Time: 19.1ms
  Estimated CPU Usage: 35.2%
```

**Collision Warning:**
```
[WARNING] Collision detection exceeds 1ms target: 1.2ms
```

**CPU Warning:**
```
[WARNING] CPU usage exceeds 40% target: 42.5%
```

## Testing Scenarios

### Scenario 1: Startup Performance Test

**Objective:** Verify map generation meets performance targets

**Steps:**
1. Start the server executable
2. Observe console output immediately
3. Look for "MAP GENERATION PERFORMANCE" section

**Success Criteria:**
- Map generates in < 100ms
- Connectivity validation passes
- Quadtree builds successfully
- No errors or warnings

**Expected Results:**
- Generation time: 30-80ms typical
- Attempts: 1-2 typical
- Walls: 15-25
- Coverage: 25-35%

### Scenario 2: Idle Performance Test

**Objective:** Measure baseline performance with no activity

**Steps:**
1. Start server
2. Connect one client
3. Click READY on client
4. Click PLAY on server
5. Leave both players stationary for 30 seconds
6. Observe performance metrics in server console

**Success Criteria:**
- FPS: 60 (or very close)
- Collision time: < 0.1ms (minimal checks when stationary)
- CPU usage: < 25%
- Network bandwidth: Minimal (~640 bytes/sec)

**Expected Results:**
- Very stable FPS (60)
- Low CPU usage (15-25%)
- Minimal collision checks
- Steady network traffic (position updates only)

### Scenario 3: Active Movement Test

**Objective:** Measure performance during normal gameplay

**Steps:**
1. Start server and connect client
2. Start game (READY → PLAY)
3. Move both players continuously using WASD
4. Move in different directions and patterns
5. Continue for 60 seconds
6. Monitor performance metrics

**Success Criteria:**
- FPS: 55+ consistently
- Collision time: < 1ms average
- CPU usage: < 40%
- Network bandwidth: ~1-2KB/sec

**Expected Results:**
- Stable FPS (55-60)
- Moderate CPU usage (25-35%)
- Collision detection efficient due to Quadtree
- Network traffic increases with movement

### Scenario 4: Collision-Heavy Test

**Objective:** Stress test collision detection system

**Steps:**
1. Start server and connect client
2. Start game
3. Move players into areas with many walls
4. Navigate through tight spaces
5. Deliberately collide with walls repeatedly
6. Monitor collision detection time

**Success Criteria:**
- Collision time: < 1ms even with many walls
- FPS: 55+ maintained
- No performance degradation
- Quadtree optimization working

**Expected Results:**
- Collision time: 0.2-0.8ms typical
- FPS remains stable
- Quadtree reduces checks from O(n) to O(log n)
- CPU usage may increase slightly (30-40%)

### Scenario 5: Network Culling Test

**Objective:** Verify spatial culling optimization

**Steps:**
1. Start server and connect client
2. Start game
3. Move players close together (< 50 units)
4. Note network bandwidth
5. Move players far apart (> 50 units)
6. Note network bandwidth reduction

**Success Criteria:**
- Bandwidth reduces when players far apart
- Network culling working (50-unit radius)
- FPS unaffected by culling
- No visual glitches

**Expected Results:**
- Close together: ~1280 bytes/sec sent
- Far apart: ~640 bytes/sec sent (50% reduction)
- Smooth gameplay regardless of distance
- Culling transparent to players

### Scenario 6: Extended Gameplay Test

**Objective:** Verify performance stability over time

**Steps:**
1. Start server and connect client
2. Start game
3. Play normally for 10 minutes
4. Vary activity (idle, movement, collisions)
5. Monitor for performance degradation
6. Check for memory leaks or slowdowns

**Success Criteria:**
- FPS remains stable throughout
- No gradual performance degradation
- CPU usage stays consistent
- No memory leaks

**Expected Results:**
- Consistent performance over time
- No FPS drops after extended play
- Stable memory usage
- No accumulating lag

## Performance Optimization Features

### 1. Quadtree Spatial Partitioning

**What it does:**
- Divides map into hierarchical spatial regions
- Enables O(log n) collision queries instead of O(n)
- Only checks walls near the player

**Configuration:**
- Max depth: 5 levels
- Max walls per node: 10
- Automatic subdivision when limits exceeded

**Performance impact:**
- Reduces collision checks by 80-90%
- Enables efficient handling of 50+ walls
- Critical for maintaining < 1ms collision time

### 2. Network Spatial Culling

**What it does:**
- Only sends player positions within 50-unit radius
- Reduces network bandwidth significantly
- Transparent to gameplay

**Configuration:**
- Culling radius: 50 units
- Update frequency: 20Hz (50ms intervals)
- Applies to all player position updates

**Performance impact:**
- Reduces bandwidth by 50%+ when players far apart
- Scales better with more players
- Reduces network processing overhead

### 3. Interpolated Movement

**What it does:**
- Smooths player movement between updates
- Uses linear interpolation (lerp)
- Provides 60 FPS visual smoothness with 20Hz updates

**Configuration:**
- Interpolation alpha: Based on delta time
- Previous and current positions stored
- Applied to all player rendering

**Performance impact:**
- Smooth visuals with lower network frequency
- Reduces network bandwidth requirements
- No visual stuttering

### 4. Frame-Independent Movement

**What it does:**
- Uses delta time for movement calculations
- Ensures consistent speed regardless of FPS
- Prevents speed variations

**Configuration:**
- Movement speed: 5 units/second
- Delta time multiplied by 60 for frame independence
- Applied to all player movement

**Performance impact:**
- Consistent gameplay across different hardware
- No speed advantages from higher FPS
- Stable physics simulation

## Troubleshooting Performance Issues

### Issue: Map Generation > 100ms

**Possible causes:**
- Complex connectivity validation
- Many failed attempts
- Slow random number generation

**Solutions:**
1. Check number of attempts in output
2. If > 5 attempts, consider adjusting wall generation
3. Verify BFS algorithm efficiency
4. Check for blocking operations during generation

### Issue: FPS < 55

**Possible causes:**
- Inefficient rendering
- Too many collision checks
- Blocking operations in main loop
- CPU bottleneck

**Solutions:**
1. Check collision detection time (should be < 1ms)
2. Verify Quadtree is being used
3. Profile rendering loop
4. Check for blocking network operations
5. Verify frame rate limiter (should be 60)

### Issue: Collision Detection > 1ms

**Possible causes:**
- Quadtree not working properly
- Too many walls in query area
- Inefficient collision algorithm

**Solutions:**
1. Verify Quadtree construction in logs
2. Check query area size (should be player radius + 1)
3. Ensure early exit on first collision
4. Profile Quadtree query performance
5. Check wall count (should be 15-25)

### Issue: CPU Usage > 40%

**Possible causes:**
- Inefficient rendering
- Too many calculations per frame
- Fog of war overhead
- Network processing overhead

**Solutions:**
1. Profile hot code paths
2. Optimize fog of war calculations
3. Check for unnecessary allocations
4. Verify network thread efficiency
5. Consider reducing update frequency

### Issue: High Network Bandwidth

**Possible causes:**
- Spatial culling not working
- Update frequency too high
- Packet size too large

**Solutions:**
1. Verify culling radius (50 units)
2. Check update frequency (should be 20Hz)
3. Verify packet sizes (PositionPacket = 17 bytes)
4. Check for duplicate sends
5. Monitor player distances

## Performance Testing Checklist

### Pre-Test Setup
- [ ] Server compiled (Debug or Release)
- [ ] Client compiled
- [ ] Arial.ttf font file present
- [ ] SFML DLLs present
- [ ] Console window visible for metrics

### During Testing
- [ ] Map generation time recorded
- [ ] FPS monitored continuously
- [ ] Collision time checked
- [ ] CPU usage observed
- [ ] Network bandwidth noted
- [ ] Warnings/errors logged

### Post-Test Analysis
- [ ] All metrics within targets
- [ ] No performance warnings
- [ ] Stable performance over time
- [ ] No memory leaks
- [ ] Smooth gameplay experience

## Performance Targets Summary

| Metric | Target | Typical | Warning Threshold |
|--------|--------|---------|-------------------|
| Map Generation | < 100ms | 30-80ms | > 100ms |
| FPS | 55+ | 60 | < 55 |
| Frame Time | ~16.67ms | 16-18ms | > 18ms |
| Collision Detection | < 1ms | 0.1-0.3ms | > 1ms |
| CPU Usage (2 players) | < 40% | 25-35% | > 40% |
| Network Bandwidth | Monitored | 1-2KB/sec | N/A |

## Conclusion

The Zero Ground performance monitoring system provides comprehensive real-time metrics for all critical systems. By following this guide and monitoring the console output, you can verify that the game meets all performance targets and identify any optimization opportunities.

For questions or issues, refer to:
- `PERFORMANCE_TEST_RESULTS.md` - Detailed results and analysis
- `README.md` - General project information
- Server console output - Real-time performance data
