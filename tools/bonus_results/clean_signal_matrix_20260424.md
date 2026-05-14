# Clean Signal Matrix Measurements

Current-policy interpretation of the clean signal matrix. Dominant-frequency
values come from the captured DUT/reference sessions; adaptive sampling rates
use the current `8x`, `20-50 Hz`, `5 Hz` step policy.

Source sessions:
- `tools/plot_sessions/20260422_120509_clean_dut_no_ina219_60s_v2/`
- `tools/plot_sessions/20260424_104417_clean_b_dut/`
- `tools/plot_sessions/20260424_104628_clean_c_dut/`

| Signal | Formula | Expected highest | Observed dominant | Adaptive fs | Window n | Notes |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| A | `2*sin(2*pi*3*t)+4*sin(2*pi*5*t)` | `5.00 Hz` | `5.02 Hz` | `40.00 Hz` | `200` | reference case; dominant and highest tone align |
| B | `4*sin(2*pi*3*t)+2*sin(2*pi*9*t)` | `9.00 Hz` | `3.01 Hz` | `25.00 Hz` | `125` | dominant peak is `3 Hz`, so the controller follows the stronger low-frequency tone |
| C | `2*sin(2*pi*2*t)+3*sin(2*pi*5*t)+1.5*sin(2*pi*7*t)` | `7.00 Hz` | `5.03 Hz` | `40.00 Hz` | `200` | dominant peak stays near `5 Hz`, so the current policy targets `40 Hz` |

## Main finding

The current controller follows the FFT dominant peak, not the highest-frequency tone present in the signal. Signal B is the clearest limitation case: it contains `9 Hz`, but the stronger `3 Hz` tone drives the adaptive decision.
