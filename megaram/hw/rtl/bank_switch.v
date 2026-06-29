/* bank_switch.v — Combinational bank-number decoder for ATARI XL/XE memory expansions.
 *
 * Mirrors the bank_lut[] table from abex-megaram/megaram/src/main.c.
 * All eight expansion types are supported; the active type is selected by
 * the three DIP-switch bits on conf[2:0].
 *
 * Outputs:
 *   bank[5:0]  — 6-bit bank index (0–63); meaningless when active=0
 *   active     — 1 when the external SRAM should respond to the current cycle
 *
 * Conditions for active=1 (in addition to expansion-specific checks):
 *   - pbctl_crb2 must be 1 (PBCTL bit 2 set → output register mode;
 *     if 0 the $D301 access goes to the DDR, not PORTB)
 */
`default_nettype none

module bank_switch (
    input  wire [7:0] portb,        /* current PORTB value                  */
    input  wire       pbctl_crb2,   /* PBCTL bit 2: 1 = output register mode */
    input  wire [2:0] conf,         /* DIP-switch expansion type select      */
    output wire [5:0] bank,         /* bank index                            */
    output wire       active         /* 1 = external SRAM should respond      */
);
    /* Expansion-type codes — match EXPANSION_TYPE_* in main.h */
    localparam [2:0]
        EXP_130XE          = 3'd0,  /* 64K   extended  (4 banks × 16K)       */
        EXP_192K           = 3'd1,  /* 128K  extended  (8 banks × 16K)       */
        EXP_256K_RAMBO     = 3'd2,  /* 256K  RAMBO     (16 banks × 16K)      */
        EXP_320K           = 3'd3,  /* 256K  extended  (16 banks × 16K)      */
        EXP_320K_COMPYSHOP = 3'd4,  /* 256K  Compyshop (16 banks × 16K)      */
        EXP_576K_MOD       = 3'd5,  /* 512K  Mod       (32 banks × 16K)      */
        EXP_576K_COMPYSHOP = 3'd6,  /* 512K  Compyshop (32 banks × 16K)      */
        EXP_1088K_MOD      = 3'd7;  /* 1024K Mod       (64 banks × 16K)      */

    reg [5:0] bank_r;
    reg       active_r;

    always @(*) begin
        bank_r   = 6'd0;
        active_r = 1'b0;

        if (pbctl_crb2) begin
            case (conf)

                /* ----------------------------------------------------------
                 * 130XE: CBE (bit 4) = 0 enables CPU access to extended RAM.
                 * VBE (bit 5) is for ANTIC DMA only, never visible on PHI2.
                 * Bank bits: portb[3:2] → 4 banks
                 */
                EXP_130XE:
                    if (!portb[4]) begin
                        bank_r   = {4'b0, portb[3:2]};
                        active_r = 1'b1;
                    end

                /* ----------------------------------------------------------
                 * Compyshop 192K: same CBE check; adds bit 6 for 8 banks.
                 * Bank bits: {portb[6], portb[3:2]}
                 */
                EXP_192K:
                    if (!portb[4]) begin
                        bank_r   = {3'b0, portb[6], portb[3:2]};
                        active_r = 1'b1;
                    end

                /* ----------------------------------------------------------
                 * RAMBO 256K: RAME (bit 4) = 0 → RAMBO active (all 16 banks).
                 *             RAME (bit 4) = 1 → Atari internal RAM responds.
                 * Bank bits: {portb[6:5], portb[3:2]} → 16 banks
                 */
                EXP_256K_RAMBO:
                    if (!portb[4]) begin
                        bank_r   = {2'b0, portb[6:5], portb[3:2]};
                        active_r = 1'b1;
                    end

                /* ----------------------------------------------------------
                 * 320K (standard RAMBO 320K): same bank formula as 256K_RAMBO.
                 * Bank bits: {portb[6:5], portb[3:2]} → 16 banks
                 */
                EXP_320K:
                    if (!portb[4]) begin
                        bank_r   = {2'b0, portb[6:5], portb[3:2]};
                        active_r = 1'b1;
                    end

                /* ----------------------------------------------------------
                 * Compyshop 320K: combined enable (bit5 AND bit4 must not
                 * both be set).  Bank bits: {portb[7:6], portb[3:2]}
                 */
                EXP_320K_COMPYSHOP:
                    if (portb[5:4] != 2'b11) begin
                        bank_r   = {2'b0, portb[7:6], portb[3:2]};
                        active_r = 1'b1;
                    end

                /* ----------------------------------------------------------
                 * 576K Mod: bit 4 = 0 enables; adds bits 1,5,6 for 32 banks.
                 * Bank bits: {portb[6:5], portb[3:1]} → 32 banks
                 */
                EXP_576K_MOD:
                    if (!portb[4]) begin
                        bank_r   = {1'b0, portb[6:5], portb[3:1]};
                        active_r = 1'b1;
                    end

                /* ----------------------------------------------------------
                 * Compyshop 576K: bit 4 = 0 enables; adds bits 1,6,7.
                 * Bank bits: {portb[7:6], portb[3:1]} → 32 banks
                 */
                EXP_576K_COMPYSHOP:
                    if (!portb[4]) begin
                        bank_r   = {1'b0, portb[7:6], portb[3:1]};
                        active_r = 1'b1;
                    end

                /* ----------------------------------------------------------
                 * 1088K Mod: bit 4 = 0 enables; uses bits 1-3 and 5-7.
                 * Bank bits: {portb[7:5], portb[3:1]} → 64 banks (1MB SRAM)
                 */
                EXP_1088K_MOD:
                    if (!portb[4]) begin
                        bank_r   = {portb[7:5], portb[3:1]};
                        active_r = 1'b1;
                    end

                default: begin
                    bank_r   = 6'd0;
                    active_r = 1'b0;
                end
            endcase
        end
    end

    assign bank   = bank_r;
    assign active = active_r;

endmodule
