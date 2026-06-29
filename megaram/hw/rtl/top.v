/* top.v — ATARI XL/XE memory-expansion glue logic for CPLD / FPGA.
 *
 * ── Topology (Topology B — glue-logic only) ──────────────────────────────
 *
 * The SRAM data bus is wired DIRECTLY to the Atari data bus on the PCB via
 * 5V↔3.3V bidirectional level-shifters (e.g. TXS0108B).  The FPGA acts
 * only as glue logic: it provides the bank-mapped SRAM address and the SRAM
 * control signals (CE_N, OE_N, WE_N), and also emulates the PIA registers
 * ($D301/$D303) by driving the Atari data bus only for those reads.
 *
 * This matches the topology used by real ATARI memory-expansion boards
 * (RAMBO, Compyshop) where the SRAM is on the Atari bus, not behind a
 * separate FPGA-managed data path.
 *
 * ── Theory of operation ──────────────────────────────────────────────────
 *
 * The ATARI XE/XL computers perform banked-RAM access by writing to PORTB
 * ($D301) after setting PBCTL bit 2 ($D303).  The bank-window is the
 * 16 KB region $4000–$7FFF.  When the CPU accesses that window with the
 * appropriate PORTB value, the FPGA:
 *
 *   1. Asserts EXTSEL_N (active-low) to disable the Atari's internal RAM
 *      in $4000–$7FFF.
 *   2. Presents {bank, addr[13:0]} on the SRAM address bus.
 *   3. Controls SRAM_CE_N, SRAM_OE_N, SRAM_WE_N for the access.
 *      The SRAM data bus drives the Atari data bus directly (reads) or
 *      is driven by the Atari (writes) — the FPGA is not in the data path.
 *
 * Accesses to $D301/$D303 are intercepted to maintain the PORTB and PBCTL
 * shadow registers; the FPGA drives the Atari data bus only for those reads.
 *
 * ── Signal timing ─────────────────────────────────────────────────────────
 *
 *  ┌──────────┐      ┌──────────────────────────────┐
 *  │ PHI2     │      │  HIGH (~279 ns @ 1.79 MHz)   │
 * ─┘          └──────┘                              └────
 *  ← addr valid before PHI2 rise (6502 spec)
 *  ← SRAM_ADDR / CE_N / OE_N asserted combinationally when PHI2 rises
 *  ← SRAM data available within SRAM access time (10 ns for -10 chip)
 *  ← For writes: data valid from Atari during PHI2 high; WE_N = !PHI2·sel
 *  ← Registers (PORTB, PBCTL) updated on PHI2 falling edge
 *
 * ── SRAM sizing ───────────────────────────────────────────────────────────
 *
 *  The parameter SRAM_ADDR_BITS controls the SRAM address width:
 *    19 → AS7C38096A-10TIN  512 K × 8  (covers up to 576K expansion)
 *    20 → larger 1M × 8 chip           (required for 1088K expansion)
 *
 * ── Required tools ────────────────────────────────────────────────────────
 *
 *  Synthesis:  yosys   (https://yosyshq.net/yosys/)
 *  iCE40 P&R:  nextpnr-ice40 + icestorm
 *  ECP5  P&R:  nextpnr-ecp5  + prjtrellis
 */
`default_nettype none
`timescale 1ns/1ps

module top #(
    parameter SRAM_ADDR_BITS = 20   /* 19 for 512K SRAM, 20 for 1M SRAM */
)(
    /* ── Atari bus (all signals 5V; use level-shifters on the PCB) ──────*/
    input  wire        phi2,            /* PHI2 system clock (1.79 MHz)   */
    input  wire [15:0] addr,            /* address bus A0–A15              */
    inout  wire  [7:0] atari_data,      /* data bus D0–D7 (bidirectional)  */
    input  wire        rw,              /* 1 = read, 0 = write             */

    /* ── Expansion-type DIP switches (3 bits = 8 types) ────────────────*/
    input  wire  [2:0] conf,            /* CONF0=conf[0], CONF1, CONF2    */

    /* ── Atari control outputs ──────────────────────────────────────────*/
    output wire        extsel_n,        /* EXTSEL_N: disable internal RAM  */

    /* ── External SRAM — address and control only ───────────────────────
     * The SRAM data bus is wired directly to the Atari data bus on the PCB
     * via bidirectional level-shifters; no SRAM data pins on the FPGA.   */
    output wire [SRAM_ADDR_BITS-1:0] sram_addr,
    output wire        sram_ce_n,
    output wire        sram_oe_n,
    output wire        sram_we_n
);

    /* ── Address decode ─────────────────────────────────────────────────*/

    /* $4000–$7FFF: banked window (addr[15:14] == 2'b01) */
    wire is_window = (addr[15:14] == 2'b01);

    /* $D300–$D3FF: PIA area.
     * The Atari wires A0→RS1, A1→RS0 on the 6520 (reversed), so:
     *   addr[1:0] == 2'b01 → RS = 10 → PORTB / DDRB  ($D301, $D305 …)
     *   addr[1:0] == 2'b11 → RS = 11 → PBCTL / CRB   ($D303, $D307 …) */
    wire is_d3xx   = (addr[15:8] == 8'hD3);
    wire is_portb  = is_d3xx && (addr[1:0] == 2'b01);
    wire is_pbctl  = is_d3xx && (addr[1:0] == 2'b11);
    wire is_pia    = is_portb || is_pbctl;

    /* ── PORTB / PBCTL emulator ─────────────────────────────────────────*/
    wire [7:0] portb, pbctl;
    wire [7:0] pia_data_out;
    wire       pia_data_oe;

    portb_emu u_pia (
        .phi2     (phi2),
        .pia_sel  (is_pia),
        .rs       (addr[1:0]),
        .data_in  (atari_data),
        .rw       (rw),
        .portb    (portb),
        .pbctl    (pbctl),
        .data_out (pia_data_out),
        .data_oe  (pia_data_oe)
    );

    /* ── Bank decoder ───────────────────────────────────────────────────*/
    wire [5:0] bank;
    wire       ram_active;

    bank_switch u_bsw (
        .portb      (portb),
        .pbctl_crb2 (pbctl[2]),
        .conf       (conf),
        .bank       (bank),
        .active     (ram_active)
    );

    /* ── SRAM control ───────────────────────────────────────────────────*/

    /* A cycle hits the external SRAM when: PHI2 high, address in window,
     * ram_active (PORTB / PBCTL configured correctly for this type). */
    wire sram_sel = phi2 && is_window && ram_active;
    wire sram_rd  = sram_sel &&  rw;
    wire sram_wr  = sram_sel && !rw;

    /* SRAM address: upper (SRAM_ADDR_BITS-14) bits = bank LSBs,
     * lower 14 bits = offset within the 16K window.
     * For a 512K SRAM (SRAM_ADDR_BITS=19) only bank[4:0] is used;
     * for a 1M SRAM (SRAM_ADDR_BITS=20) all bank[5:0] bits are used. */
    assign sram_addr = {bank[SRAM_ADDR_BITS-15:0], addr[13:0]};

    assign sram_ce_n = !sram_sel;
    assign sram_oe_n = !sram_rd;
    /* WE_N is active during the whole PHI2-high write period, which gives
     * the SRAM the maximum data-setup time. */
    assign sram_we_n = !sram_wr;

    /* ── Data-bus management ────────────────────────────────────────────
     * The FPGA drives the Atari data bus ONLY for PIA register reads
     * ($D301 / $D303).  For SRAM reads the SRAM chip drives the bus
     * directly (SRAM_OE_N low); the FPGA must tristate. */
    assign atari_data = pia_data_oe ? pia_data_out : 8'hZZ;

    /* ── EXTSEL_N ────────────────────────────────────────────────────────
     * Assert (low) to disable Atari's own $4000–$7FFF SRAM whenever we
     * are serving the cycle from external SRAM. */
    assign extsel_n = !sram_sel;

endmodule
