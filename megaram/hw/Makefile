# Makefile — ATARI memory-expansion FPGA/CPLD build
#
# Targets:
#   make ice40   — synthesise + P&R for iCE40HX-8K (ct256 package)
#   make ecp5    — synthesise + P&R for ECP5-25k (CABGA256 package)
#   make sim     — run simulation with Icarus Verilog
#   make clean   — remove generated files
#
# Required tools (all available in standard Linux package managers):
#   yosys          https://github.com/YosysHQ/yosys
#   nextpnr-ice40  https://github.com/YosysHQ/nextpnr
#   nextpnr-ecp5   https://github.com/YosysHQ/nextpnr
#   icestorm       https://github.com/YosysHQ/icestorm      (icepack, iceprog)
#   prjtrellis     https://github.com/YosysHQ/prjtrellis    (ecppack)
#   iverilog       https://github.com/steveicarus/iverilog  (simulation)

# ── Source files ────────────────────────────────────────────────────────────
RTL_SRCS  = rtl/top.v rtl/portb_emu.v rtl/bank_switch.v
SIM_SRCS  = $(RTL_SRCS) sim/tb_top.v

TOP       = top

# ── iCE40 settings ──────────────────────────────────────────────────────────
ICE40_DEV     = hx8k
ICE40_PKG     = ct256
ICE40_PCF     = constraints/ice40hx8k.pcf
ICE40_JSON    = build/ice40/top.json
ICE40_ASC     = build/ice40/top.asc
ICE40_BIN     = build/ice40/top.bin

# ── ECP5 settings ───────────────────────────────────────────────────────────
ECP5_DEV      = 25k
ECP5_PKG      = CABGA256
ECP5_LPF      = constraints/ecp5_25k.lpf
ECP5_JSON     = build/ecp5/top.json
ECP5_CONFIG   = build/ecp5/top.config
ECP5_BIT      = build/ecp5/top.bit

# ── Simulation ──────────────────────────────────────────────────────────────
SIM_OUT       = build/sim/tb_top
SIM_VCD       = build/sim/tb_top.vcd

.PHONY: all ice40 ecp5 sim prog_ice40 clean

all: ice40

# ── iCE40 flow ──────────────────────────────────────────────────────────────
ice40: $(ICE40_BIN)

$(ICE40_BIN): $(ICE40_ASC)
	icepack $< $@
	@echo ">>> iCE40 bitstream: $@"

$(ICE40_ASC): $(ICE40_JSON) $(ICE40_PCF)
	@mkdir -p $(dir $@)
	nextpnr-ice40 --$(ICE40_DEV) --package $(ICE40_PKG) \
	              --json $(ICE40_JSON) \
	              --pcf  $(ICE40_PCF) \
	              --asc  $@

$(ICE40_JSON): $(RTL_SRCS)
	@mkdir -p $(dir $@)
	yosys -p "synth_ice40 -top $(TOP) -json $@" $(RTL_SRCS)

# Flash iCE40 via FTDI (iceprog)
prog_ice40: $(ICE40_BIN)
	iceprog $<

# ── ECP5 flow ───────────────────────────────────────────────────────────────
ecp5: $(ECP5_BIT)

$(ECP5_BIT): $(ECP5_CONFIG)
	ecppack $< $@
	@echo ">>> ECP5 bitstream: $@"

$(ECP5_CONFIG): $(ECP5_JSON) $(ECP5_LPF)
	@mkdir -p $(dir $@)
	nextpnr-ecp5 --$(ECP5_DEV) --package $(ECP5_PKG) \
	             --json   $(ECP5_JSON) \
	             --lpf    $(ECP5_LPF) \
	             --textcfg $@

$(ECP5_JSON): $(RTL_SRCS)
	@mkdir -p $(dir $@)
	yosys -p "synth_ecp5 -top $(TOP) -json $@" $(RTL_SRCS)

# ── Simulation ──────────────────────────────────────────────────────────────
sim: $(SIM_VCD)

$(SIM_VCD): $(SIM_OUT)
	cd build/sim && vvp $(notdir $(SIM_OUT))

$(SIM_OUT): $(SIM_SRCS)
	@mkdir -p $(dir $@)
	iverilog -g2005-sv -Wall \
	         -o $@ \
	         $(SIM_SRCS)

# ── Clean ────────────────────────────────────────────────────────────────────
clean:
	rm -rf build/
