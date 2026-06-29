/* portb_emu.v — Minimal PIA 6520 emulation for ATARI XL/XE PORTB bank-switching.
 *
 * Only the Port-B side of the 6520 is emulated (the STM32 firmware does the
 * same).  The relevant registers, using the Atari address mapping where A0 is
 * wired to RS1 and A1 to RS0 (i.e. reversed from the generic 6520 spec):
 *
 *   $D301 (addr[1:0]=01): PORTB output register  OR  DDRB
 *                         selected by pbctl[2] (CRB-2 / DDR):
 *                           pbctl[2]=1 → output register (PORTB)
 *                           pbctl[2]=0 → data-direction  (DDRB)
 *   $D303 (addr[1:0]=11): PBCTL (Port B Control Register)
 *
 * Both addresses mirror every 4 bytes throughout $D300–$D3FF.
 *
 * Reset / power-on values matching the STM32 firmware:
 *   PORTB = 0xFF  (all bits output, memory-map = Atari internal + OS ROM)
 *   PBCTL = 0x00  (DDR mode initially, until OS sets bit 2)
 *   DDRB  = 0x00
 *
 * Writes are latched on the FALLING edge of PHI2; the 6502 guarantees data
 * hold past the falling edge, so this is safe at 1.79 MHz.
 *
 * NOTE: PHI2 must be routed through a global clock buffer on the target FPGA
 * (e.g. SB_GB on iCE40, CLKDIVF/CLKBUF on ECP5) to avoid hold-time issues.
 */
`default_nettype none

module portb_emu (
    input  wire       phi2,       /* PHI2 bus clock (1.79 MHz)              */
    input  wire       pia_sel,    /* 1 when addr is in $D3xx                */
    input  wire [1:0] rs,         /* addr[1:0] register select              */
    input  wire [7:0] data_in,    /* data bus driven by the Atari (writes)  */
    input  wire       rw,         /* 1 = read, 0 = write                    */
    output reg  [7:0] portb,      /* PORTB output register                  */
    output reg  [7:0] pbctl,      /* PBCTL                                  */
    output wire [7:0] data_out,   /* data to drive onto Atari bus (reads)   */
    output wire       data_oe     /* 1 = drive data_out onto the Atari bus  */
);
    reg [7:0] ddrb;  /* data direction register (not visible externally)   */

    initial begin
        portb = 8'hFF;
        pbctl = 8'h00;
        ddrb  = 8'h00;
    end

    /* --- Write path: latch on PHI2 falling edge -------------------------*/
    always @(negedge phi2) begin
        if (!rw && pia_sel) begin
            case (rs)
                2'b01: begin   /* $D301 / mirrors */
                    if (pbctl[2]) portb <= data_in;
                    else          ddrb  <= data_in;
                end
                2'b11: begin   /* $D303 / mirrors */
                    pbctl <= data_in;
                end
                default: ;
            endcase
        end
    end

    /* --- Read path: purely combinational --------------------------------*/
    wire [7:0] d301_rd = pbctl[2] ? portb : ddrb;

    assign data_out = (rs == 2'b01) ? d301_rd : pbctl;

    /* Drive the Atari bus only for odd-addressed PIA registers during a
     * PHI2-high read cycle.  Both $D301 and $D303 have addr[0]=1. */
    assign data_oe  = phi2 && rw && pia_sel && rs[0];

endmodule
