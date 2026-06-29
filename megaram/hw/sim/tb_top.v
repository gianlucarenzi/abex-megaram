/* tb_top.v — Simulation testbench for the ATARI memory-expansion top module.
 *
 * Topology B: SRAM data bus is wired directly to the Atari data bus.
 * The testbench models this by having the SRAM model drive/receive from
 * atari_data (the shared bus), not a separate sram_data wire.
 *
 * Tests:
 *   1. Write PBCTL ($D303) to enable output-register mode (bit 2 = 1).
 *   2. Write PORTB ($D301) to select bank 3 (130XE conf) → bank_r = 3.
 *   3. Read  from banked window ($5000) → check SRAM address & control.
 *   4. Write to banked window ($5000) → check SRAM write pulse.
 *   5. Write PORTB with CBE=1 (bit4=1) → SRAM must not respond.
 *   6. 256K_RAMBO: RAME=1 (bit4=1) → SRAM must not respond.
 *
 * Run with:  iverilog -g2005-sv -o tb tb_top.v rtl/top.v rtl/portb_emu.v \
 *                     rtl/bank_switch.v && vvp tb
 */
`default_nettype none
`timescale 1ns/1ps

module tb_top;

    /* ── DUT signals ────────────────────────────────────────────────────*/
    reg        phi2;
    reg [15:0] addr;
    wire [7:0] atari_data;
    reg        rw;
    reg  [2:0] conf;
    wire       extsel_n;
    wire [19:0] sram_addr;
    wire        sram_ce_n, sram_oe_n, sram_we_n;

    reg [7:0] atari_data_drive;
    reg       atari_data_en;

    /* Atari CPU drives the bus during write cycles */
    assign atari_data = atari_data_en ? atari_data_drive : 8'hZZ;

    /* Minimal SRAM model: 512K × 8.
     * Topology B: SRAM data is on the Atari data bus.
     * Drive atari_data during reads; latch from atari_data on WE_N negedge. */
    reg [7:0] sram_mem [0:524287];
    assign atari_data = (!sram_ce_n && !sram_oe_n && sram_we_n)
                        ? sram_mem[sram_addr[18:0]] : 8'hZZ;
    always @(negedge sram_we_n)
        if (!sram_ce_n) sram_mem[sram_addr[18:0]] <= atari_data;

    /* ── DUT instantiation ──────────────────────────────────────────────*/
    top #(.SRAM_ADDR_BITS(20)) dut (
        .phi2      (phi2),
        .addr      (addr),
        .atari_data(atari_data),
        .rw        (rw),
        .conf      (conf),
        .extsel_n  (extsel_n),
        .sram_addr (sram_addr),
        .sram_ce_n (sram_ce_n),
        .sram_oe_n (sram_oe_n),
        .sram_we_n (sram_we_n)
    );

    /* ── Clock: PHI2 at 1.79 MHz (period ≈ 559 ns) ─────────────────────*/
    initial phi2 = 0;
    always #279 phi2 = ~phi2;

    /* ── Helper tasks ───────────────────────────────────────────────────*/

    /* One full PHI2 write bus cycle */
    task bus_write;
        input [15:0] a;
        input  [7:0] d;
        begin
            @(negedge phi2);        /* start on PHI2 low */
            addr           = a;
            atari_data_drive = d;
            atari_data_en  = 1;
            rw             = 0;
            @(posedge phi2);        /* PHI2 rises: cycle begins */
            @(negedge phi2);        /* PHI2 falls: DUT latches */
            #5;
            atari_data_en = 0;
            rw = 1;
        end
    endtask

    /* One full PHI2 read bus cycle; returns sampled data in rd_data */
    task bus_read;
        input  [15:0] a;
        output  [7:0] rd;
        begin
            @(negedge phi2);
            addr          = a;
            atari_data_en = 0;
            rw            = 1;
            @(posedge phi2);
            #50;                    /* wait for combinational propagation */
            rd = atari_data;
            @(negedge phi2);
            #5;
        end
    endtask

    /* ── Stimulus ───────────────────────────────────────────────────────*/
    integer errors;
    reg [7:0] rd;

    initial begin
        $dumpfile("tb_top.vcd");
        $dumpvars(0, tb_top);

        phi2          = 0;
        addr          = 16'h0000;
        rw            = 1;
        atari_data_en = 0;
        conf          = 3'd0;       /* 130XE */
        errors        = 0;

        /* Seed the SRAM with known pattern */
        sram_mem[20'h01000] = 8'hAB;

        /* ── TEST 1: PBCTL write (enable output register mode) ──────────*/
        #100;
        bus_write(16'hD303, 8'h3C);  /* PBCTL = 0x3C: bit2=1 */
        #50;
        if (dut.u_pia.pbctl !== 8'h3C) begin
            $display("FAIL T1: PBCTL expected 0x3C got 0x%02x", dut.u_pia.pbctl);
            errors = errors + 1;
        end else $display("PASS T1: PBCTL=0x3C");

        /* ── TEST 2: PORTB write → bank 3 (bits 3:2 = 11) ──────────────*/
        bus_write(16'hD301, 8'hEC);  /* portb = 0b11101100 → bits[3:2]=11 → bank=3; bit4=0 */
        #50;
        if (dut.u_pia.portb !== 8'hEC) begin
            $display("FAIL T2: PORTB expected 0xEC got 0x%02x", dut.u_pia.portb);
            errors = errors + 1;
        end else $display("PASS T2: PORTB=0xEC");
        if (dut.bank !== 6'd3) begin
            $display("FAIL T2b: bank expected 3 got %0d", dut.bank);
            errors = errors + 1;
        end else $display("PASS T2b: bank=3");

        /* ── TEST 3: Read from banked window → check SRAM address/control ─
         * Expected SRAM addr = bank(3)<<14 | ($5000 & $3FFF) = $C000|$1000 = $D000
         * Control signals are sampled while PHI2 is still high. */
        @(negedge phi2);
        addr = 16'h5000; rw = 1; atari_data_en = 0;
        @(posedge phi2); #50;          /* sample mid-cycle while PHI2 high */
        if (sram_ce_n !== 1'b0) begin
            $display("FAIL T3: SRAM_CE_N should be 0 during window read");
            errors = errors + 1;
        end else $display("PASS T3: SRAM_CE_N=0");
        if (sram_oe_n !== 1'b0) begin
            $display("FAIL T3b: SRAM_OE_N should be 0 during window read");
            errors = errors + 1;
        end else $display("PASS T3b: SRAM_OE_N=0");
        if (extsel_n !== 1'b0) begin
            $display("FAIL T3c: EXTSEL_N should be 0 during window read");
            errors = errors + 1;
        end else $display("PASS T3c: EXTSEL_N=0");
        $display("     SRAM_ADDR=0x%05X (expected 0x0D000)", sram_addr);
        @(negedge phi2); #5; rw = 1;

        /* ── TEST 4: Write to banked window → check WE_N pulse ──────────*/
        bus_write(16'h5000, 8'h55);
        #50;
        if (sram_mem[20'h0D000] !== 8'h55) begin
            $display("FAIL T4: SRAM[0x0D000] expected 0x55 got 0x%02x", sram_mem[20'h0D000]);
            errors = errors + 1;
        end else $display("PASS T4: SRAM write 0x55 at 0x0D000");

        /* ── TEST 5: CBE=1 (bit4=1) → SRAM must not respond ────────────*/
        bus_write(16'hD301, 8'hFC);  /* portb[4]=1 → CBE set → no external RAM */
        #50;
        @(negedge phi2);
        addr = 16'h5000; rw = 1; atari_data_en = 0;
        @(posedge phi2); #50;
        if (sram_ce_n !== 1'b1) begin
            $display("FAIL T5: SRAM_CE_N should be 1 when CBE=1");
            errors = errors + 1;
        end else $display("PASS T5: SRAM_CE_N=1 with CBE=1 (no ext RAM)");
        if (extsel_n !== 1'b1) begin
            $display("FAIL T5b: EXTSEL_N should be 1 when CBE=1");
            errors = errors + 1;
        end else $display("PASS T5b: EXTSEL_N=1");
        @(negedge phi2); rw = 1;

        /* ── TEST 6: 256K_RAMBO — RAME=1 (bit4=1) must block access ────*/
        conf = 3'd2;  /* EXP_256K_RAMBO */
        bus_write(16'hD303, 8'h04);  /* PBCTL[2]=1 */
        bus_write(16'hD301, 8'hF0);  /* portb[4]=1 → RAME=1 → disabled */
        @(negedge phi2);
        addr = 16'h5000; rw = 1; atari_data_en = 0;
        @(posedge phi2); #50;
        if (sram_ce_n !== 1'b1) begin
            $display("FAIL T6: SRAM_CE_N should be 1 when RAME=1 (256K_RAMBO)");
            errors = errors + 1;
        end else $display("PASS T6: SRAM_CE_N=1 with RAME=1 (256K_RAMBO disabled)");
        @(negedge phi2);

        /* ── RESULT ──────────────────────────────────────────────────────*/
        #200;
        if (errors == 0)
            $display("\n*** ALL TESTS PASSED ***");
        else
            $display("\n*** %0d TEST(S) FAILED ***", errors);
        $finish;
    end

    /* Timeout watchdog */
    initial begin
        #500000;
        $display("TIMEOUT");
        $finish;
    end

endmodule
