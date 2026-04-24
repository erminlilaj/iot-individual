# Clean Signal Matrix Measurements

Measured on the DUT with the current `2.0 * dominant` adaptive rule.

Source sessions:
- `tools/plot_sessions/20260422_120509_clean_dut_no_ina219_60s_v2/`
- `tools/plot_sessions/20260424_104417_clean_b_dut/`
- `tools/plot_sessions/20260424_104628_clean_c_dut/`

| Signal | Formula | Expected highest | Observed dominant | Adaptive fs | Window n | Notes |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| A | `2*sin(2*pi*3*t)+4*sin(2*pi*5*t)` | `5.00 Hz` | `5.00 Hz` | `10.00 Hz` | `50` | baseline case |
| B | `4*sin(2*pi*3*t)+2*sin(2*pi*9*t)` | `9.00 Hz` | `3.01 Hz` | `10.00 Hz` | `50` | dominant peak is `3 Hz`, so the controller does not scale up to `18 Hz` |
| C | `2*sin(2*pi*2*t)+3*sin(2*pi*5*t)+1.5*sin(2*pi*7*t)` | `7.00 Hz` | `5.03 Hz` | `10.10 Hz` | `50` | dominant peak stays near `5 Hz`, so adaptation stays near `10 Hz` |

## Main finding

The current controller follows the FFT dominant peak, not the highest-frequency tone present in the signal. This is why signals B and C remain near `10 Hz` even though they contain `9 Hz` and `7 Hz` components.
