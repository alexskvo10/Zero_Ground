# Performance Metrics Quick Reference

## Quick Start
```cmd
cd tests
run_performance_tests.bat
```

## Performance Targets

| Metric | Target | Typical | Warning |
|--------|--------|---------|---------|
| Map Generation | < 100ms | 30-80ms | > 100ms |
| FPS | 55+ | 60 | < 55 |
| Collision Detection | < 1ms | 0.1-0.3ms | > 1ms |
| CPU Usage | < 40% | 25-35% | > 40% |
| Network Bandwidth | Monitored | 1-2KB/s | N/A |

## Console Output Guide

### At Startup
```
=== MAP GENERATION PERFORMANCE ===
Total Time: 45ms (target: <100ms)
Attempts: 1
Walls Generated: 18
Coverage: 28.5%
[SUCCESS] Map generation within target time
===================================
```
✓ Look for: Time < 100ms, [SUCCESS] message

### During Gameplay (Every Second)
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
✓ Look for: FPS 55+, Collision < 1ms, CPU < 40%

### Performance Warning
```
[WARNING] Performance degradation detected!
  FPS: 52.3 (target: 55+)
  Players: 2
  Walls: 18
  Avg Collision Time: 0.18ms (target: <1ms)
  Avg Frame Time: 19.1ms
  Estimated CPU Usage: 35.2%
```
⚠ Action needed: Check system resources, close other apps

## What Each Metric Means

**FPS (Frames Per Second)**
- How many frames rendered per second
- Target: 55+ (game runs at 60 FPS cap)
- Lower = choppy gameplay

**Frame Time**
- Time to render one frame in milliseconds
- Target: ~16.67ms (60 FPS)
- Higher = lower FPS

**Collision Detection**
- Time to check player-wall collisions
- Target: < 1ms per frame
- Optimized with Quadtree

**CPU Usage**
- Estimated processor usage
- Target: < 40% with 2 players
- Based on frame time

**Network Bandwidth**
- Data sent/received per second
- Sent: Server → Clients
- Received: Clients → Server
- Optimized with spatial culling

## Optimization Features

✓ **Quadtree Spatial Partitioning**
- Reduces collision checks by 80-90%
- O(log n) instead of O(n)

✓ **Network Spatial Culling**
- Only sends players within 50 units
- Reduces bandwidth by 50%+

✓ **Interpolated Movement**
- Smooth 60 FPS with 20Hz updates
- No visual stuttering

✓ **Frame-Independent Movement**
- Consistent speed regardless of FPS
- Uses delta time

## Troubleshooting

**FPS < 55?**
- Close other applications
- Check CPU usage
- Verify collision time < 1ms
- Check for background processes

**Collision > 1ms?**
- Check wall count (should be 15-25)
- Verify Quadtree in logs
- May indicate issue with spatial partitioning

**CPU > 40%?**
- Normal with many players
- Check for memory leaks
- Profile hot code paths
- Consider optimization

**High Network Bandwidth?**
- Verify spatial culling working
- Check player distances
- Should reduce when far apart

## Testing Scenarios

1. **Idle Test**: Both players stationary
   - Expected: 60 FPS, low CPU, minimal bandwidth

2. **Movement Test**: Both players moving
   - Expected: 55+ FPS, moderate CPU, normal bandwidth

3. **Collision Test**: Players near walls
   - Expected: Collision < 1ms, stable FPS

4. **Distance Test**: Players far apart
   - Expected: Reduced bandwidth (culling)

## Documentation

- `PERFORMANCE_TESTING_GUIDE.md` - Full testing procedures
- `PERFORMANCE_TEST_RESULTS.md` - Detailed analysis
- `PERFORMANCE_IMPLEMENTATION_SUMMARY.md` - Implementation details

## Support

If performance issues persist:
1. Check console for warnings
2. Review testing guide
3. Verify hardware meets requirements
4. Check for system resource constraints
