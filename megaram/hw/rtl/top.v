/* top.v — ATARI XL/XE memory-expansion glue logic for CPLD / FPGA.
 *
 * This module replaces the STM32F429 bank-switching firmware in
 * abex-megaram with a pure-hardware implementation suitable for any
 * Lattice iCE40 or ECP5 device (open-source toolchain: Yosys + nextpnr).
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
 *   3. Controls SRAM_CE_N, SRAM_OE_N, SRAM_WE_N for the read or write.
 *   4. For reads: drives the decoded SRAM data onto the Atari data bus.
 *
 * Accesses to $D301/$D303 are intercepted to maintain the PORTB and PBCTL
 * shadow registers; all other cycles are ignored.
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

    /* ── External SRAM interface ────────────────────────────────────────*/
    output wire [SRAM_ADDR_BITS-1:0] sram_addr,
    inout  wire  [7:0] sram_data,
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

    /* ── Data-bus management ────────────────────────────────────────────*/

    /* The FPGA drives the Atari data bus during:
     *   - PIA register reads  (PORTB / PBCTL)
     *   - External SRAM reads (data forwarded from sram_data) */
    wire       atari_drv = pia_data_oe || sram_rd;
    wire [7:0] atari_out = pia_data_oe ? pia_data_out : sram_data;

    assign atari_data = atari_drv ? atari_out : 8'hZZ;

    /* SRAM data bus: FPGA drives during writes (Atari → SRAM),
     * tristated during reads (SRAM drives, FPGA only reads). */
    assign sram_data = sram_wr ? atari_data : 8'hZZ;

    /* ── EXTSEL_N ────────────────────────────────────────────────────────
     * Assert (low) to disable Atari's own $4000–$7FFF SRAM whenever we
     * are serving the cycle from external SRAM. */
    assign extsel_n = !sram_sel;

endmodule
