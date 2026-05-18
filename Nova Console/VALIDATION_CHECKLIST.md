## Long Session Stability
- [ ] 1+ hour continuous runtime stable
- [ ] No memory leaks or CPU creep over time
- [ ] No WebView/timer/UI/meter/graph degradation
- [ ] No parameter desynchronization or state corruption
- [ ] No redraw or meter slowdown
- [ ] Repeated automation, preset, oversampling, idle/active, transport, and UI cycles remain stable
# Nova Console Validation Checklist

## Compressor Punch
- [ ] SSL-style punch preserved
- [ ] Attack is reactive, not sluggish
- [ ] Auto-release is musical

## Gate Smoothness
- [ ] No chatter or clicking
- [ ] No unnatural cutoff or pumping

## Analog Buildup
- [ ] No mud buildup with stacking
- [ ] No harshness or mono collapse
- [ ] Subtle, stackable analog feel

## Mono Compatibility
- [ ] No phase issues or collapse

## UI Responsiveness
- [ ] Tight, immediate feel
- [ ] No web-app lag
- [ ] Fast knob drags and graph updates

## Automation Feel
- [ ] No zipper noise or crackles
- [ ] No coefficient instability or chirping
- [ ] No denormal spikes
- [ ] No UI lag during automation

## Preset Recall
- [ ] Presets recall all parameters correctly
- [ ] No state loss or glitches

## Oversampling Behavior
- [ ] Only nonlinear stages oversampled
- [ ] No instability or artifacts when toggling

## Denormal Protection
- [ ] Flush-to-zero enabled globally
- [ ] No denormal spikes in Drift, Heat, Gate, Crosstalk, Analog engine
- [ ] No CPU spikes during silence/tails
- [ ] No idle runaway CPU

## Oversampling Toggle Safety
- [ ] No clicks/pops/phase issues when toggling live
- [ ] No CPU spikes or buffer instability

## Preset Gain Normalization
- [ ] No wild output jumps between presets
- [ ] Loudness consistent for aggressive/mixbus/vocal presets

## Automation Extreme Cases
- [ ] 10+ simultaneous parameter changes stable
- [ ] Rapid ramps, preset/oversampling switching during automation
- [ ] Graph movement during automation

## CPU Logging Granularity
- [ ] Idle, active, oversampling, all modules, multi-instance, automation
- [ ] Peak, average, and worst-case spikes measured

## Analog Engine Stack Test
- [ ] No mud, smear, harshness, low-mid collapse, or harmonic buildup with multiple instances

## UI Stress Test
- [ ] Rapid knob/preset/oversampling/graph changes do not cause lag or bottlenecks

## Realistic Test Signals
- [ ] Vocals, drums, stereo music, transients, low-end, dense mixes tested

## Final Rule
- [ ] Plugin remains subtle, musical, weighted, and premium under stress
