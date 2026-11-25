# Nova Audio changes to ptformat

Upstream: https://github.com/zamaudio/ptformat (LGPL-2.1, see COPYING.LESSER), fetched 2026-11-25.

1. `_ratefactor` is a `double` (was `float`): positions past ~16.7M samples
   (about 6 minutes at 44.1 kHz) were rounded by the float conversion.
2. `load (path, targetsr)` with `targetsr <= 0` keeps the session's own sample rate.
3. Rescaled positions are rounded (`llround`) instead of truncated.

`ptx2json.cc` (ours) is the helper PT Converter runs to read a .ptx.
