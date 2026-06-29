# ATARI XL/XE Memory Expansion — FPGA/CPLD Hardware Implementation

Pure-hardware alternative to the STM32F429 firmware in `../src/main.c`.
The same bank-switching logic is implemented in synthesisable Verilog,
targeting any Lattice **iCE40** or **ECP5** device with the fully open-source
toolchain (Yosys + nextpnr).

---

## Theory of operation

### Topology (Topology B — glue-logic only)

The SRAM data bus is wired **directly** to the Atari data bus on the PCB via
5V↔3.3V bidirectional level-shifters (e.g. TXS0108B).  The FPGA acts only
as glue logic: it provides the bank-mapped SRAM address and the SRAM control
signals (CE\_N, OE\_N, WE\_N).  This matches the topology used by real ATARI
memory-expansion boards (RAMBO, Compyshop).

### Bank-switching

The ATARI XL/XE selects banked RAM by writing PORTB (`$D301`) after enabling
the output-register mode via PBCTL (`$D303`, bit 2).  The banked window is
`$4000–$7FFF` (16 KB).  The FPGA:

1. Intercepts accesses to `$D301` / `$D303` to maintain shadow copies of
   PORTB and PBCTL.  Drives the Atari data bus only for those register reads.
2. Decodes the current bank number from PORTB according to the DIP-switch
   selected expansion type.
3. When the CPU accesses `$4000–$7FFF` with external RAM enabled:
   - Asserts **EXTSEL\_N** (active-low) → disables the Atari's own SRAM.
   - Presents `{bank[5:0], addr[13:0]}` on the SRAM address bus.
   - Controls SRAM\_CE\_N / SRAM\_OE\_N / SRAM\_WE\_N for the access.
   - For reads: the SRAM drives the Atari data bus directly (OE\_N low);
     the FPGA tristates.
   - For writes: the Atari CPU drives the shared data bus; the SRAM latches
     it when WE\_N is asserted.

### Bus timing

```
        ┌──────────────────────────────┐
PHI2 ───┘ HIGH ≈ 279 ns @ 1.79 MHz    └──────
         │                            │
         ├─ addr valid (before rise)  │
         ├─ SRAM_ADDR / CE_N set      │
         ├─ SRAM data ready < 15 ns   │
         │                            ↓
         PORTB/PBCTL latched on PHI2 falling edge (portb_emu registers)
```

SRAM WE\_N is active for the whole PHI2 high period, giving the SRAM maximum
data-setup time (typically > 270 ns, well above the 7 ns required by the
AS7C38096A-10TIN).

---

## Supported expansion types

| CONF2 | CONF1 | CONF0 | Type              | Banks | Max size |
|:-----:|:-----:|:-----:|:------------------|------:|---------:|
|   0   |   0   |   0   | 130XE             |     4 |    64 KB |
|   0   |   0   |   1   | Compyshop 192K    |     8 |   128 KB |
|   0   |   1   |   0   | RAMBO 256K        |    16 |   256 KB |
|   0   |   1   |   1   | 320K              |    16 |   256 KB |
|   1   |   0   |   0   | Compyshop 320K    |    16 |   256 KB |
|   1   |   0   |   1   | 576K Mod          |    32 |   512 KB |
|   1   |   1   |   0   | Compyshop 576K    |    32 |   512 KB |
|   1   |   1   |   1   | 1088K Mod         |    64 |  1024 KB |

---

## Directory structure

```
hw/
├── rtl/
│   ├── top.v           Top-level: address decode, SRAM control, data-bus mux
│   ├── portb_emu.v     PORTB / PBCTL / DDRB register file (negedge PHI2)
│   └── bank_switch.v   Combinational bank-number decoder (all 8 types)
├── sim/
│   └── tb_top.v        Icarus Verilog testbench
├── constraints/
│   ├── ice40hx8k.pcf   Pin constraints template — iCE40HX-8K (ct256)
│   └── ecp5_25k.lpf    Pin constraints template — ECP5-25k (CABGA256)
├── Makefile
└── README.md           (this file)
```

---

## Required tools

| Tool | Purpose | Install |
|:-----|:--------|:--------|
| **yosys** | Synthesis | `apt install yosys` or build from source |
| **nextpnr-ice40** | iCE40 P&R | `apt install nextpnr-ice40` |
| **nextpnr-ecp5** | ECP5 P&R | `apt install nextpnr-ecp5` |
| **icestorm** (`icepack`, `iceprog`) | iCE40 bitstream | `apt install icestorm` |
| **prjtrellis** (`ecppack`) | ECP5 bitstream | `apt install prjtrellis` |
| **iverilog** | Simulation | `apt install iverilog` |

All tools are available in Debian/Ubuntu:
```bash
sudo apt install yosys nextpnr-ice40 nextpnr-ecp5 fpga-icestorm prjtrellis iverilog
```

---

## Build

```bash
# Simulate (Icarus Verilog)
make sim

# Synthesise + P&R for iCE40HX-8K
make ice40          # → build/ice40/top.bin

# Flash to iCE40 board via FTDI (iceprog)
make prog_ice40

# Synthesise + P&R for ECP5-25k
make ecp5           # → build/ecp5/top.bit
```

---

## Hardware interface

### Signals

| Signal | Dir | Description |
|:-------|:----|:------------|
| `phi2` | IN | PHI2 system clock (1.79 MHz) — **route to clock-capable pin** |
| `addr[15:0]` | IN | Atari address bus A0–A15 |
| `atari_data[7:0]` | BIDIR | Atari data bus D0–D7 (FPGA drives only for PIA reads) |
| `rw` | IN | 1 = read, 0 = write |
| `conf[2:0]` | IN | DIP switches: expansion type (CONF0=LSB) |
| `extsel_n` | OUT | Active-low: disables Atari internal RAM in `$4000–$7FFF` |
| `sram_addr[19:0]` | OUT | SRAM address bus (A0–A19; only A0–A18 used for 512 KB SRAM) |
| `sram_ce_n` | OUT | SRAM Chip Enable (active-low) |
| `sram_oe_n` | OUT | SRAM Output Enable (active-low) |
| `sram_we_n` | OUT | SRAM Write Enable (active-low) |
| *(no sram\_data)* | — | SRAM data bus is wired directly to Atari bus on PCB |

### Level shifting

All Atari bus signals are 5V TTL.  The FPGA operates at 3.3V.  Use a
bidirectional level-shifter on the data bus (e.g. **TXS0108B** or
**TXS0108B**) and a unidirectional shifter on the address bus.

### SRAM

Compatible with any asynchronous SRAM with ≤ 15 ns access time:

| Part | Size | Addr bits | Notes |
|:-----|:-----|:---------:|:------|
| AS7C38096A-10TIN | 512 K × 8 | 19 | Covers up to 576K |
| AS7C4098A-10TIN  | 512 K × 8 | 19 | Pin-compatible |
| AS6C1008-55TIN   | 1 M × 8   | 20 | Required for 1088K |

Set `parameter SRAM_ADDR_BITS` in `top.v` accordingly (default: 20).

---

## Relationship to the STM32 firmware

The PORTB/PBCTL emulation and all bank formulas in `bank_switch.v` are a
direct translation of `init_bank_lut()` in `../src/main.c`.  Any change to
the C code should be reflected here and vice versa.
