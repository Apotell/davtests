/*
 Copyright 2020 Apotell

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

// clang-format off
// ---------------------------------------------------------------------------
// SystemVerilog source under test: tests/ariane/fpga/src/ariane-ethernet/framing_top.sv
// (copied verbatim; tabs expanded to spaces and trailing blanks removed, the
// file is already pure ASCII so no character conversion was needed)
// ---------------------------------------------------------------------------
// // See LICENSE for license details.
// `default_nettype none
//
// module framing_top
//   (
//   input wire          msoc_clk,
//   input wire [14:0]   core_lsu_addr,
//   input wire [63:0]   core_lsu_wdata,
//   input wire [7:0]    core_lsu_be,
//   input wire          ce_d,
//   input wire          we_d,
//   input wire          framing_sel,
//   output logic [63:0] framing_rdata,
//
//     // Internal 125 MHz clock
//   input wire          clk_int,
//   input wire          rst_int,
//   input wire          clk90_int,
//   input wire          clk_200_int,
//
//     /*
//      * Ethernet: 1000BASE-T RGMII
//      */
//   input wire          phy_rx_clk,
//   input wire [3:0]    phy_rxd,
//   input wire          phy_rx_ctl,
//   output wire         phy_tx_clk,
//   output wire [3:0]   phy_txd,
//   output wire         phy_tx_ctl,
//   output wire         phy_reset_n,
//   input wire          phy_int_n,
//   input wire          phy_pme_n,
//
//   input wire          phy_mdio_i,
//   output reg          phy_mdio_o,
//   output reg          phy_mdio_oe,
//   output wire         phy_mdc,
//
//   output reg          eth_irq
//    );
//
// // obsolete signals to be removedphy_
// //
//
//
// logic [14:0] core_lsu_addr_dly;
//
// logic tx_enable_i, mac_gmii_tx_en;
// logic [47:0] mac_address, rx_dest_mac;
// logic [10:0] tx_frame_addr, rx_length_axis[0:7], tx_packet_length;
// logic [12:0] axis_tx_frame_size;
// logic        ce_d_dly, avail;
// logic [63:0] framing_rdata_pkt, framing_wdata_pkt;
// logic [3:0] tx_enable_dly, firstbuf, nextbuf, lastbuf;
// logic [2:0] last;
//
// reg        byte_sync, sync, irq_en, tx_busy;
//
//    wire [7:0] m_enb = (we_d ? core_lsu_be : 8'hFF);
//    logic phy_mdclk, cooked, tx_enable_old, loopback, promiscuous;
//    logic [3:0] spare;
//    logic [10:0] rx_addr_axis;
//
//        /*
//         * AXI input
//         */
//         reg         tx_axis_tvalid;
//         reg         tx_axis_tvalid_dly;
//         reg         tx_axis_tlast;
//         wire [7:0]  tx_axis_tdata;
//         wire        tx_axis_tready;
//         wire        tx_axis_tuser = 1'b0;
//
//        /*
//         * AXI output
//         */
//        wire       rx_clk;
//        wire [7:0] rx_axis_tdata;
//        wire       rx_axis_tvalid;
//        wire       rx_axis_tlast;
//        wire       rx_axis_tuser;
//
//       /*
//         * AXIS Status
//         */
//          wire [31:0] tx_fcs_reg_rev, rx_fcs_reg_rev;
//
//    always @(posedge rx_clk)
//      if (rst_int == 1'b1)
//        begin
//           byte_sync <= 1'b0;
//        end
//      else
//        begin
//           if (rx_axis_tvalid && (byte_sync == 0) && (nextbuf != (firstbuf+lastbuf)&15))
//             begin
//                byte_sync <= 1'b1;
//             end
//           if (rx_axis_tlast && byte_sync)
//             begin
//                last <= 1'b1;
//             end
//           else if ((last > 0) && (last < 7))
//             begin
//                byte_sync <= 1'b0;
//                last <= last + 3'b1;
//             end
//           else
//             begin
//                last <= 3'b0;
//             end
//        end
//
//    wire  [1:0] rx_wr = rx_axis_tvalid << rx_addr_axis[2];
//    logic [15:0] douta;
//    assign tx_axis_tdata = douta >> {tx_frame_addr[2],3'b000};
//    assign phy_mdc = phy_mdclk;
//
//    dualmem_widen8 RAMB16_inst_rx (
//                                     .clka(rx_clk),                // Port A Clock
//                                     .clkb(msoc_clk),              // Port A Clock
//                                     .douta(),                     // Port A 8-bit Data Output
//                                     .addra({nextbuf[2:0],rx_addr_axis[10:3],rx_addr_axis[1:0]}),    // Port A 11-bit Address Input
//                                     .dina({rx_axis_tdata,rx_axis_tdata}), // Port A 8-bit Data Input
//                                     .ena(rx_axis_tvalid),         // Port A RAM Enable Input
//                                     .wea(rx_wr),                  // Port A Write Enable Input
//                                     .doutb(framing_rdata_pkt),    // Port B 32-bit Data Output
//                                     .addrb(core_lsu_addr[13:3]),  // Port B 9-bit Address Input
//                                     .dinb(core_lsu_wdata),        // Port B 32-bit Data Input
//                                     .enb(ce_d & framing_sel & core_lsu_addr[14]),
//                                                                   // Port B RAM Enable Input
//                                     .web(we_d ? {(|core_lsu_be[7:4]),(|core_lsu_be[3:0])} : 2'b0) // Port B Write Enable Input
//                                     );
//
//     dualmem_widen RAMB16_inst_tx (
//                                    .clka(~clk_int),             // Port A Clock
//                                    .clkb(msoc_clk),              // Port A Clock
//                                    .douta(douta),                // Port A 8-bit Data Output
//                                    .addra({1'b0,tx_frame_addr[10:3],tx_frame_addr[1:0]}),  // Port A 11-bit Address Input
//                                    .dina(16'b0),                 // Port A 8-bit Data Input
//                                    .ena(tx_axis_tvalid),         // Port A RAM Enable Input
//                                    .wea(2'b0),                  // Port A Write Enable Input
//                                    .doutb(framing_wdata_pkt),    // Port B 32-bit Data Output
//                                    .addrb(core_lsu_addr[11:3]),  // Port B 9-bit Address Input
//                                    .dinb(core_lsu_wdata), // Port B 32-bit Data Input
//                                    .enb(ce_d & framing_sel & (core_lsu_addr[14:12]==3'b001)),
//                                                                  // Port B RAM Enable Input
//                                    .web(we_d ? {(|core_lsu_be[7:4]),(|core_lsu_be[3:0])} : 2'b0) // Port B Write Enable Input
//                                    );
//
// always @(posedge msoc_clk)
//   if (rst_int)
//     begin
//     core_lsu_addr_dly <= 0;
//     mac_address <= 48'H230100890702;
//     tx_packet_length <= 0;
//     tx_enable_dly <= 0;
//     cooked <= 1'b0;
//     loopback <= 1'b0;
//     spare <= 4'b0;
//     promiscuous <= 1'b0;
//     phy_mdio_oe <= 1'b0;
//     phy_mdio_o <= 1'b0;
//     phy_mdclk <= 1'b0;
//     sync <= 1'b0;
//     firstbuf <= 4'b0;
//     lastbuf <= 4'b0;
//     nextbuf <= 4'b0;
//     eth_irq <= 1'b0;
//     irq_en <= 1'b0;
//     ce_d_dly <= 1'b0;
//     tx_busy <= 1'b0;
//     avail = 1'b0;
//     end
//   else
//     begin
//     core_lsu_addr_dly <= core_lsu_addr;
//     ce_d_dly <= ce_d;
//     avail = nextbuf != firstbuf;
//     eth_irq <= avail & irq_en; // make eth_irq go away immediately if irq_en is low
//     if (framing_sel&we_d&(&core_lsu_be[3:0])&(core_lsu_addr[14:11]==4'b0001))
//       case(core_lsu_addr[6:3])
//         0: mac_address[31:0] <= core_lsu_wdata;
//         1: {irq_en,promiscuous,spare,loopback,cooked,mac_address[47:32]} <= core_lsu_wdata;
//         2: begin tx_enable_dly <= 10; tx_packet_length <= core_lsu_wdata; end /* tx payload size */
//         3: begin tx_enable_dly <= 0; tx_packet_length <= 0; end
//         4: begin {phy_mdio_oe,phy_mdio_o,phy_mdclk} <= core_lsu_wdata; end
//         5: begin lastbuf <= core_lsu_wdata[3:0]; end
//         6: begin firstbuf <= core_lsu_wdata[3:0]; end
//         default:;
//       endcase
//        if ((last > 0) && ~sync)
//          begin
//          // check broadcast/multicast address
//              sync <= (rx_dest_mac[47:24]==24'h01005E) | (&rx_dest_mac) | (mac_address == rx_dest_mac) | promiscuous;
//          end
//        else if (!last)
//          begin
//             if (sync) nextbuf <= nextbuf + 1'b1;
//             sync <= 1'b0;
//          end
//        if (mac_gmii_tx_en && tx_enable_i)
//          begin
//             tx_enable_dly <= 0;
//          end
//        else if (1'b1 == |tx_enable_dly)
//          begin
//          tx_busy <= 1'b1;
//          tx_enable_dly <= tx_enable_dly + !(&tx_enable_dly);
//          end
//        else if (~mac_gmii_tx_en)
//          tx_busy <= 1'b0;
//     end
//
// always @(posedge clk_int)
//   if (rst_int)
//     begin
//        tx_enable_i <= 1'b0;
//     end
//   else
//     begin
//        if (mac_gmii_tx_en && tx_axis_tlast)
//          begin
//             tx_enable_i <= 1'b0;
//          end
//        else if (1'b1 == &tx_enable_dly)
//          tx_enable_i <= 1'b1;
//     end
//
//    always @* casez({ce_d_dly,core_lsu_addr_dly[14:3]})
//     13'b10001????0000 : framing_rdata = mac_address[31:0];
//     13'b10001????0001 : framing_rdata = {irq_en, promiscuous, spare, loopback, cooked, mac_address[47:32]};
//     13'b1000?????0010 : framing_rdata = {tx_busy, 4'b0, tx_frame_addr, 5'b0, tx_packet_length};
//     13'b10001????0011 : framing_rdata = tx_fcs_reg_rev;
//     13'b10001????0100 : framing_rdata = {phy_mdio_i,phy_mdio_oe,phy_mdio_o,phy_mdclk};
//     13'b10001????0101 : framing_rdata = rx_fcs_reg_rev;
//     13'b10001????0110 : framing_rdata = {eth_irq, avail, lastbuf, nextbuf, firstbuf};
//     13'b10001????1??? : framing_rdata = rx_length_axis[core_lsu_addr_dly[5:3]];
//     13'b10010???????? : framing_rdata = framing_wdata_pkt;
//     13'b11??????????? : framing_rdata = framing_rdata_pkt;
//     default: framing_rdata = 'h0;
//     endcase
//
//    parameter dly = 0;
//
//    wire [31:0]      tx_fcs_reg, rx_fcs_reg;
//    assign           tx_fcs_reg_rev = {tx_fcs_reg[0],tx_fcs_reg[1],tx_fcs_reg[2],tx_fcs_reg[3],
//                                           tx_fcs_reg[4],tx_fcs_reg[5],tx_fcs_reg[6],tx_fcs_reg[7],
//                                           tx_fcs_reg[8],tx_fcs_reg[9],tx_fcs_reg[10],tx_fcs_reg[11],
//                                           tx_fcs_reg[12],tx_fcs_reg[13],tx_fcs_reg[14],tx_fcs_reg[15],
//                                           tx_fcs_reg[16],tx_fcs_reg[17],tx_fcs_reg[18],tx_fcs_reg[19],
//                                           tx_fcs_reg[20],tx_fcs_reg[21],tx_fcs_reg[22],tx_fcs_reg[23],
//                                           tx_fcs_reg[24],tx_fcs_reg[25],tx_fcs_reg[26],tx_fcs_reg[27],
//                                           tx_fcs_reg[28],tx_fcs_reg[29],tx_fcs_reg[30],tx_fcs_reg[31]};
//    assign           rx_fcs_reg_rev = {rx_fcs_reg[0],rx_fcs_reg[1],rx_fcs_reg[2],rx_fcs_reg[3],
//                                           rx_fcs_reg[4],rx_fcs_reg[5],rx_fcs_reg[6],rx_fcs_reg[7],
//                                           rx_fcs_reg[8],rx_fcs_reg[9],rx_fcs_reg[10],rx_fcs_reg[11],
//                                           rx_fcs_reg[12],rx_fcs_reg[13],rx_fcs_reg[14],rx_fcs_reg[15],
//                                           rx_fcs_reg[16],rx_fcs_reg[17],rx_fcs_reg[18],rx_fcs_reg[19],
//                                           rx_fcs_reg[20],rx_fcs_reg[21],rx_fcs_reg[22],rx_fcs_reg[23],
//                                           rx_fcs_reg[24],rx_fcs_reg[25],rx_fcs_reg[26],rx_fcs_reg[27],
//                                           rx_fcs_reg[28],rx_fcs_reg[29],rx_fcs_reg[30],rx_fcs_reg[31]};
//
//    always @(posedge clk_int)
//      if (rst_int)
//        begin
//           tx_axis_tvalid <= 'b0;
//           tx_axis_tvalid_dly <= 'b0;
//           tx_frame_addr <= 'b0;
//           tx_axis_tlast <= 'b0;
//        end
//      else
//        begin
//           tx_enable_old <= tx_enable_i;
//           if (tx_enable_i & (tx_enable_old == 0))
//             begin
//                tx_frame_addr <= 'b0;
//             end
//           if (tx_axis_tready)
//             begin
//                tx_frame_addr <= tx_frame_addr + 1;
//                tx_axis_tlast <= (tx_frame_addr == tx_packet_length-2) & tx_axis_tvalid_dly;
//             end
//           tx_axis_tvalid <= tx_axis_tvalid_dly;
//           if (tx_enable_old)
//               tx_axis_tvalid_dly <= 1'b1;
//           else if (~tx_axis_tlast)
//               tx_axis_tvalid_dly <= 1'b0;
//       end
//
//    always @(posedge rx_clk)
//      if (rst_int)
//        begin
//           rx_addr_axis <= 'b0;
//           rx_dest_mac <= 'b0;
//        end
//      else
//        begin
//           if (rx_axis_tvalid)
//             begin
//             rx_addr_axis <= rx_addr_axis + 1;
//             if (rx_addr_axis < 6)
//               rx_dest_mac <= {rx_dest_mac[39:0],rx_axis_tdata};
//             end
//           if (rx_axis_tlast)
//             begin
//                 rx_length_axis[nextbuf[2:0]] <= rx_addr_axis + 1;
//                 rx_addr_axis <= 'b0;
//             end
//       end
//
// rgmii_soc rgmii_soc1
//   (
//    .rst_int(rst_int),
//    .clk_int(clk_int),
//    .clk90_int(clk90_int),
//    .clk_200_int(clk_200_int),
//    .rx_clk(rx_clk),
//    /*
//     * Ethernet: 1000BASE-T RGMII
//     */
//    .phy_rx_clk(phy_rx_clk),
//    .phy_rxd(phy_rxd),
//    .phy_rx_ctl(phy_rx_ctl),
//    .phy_tx_clk(phy_tx_clk),
//    .phy_txd(phy_txd),
//    .phy_tx_ctl(phy_tx_ctl),
//    .phy_reset_n(phy_reset_n),
//    .phy_int_n(phy_int_n),
//    .phy_pme_n(phy_pme_n),
//    .mac_gmii_tx_en(mac_gmii_tx_en),
//    .tx_axis_tdata(tx_axis_tdata),
//    .tx_axis_tvalid(tx_axis_tvalid),
//    .tx_axis_tready(tx_axis_tready),
//    .tx_axis_tlast(tx_axis_tlast),
//    .tx_axis_tuser(tx_axis_tuser),
//    .rx_axis_tdata(rx_axis_tdata),
//    .rx_axis_tvalid(rx_axis_tvalid),
//    .rx_axis_tlast(rx_axis_tlast),
//    .rx_axis_tuser(rx_axis_tuser),
//    .rx_fcs_reg(rx_fcs_reg),
//    .tx_fcs_reg(tx_fcs_reg)
// );
//
// // `define XILINX_ILA_1
// // `define XILINX_ILA_2
// // `define XILINX_ILA_3
//
// `ifdef XILINX_ILA_1
// xlnx_ila_1 eth_ila_clk_rx (
//         .clk(clk_int), // input wire clk
//         .probe0(rx_axis_tdata), // input wire [7:0]  probe4
//         .probe1(rx_axis_tvalid), // input wire [0:0]  probe5
//         .probe2(rx_axis_tlast), // input wire [0:0]  probe6
//         .probe3(rx_axis_tuser), // input wire [0:0]  probe7
//         .probe4(byte_sync),
//         .probe5(last),
//         .probe6(rx_addr_axis),
//         .probe7(rx_dest_mac),
//         .probe8(rx_length_axis[nextbuf[2:0]]),
//         .probe9(rx_clk)
// );
// `endif
//
// `ifdef XILINX_ILA_2
// xlnx_ila_2 eth_ila_clk_int (
//         .clk(clk_int), // input wire clk
//         .probe0(tx_axis_tdata), // input wire [7:0]  probe0
//         .probe1(tx_axis_tvalid), // input wire [0:0]  probe1
//         .probe2(tx_axis_tready), // input wire [0:0]  probe2
//         .probe3(tx_axis_tlast), // input wire [0:0]  probe3
//         .probe4(tx_enable_i),
//         .probe5(douta),
//         .probe6(tx_axis_tvalid_dly),
//         .probe7(tx_frame_addr),
//         .probe8(mac_gmii_tx_en),
//         .probe9(tx_enable_old)
// );
// `endif
//
// `ifdef XILINX_ILA_3
// xlnx_ila_3 eth_ila_clk_msoc (
//         .clk(msoc_clk), // input wire clk
//         .probe0(sync),
//         .probe1(avail),
//         .probe2(nextbuf),
//         .probe3(tx_enable_dly),
//         .probe4(tx_busy)
// );
// `endif
//
// endmodule // framing_top
// `default_nettype wire
// ---------------------------------------------------------------------------
// clang-format on

// IEEE 1800-2023 semantics exercised:
//   - `default_nettype none (Sec 22.8) is in effect for the whole of
//     framing_top (it is restored to wire after endmodule). Inside framing_top
//     every net must therefore be explicitly declared and NO implicit net may
//     be created (Sec 6.10); the source does declare every identifier before
//     use, so no implicit-net error is expected. Modules compiled after the
//     `default_nettype wire line (dualmem_widen8, rgmii_soc, ...) are back to
//     the wire default. The VPI property vpiDefNetType of a module reports the
//     default net type in effect for it (Sec 37.10 instance object diagram).
//   - ANSI ports (Sec 23.2.2.3): "input wire"/"output wire" ports are nets;
//     "output reg" and "output logic" ports have an explicit data type, so
//     they are VARIABLES (framing_rdata, phy_mdio_o, phy_mdio_oe, eth_irq).
//   - Body "logic"/"reg" declarations are variables (Sec 6.5, 6.11.2),
//     including the unpacked array rx_length_axis[0:7] of logic [10:0]
//     (Sec 7.4.2); "wire" declarations are nets. Three nets use a net
//     declaration assignment (Sec 10.3.1): m_enb = (we_d ? core_lsu_be :
//     8'hFF), tx_axis_tuser = 1'b0 and rx_wr = rx_axis_tvalid <<
//     rx_addr_axis[2] (shift, Sec 11.4.10). mac_gmii_tx_en is a logic
//     variable driven by an output port of rgmii_soc (legal, Sec 23.3.3.2).
//   - "parameter dly = 0;" in the body of a module WITHOUT a parameter port
//     list is an ordinary overridable parameter, not a localparam
//     (Sec 6.20.1).
//   - Continuous assigns (Sec 10.3.2): tx_axis_tdata = douta >>
//     {tx_frame_addr[2],3'b000}, phy_mdc = phy_mdclk, and the two 32-bit
//     bit-reversal concatenations tx_fcs_reg_rev / rx_fcs_reg_rev
//     (Sec 11.4.12).
//   - Six always procedures (Sec 9.2.2.1). Five use "@(posedge clk)" with an
//     if/else on the reset and nonblocking assignments (Sec 10.4.2); the
//     msoc_clk one also contains BLOCKING assignments "avail = ..."
//     (Sec 10.4.1) inside a clocked block, a "case" with 7 items + default
//     (Sec 12.5) and a concatenation as an assignment target. The byte_sync
//     block has the condition "nextbuf != (firstbuf+lastbuf)&15" which, by
//     operator precedence (Sec 11.3.2: equality binds tighter than bitwise
//     &), is "(nextbuf != (firstbuf+lastbuf)) & 15". One "always @*" block
//     (implicit event list, Sec 9.4.2.2) contains a casez (Sec 12.5.1) on a
//     13-bit concatenation with wildcard '?' patterns and a default.
//   - Instances (Sec 23.3.2.2): dualmem_widen8 RAMB16_inst_rx (empty .douta(),
//     concatenation / reduction-OR / conditional actuals), dualmem_widen
//     RAMB16_inst_tx (~clk_int, 16'b0, 2'b0 actuals), rgmii_soc rgmii_soc1.
//   - The three `ifdef XILINX_ILA_n blocks (Sec 22.6) are skipped because the
//     macros are never defined (the `define lines are commented out), so the
//     xlnx_ila_* instances do not exist at all.
//   - Elaboration (Sec 23.3): framing_top is the top. dualmem_widen8 and
//     dualmem_widen instantiate infer_dpram (RAMB16 not defined), rgmii_soc
//     instantiates IDELAYCTRL / IDELAYE2 and, via rgmii_core's TARGET
//     "XILINX", BUFIO / BUFR / IDDR / ODDR; none of these has a definition ->
//     errors. RAMB16_S9_S36 (inside the not-taken `ifdef RAMB16) and the
//     xlnx_ila_* modules must not be reported.
//
// What we have to check:
//   - default net type none for framing_top, wire for later modules; no
//     implicit nets and no implicit-net / failed-to-bind errors in framing_top
//   - the 26 ports: order, direction, width; nets vs variables
//   - every body variable / net with its kind and width, the unpacked array
//   - the three net declaration assignments
//   - parameter dly: present, not local, value 0
//   - the four continuous assigns
//   - each always block: event, reset structure, blocking vs nonblocking,
//     the precedence-sensitive condition, the case / casez statements
//   - the three instances and their notable connections; no ILA instances
//   - elaborated hierarchy and the required / forbidden "no module
//     definition" errors
//
// Checked:
//   - DefaultNettypeNone, NoImplicitNets, Ports, PortNetsAndVariables,
//     BodyVariables, UnpackedArrayVariable, BodyNets, NetDeclarationAssigns,
//     BodyParameterIsOverridable, ContinuousAssigns, ByteSyncAlways,
//     MsocClkAlways, MsocClkCaseStatement, TxEnableAlways, ReadMuxCasez,
//     TxFrameAlways, RxAddrAlways, RxDualmemInstance, TxDualmemInstance,
//     RgmiiSocInstance, IfdefIlaBlocksNotCompiled, ElaboratedHierarchy,
//     UndefinedModulesReported, UndefinedModulesNotReported
//
// Not checked:
//   - HLC 786 builds no elaborated hierarchy (Design::getTopModules() is empty).
//     For a module nobody instantiates, its getAllModules() entry is the module
//     with default parameters, i.e. the IEEE top-level instance, so the top-level
//     checks fall back to that entry. Child instances and generate scopes have no
//     such entry; those tests are skipped with the reason in the skip message.
//   - the run-time behaviour of the frame buffer / MDIO / IRQ logic -- needs
//     simulation
//   - the exact bit pattern stored for the casez wildcard literals (how '?'
//     is encoded in the constant value is not specified by IEEE VPI); only
//     constant kind and size are checked
//   - every individual statement of the 20-statement reset branch beyond the
//     blocking/nonblocking split -- they are all plain constant resets
//   - internals of dualmem_widen8 / dualmem_widen / rgmii_soc -- covered by
//     their own test files

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/case_item.h>
#include <hldb/case_stmt.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/if_else.h>
#include <hldb/if_stmt.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/named_argument.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/part_select.h>
#include <hldb/port.h>
#include <hldb/process_stmt.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
// HLC 786 stores Constant::getValue() as the bare digits of the literal in its
// own base (e.g. "0068746469772d6f" for 64'h00687464_69772d6f); the base is only
// in getConstType(). This returns the "<KIND>:<digits>" form the value parsers
// below expect. The text lives in a static pool so the view stays valid.
[[maybe_unused]] static std::string_view constantValueText(const hldb::Constant *constant) {
  static std::deque<std::string> s_pool;
  std::string_view raw = constant->getValue();
  if ((raw.size() >= 2) && (raw.front() == '"') && (raw.back() == '"')) raw = raw.substr(1, raw.size() - 2);
  if (raw.find(':') != std::string_view::npos) return s_pool.emplace_back(raw);
  std::string kind;
  switch (constant->getConstType()) {
    case vpiBinaryConst: kind = "BIN:"; break;
    case vpiOctConst: kind = "OCT:"; break;
    case vpiHexConst: kind = "HEX:"; break;
    case vpiStringConst: kind = "STRING:"; break;
    case vpiUIntConst: kind = "UINT:"; break;
    case vpiIntConst: kind = "INT:"; break;
    default: kind = "DEC:"; break;
  }
  return s_pool.emplace_back(kind + std::string(raw));
}
// Name of a reference without any trailing select: HLC 786 names a bit/part
// select "sig[2]"; the tests compare the selected signal's name "sig".
[[maybe_unused]] static std::string_view selectBaseName(std::string_view name) {
  const size_t bracket = name.find('[');
  return (bracket == std::string_view::npos) ? name : name.substr(0, bracket);
}
namespace {
// ---------------------------------------------------------------------------
// Small helpers shared by the tests below. They only navigate the object
// model; every expected value is written in the TEST_F bodies.
// ---------------------------------------------------------------------------

// Value of a Constant. The database may prefix the payload with a kind tag
// such as "UINT:" or "STRING:"; that tag is stripped so that tests compare the
// literal value only. Returns an empty view if 'any' is not a Constant.
[[maybe_unused]] std::string_view constValue(const hldb::Any *any) {
  const hldb::Constant *const constant = any_cast<hldb::Constant>(any);
  if (constant == nullptr) return {};
  const std::string_view value = constantValueText(constant);
  const size_t pos = value.find(':');
  return (pos == std::string_view::npos) ? value : value.substr(pos + 1);
}

// Checks the packed range of a logic typespec. msb < 0 means "scalar, no range".
[[maybe_unused]] void checkLogicRange(const hldb::RefTypespec *refTs, int32_t msb, int32_t lsb, std::string_view what) {
  ASSERT_NE(refTs, nullptr) << what << ": no typespec";
  const hldb::LogicTypespec *const ts = any_cast<hldb::LogicTypespec>(refTs->getActual());
  ASSERT_NE(ts, nullptr) << what << ": expected a logic typespec";
  const hldb::RangeCollection *const ranges = ts->getRanges();
  if (msb < 0) {
    EXPECT_TRUE((ranges == nullptr) || ranges->empty()) << what << ": expected a scalar";
    return;
  }
  ASSERT_NE(ranges, nullptr) << what << ": expected a packed range";
  ASSERT_EQ(ranges->size(), 1U) << what;
  const hldb::Range *const range = ranges->at(0);
  EXPECT_EQ(constValue(range->getLeftExpr()), std::to_string(msb)) << what;
  EXPECT_EQ(constValue(range->getRightExpr()), std::to_string(lsb)) << what;
}

struct PortSpec {
  std::string_view m_name;
  int32_t m_direction;
  int32_t m_msb;  // -1 for a scalar port
  int32_t m_lsb;
};

// Checks the port list (order, name, direction, packed width).
[[maybe_unused]] void checkPorts(const hldb::Module *module, const std::vector<PortSpec> &specs) {
  ASSERT_NE(module, nullptr);
  const hldb::PortCollection *const ports = module->getPorts();
  ASSERT_NE(ports, nullptr);
  ASSERT_EQ(ports->size(), specs.size());
  for (size_t i = 0; i < specs.size(); ++i) {
    const hldb::Port *const port = ports->at(i);
    EXPECT_EQ(selectBaseName(port->getName()), specs[i].m_name) << "port index " << i;
    EXPECT_EQ(port->getDirection(), specs[i].m_direction) << specs[i].m_name;
    checkLogicRange(port->getTypespec(), specs[i].m_msb, specs[i].m_lsb, specs[i].m_name);
  }
}

// 'name' must be an explicitly declared wire net (and not a variable).
[[maybe_unused]] void expectNet(const hldb::Module *module, std::string_view name, int32_t msb, int32_t lsb) {
  ASSERT_NE(module, nullptr);
  const hldb::Net *const net = hldb::findByName<hldb::Net>(name, module->getNets());
  ASSERT_NE(net, nullptr) << name << " must be a net";
  EXPECT_EQ(net->getNetType(), vpiWire) << name;
  EXPECT_FALSE(net->getImplicitDecl()) << name;
  EXPECT_EQ(hldb::findByName<hldb::Variable>(name, module->getVariables()), nullptr)
      << name << " must not be a variable";
  checkLogicRange(net->getTypespec(), msb, lsb, name);
}

// 'name' must be a variable (reg/logic) with the given packed range, not a net.
[[maybe_unused]] void expectVariable(const hldb::Module *module, std::string_view name, int32_t msb, int32_t lsb) {
  ASSERT_NE(module, nullptr);
  const hldb::Variable *const var = hldb::findByName<hldb::Variable>(name, module->getVariables());
  ASSERT_NE(var, nullptr) << name << " must be a variable";
  EXPECT_EQ(hldb::findByName<hldb::Net>(name, module->getNets()), nullptr) << name << " must not be a net";
  checkLogicRange(var->getTypespec(), msb, lsb, name);
}

// First procedural assignment found (depth first) below 'stmt'.
[[maybe_unused]] const hldb::Assignment *firstAssignment(const hldb::Any *stmt) {
  if (stmt == nullptr) return nullptr;
  if (const hldb::Assignment *const assign = any_cast<hldb::Assignment>(stmt)) return assign;
  if (const hldb::EventControl *const ec = any_cast<hldb::EventControl>(stmt)) return firstAssignment(ec->getStmt());
  if (const hldb::Begin *const begin = any_cast<hldb::Begin>(stmt)) {
    if (begin->getStmts() == nullptr) return nullptr;
    for (const hldb::Any *const s : *begin->getStmts()) {
      if (const hldb::Assignment *const found = firstAssignment(s)) return found;
    }
    return nullptr;
  }
  if (const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(stmt)) {
    if (const hldb::Assignment *const found = firstAssignment(ifElse->getStmt())) return found;
    return firstAssignment(ifElse->getElseStmt());
  }
  if (const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(stmt)) return firstAssignment(ifStmt->getStmt());
  if (const hldb::CaseStmt *const caseStmt = any_cast<hldb::CaseStmt>(stmt)) {
    if (caseStmt->getCaseItems() == nullptr) return nullptr;
    for (const hldb::CaseItem *const item : *caseStmt->getCaseItems()) {
      if (const hldb::Assignment *const found = firstAssignment(item->getStmt())) return found;
    }
  }
  return nullptr;
}

// The 'always' process whose first assignment writes 'lhsName'.
[[maybe_unused]] const hldb::Always *findAlways(const hldb::Module *module, std::string_view lhsName) {
  if ((module == nullptr) || (module->getProcesses() == nullptr)) return nullptr;
  for (const hldb::Process *const process : *module->getProcesses()) {
    if (const hldb::Always *const always = any_cast<hldb::Always>(process)) {
      const hldb::Assignment *const assign = firstAssignment(always->getStmt());
      if ((assign != nullptr) && (assign->getLhs() != nullptr) &&
          (selectBaseName(assign->getLhs()->getName()) == lhsName)) {
        return always;
      }
    }
  }
  return nullptr;
}

// i-th statement of a begin-end block (nullptr if absent).
[[maybe_unused]] const hldb::Any *stmtAt(const hldb::Any *block, size_t index) {
  const hldb::Begin *const begin = any_cast<hldb::Begin>(block);
  if ((begin == nullptr) || (begin->getStmts() == nullptr) || (index >= begin->getStmts()->size())) return nullptr;
  return begin->getStmts()->at(index);
}

[[maybe_unused]] size_t stmtCount(const hldb::Any *block) {
  const hldb::Begin *const begin = any_cast<hldb::Begin>(block);
  if ((begin == nullptr) || (begin->getStmts() == nullptr)) return 0;
  return begin->getStmts()->size();
}

// Checks that 'stmt' is an assignment to 'lhsName' with the given blocking-ness.
[[maybe_unused]] const hldb::Assignment *expectAssign(const hldb::Any *stmt, std::string_view lhsName, bool blocking) {
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(stmt);
  EXPECT_NE(assign, nullptr) << "expected an assignment to " << lhsName;
  if (assign == nullptr) return nullptr;
  EXPECT_NE(assign->getLhs(), nullptr);
  if (assign->getLhs() != nullptr) EXPECT_EQ(selectBaseName(assign->getLhs()->getName()), lhsName);
  EXPECT_EQ(assign->getBlocking(), blocking) << lhsName;
  return assign;
}

// Checks that 'expr' is "<edgeOp> <signal>" (e.g. posedge clk).
[[maybe_unused]] void expectEdge(const hldb::Any *expr, int32_t edgeOp, std::string_view signal) {
  const hldb::Operation *const op = any_cast<hldb::Operation>(expr);
  ASSERT_NE(op, nullptr) << "expected an edge operation on " << signal;
  EXPECT_EQ(op->getOpType(), edgeOp) << signal;
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 1U);
  EXPECT_EQ(selectBaseName(op->getOperands()->at(0)->getName()), signal);
}

// Operand 'index' of an operation (nullptr if absent).
[[maybe_unused]] const hldb::Any *operandAt(const hldb::Any *expr, size_t index) {
  const hldb::Operation *const op = any_cast<hldb::Operation>(expr);
  if ((op == nullptr) || (op->getOperands() == nullptr) || (index >= op->getOperands()->size())) return nullptr;
  return op->getOperands()->at(index);
}

[[maybe_unused]] size_t operandCount(const hldb::Any *expr) {
  const hldb::Operation *const op = any_cast<hldb::Operation>(expr);
  return ((op == nullptr) || (op->getOperands() == nullptr)) ? 0 : op->getOperands()->size();
}

[[maybe_unused]] int32_t opType(const hldb::Any *expr) {
  const hldb::Operation *const op = any_cast<hldb::Operation>(expr);
  return (op == nullptr) ? -1 : op->getOpType();
}

// Continuous assignment whose lhs is 'lhsName'.
[[maybe_unused]] const hldb::ContAssign *findContAssign(const hldb::Module *module, std::string_view lhsName) {
  if ((module == nullptr) || (module->getContAssigns() == nullptr)) return nullptr;
  for (const hldb::ContAssign *const ca : *module->getContAssigns()) {
    if ((ca->getLhs() != nullptr) && (selectBaseName(ca->getLhs()->getName()) == lhsName)) return ca;
  }
  return nullptr;
}

// Value of parameter 'name' in a (definition or elaborated) module scope.
[[maybe_unused]] std::string_view paramValue(const hldb::Module *module, std::string_view name) {
  if (module == nullptr) return {};
  const hldb::ParamAssign *const pa =
      hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(name, module->getParamAssigns());
  return (pa == nullptr) ? std::string_view{} : constValue(pa->getRhs());
}

// Elaborated child instance 'instName' of 'parent'.
[[maybe_unused]] const hldb::Module *childInstance(const hldb::Module *parent, std::string_view instName) {
  return (parent == nullptr) ? nullptr : hldb::findByName<hldb::Module>(instName, parent->getModules());
}

// True if an instance statement refers to module 'moduleName'.
[[maybe_unused]] bool refersTo(const hldb::RefInstance *inst, std::string_view moduleName) {
  if ((inst == nullptr) || (inst->getTypespec() == nullptr)) return false;
  const hldb::ModuleTypespec *const ts = any_cast<hldb::ModuleTypespec>(inst->getTypespec()->getActual());
  return (ts != nullptr) && ((selectBaseName(ts->getName()) == moduleName) || (ts->getDefName() == moduleName));
}

// Parameter override 'name' of an instance statement (nullptr if absent).
[[maybe_unused]] const hldb::ParamAssign *instOverride(const hldb::RefInstance *inst, std::string_view name) {
  if ((inst == nullptr) || (inst->getTypespec() == nullptr)) return nullptr;
  const hldb::ModuleTypespec *const ts = any_cast<hldb::ModuleTypespec>(inst->getTypespec()->getActual());
  if (ts == nullptr) return nullptr;
  return hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(name, ts->getParamAssigns());
}

// Number of parameter overrides written on an instance statement.
[[maybe_unused]] size_t instOverrideCount(const hldb::RefInstance *inst) {
  if ((inst == nullptr) || (inst->getTypespec() == nullptr)) return 0;
  const hldb::ModuleTypespec *const ts = any_cast<hldb::ModuleTypespec>(inst->getTypespec()->getActual());
  return ((ts == nullptr) || (ts->getParamAssigns() == nullptr)) ? 0 : ts->getParamAssigns()->size();
}

// Named port connection '.formal(...)' of an instance statement (nullptr if absent).
[[maybe_unused]] const hldb::NamedArgument *instConn(const hldb::RefInstance *inst, std::string_view formal) {
  if (inst == nullptr) return nullptr;
  return hldb::findByName<hldb::NamedArgument, hldb::NamedArgument>(formal, inst->getArguments());
}

// Name of the expression connected to '.formal(...)' ("" if unconnected or absent).
[[maybe_unused]] std::string_view connName(const hldb::RefInstance *inst, std::string_view formal) {
  const hldb::NamedArgument *const na = instConn(inst, formal);
  return ((na == nullptr) || (na->getHighConn() == nullptr)) ? std::string_view{} : na->getHighConn()->getName();
}
}  // namespace

class FramingTopTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-wd",
                       "../../../tests/ariane/fpga/src/ariane-ethernet",
                       "-d",
                       "ast",
                       "-d",
                       "db",
                       "-nobuiltin",
                       "framing_top.sv",
                       "dualmem_widen8.sv",
                       "dualmem_widen.sv",
                       "rgmii_soc.sv",
                       "rgmii_core.sv",
                       "eth_mac_1g_rgmii_fifo.sv",
                       "eth_mac_1g_rgmii.sv",
                       "rgmii_phy_if.sv",
                       "ssio_ddr_in.sv",
                       "iddr.sv",
                       "oddr.sv",
                       "eth_mac_1g.sv",
                       "axis_gmii_rx.sv",
                       "axis_gmii_tx.sv",
                       "rgmii_lfsr.sv"});
  }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::Module *definition(std::string_view name = "framing_top") const {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }
  const hldb::Module *top() const {
    return (hldb::findByName<hldb::Module>("framing_top", m_design->getTopModules()) != nullptr
                ? hldb::findByName<hldb::Module>("framing_top", m_design->getTopModules())
                : hldb::findByDefName<hldb::Module>("framing_top", m_design->getAllModules()));
  }
  const hldb::RefInstance *refInstance(std::string_view name) const {
    const hldb::Module *const module = definition();
    return (module == nullptr) ? nullptr : hldb::findByName<hldb::RefInstance>(name, module->getRefInstances());
  }
  // Last operand of an operation. Used for left-associative chains such as
  // "a && b && c" or "a & b & c" so that the check does not depend on whether
  // the chain is stored nested or flattened.
  static const hldb::Any *lastOperand(const hldb::Any *expr) {
    const size_t count = operandCount(expr);
    return (count == 0) ? nullptr : operandAt(expr, count - 1);
  }
  // The posedge-clocked body: checks the event and returns the statement.
  static const hldb::Any *clockedBody(const hldb::Always *always, std::string_view clock) {
    const hldb::EventControl *const ec =
        (always == nullptr) ? nullptr : any_cast<hldb::EventControl>(always->getStmt());
    EXPECT_NE(ec, nullptr) << "always @(posedge " << clock << ")";
    if (ec == nullptr) return nullptr;
    EXPECT_FALSE(ec->getIsImplicitControl());
    const hldb::Operation *const edge = any_cast<hldb::Operation>(ec->getCondition());
    EXPECT_NE(edge, nullptr);
    if (edge != nullptr) {
      EXPECT_EQ(edge->getOpType(), vpiPosedgeOp);
      EXPECT_EQ(operandCount(edge), 1U);
      if (operandAt(edge, 0) != nullptr) EXPECT_EQ(selectBaseName(operandAt(edge, 0)->getName()), clock);
    }
    return ec->getStmt();
  }
};

// Sec 22.8, 37.10 (vpiDefNetType): `default_nettype none applies to
// framing_top; the modules compiled after "`default_nettype wire" are back to
// wire.
TEST_F(FramingTopTest, DefaultNettypeNone) {
  const hldb::Module *const module = definition();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getDefNetType(), vpiNone);
  const hldb::Module *const elab = top();
  ASSERT_NE(elab, nullptr);
  EXPECT_EQ(elab->getDefNetType(), vpiNone);
  for (std::string_view later : {"dualmem_widen8", "dualmem_widen", "rgmii_soc", "rgmii_core"}) {
    const hldb::Module *const other = definition(later);
    ASSERT_NE(other, nullptr) << later;
    EXPECT_EQ(other->getDefNetType(), vpiWire) << later;
  }
}

// Sec 6.10, 22.8: every identifier in framing_top is explicitly declared, so
// no implicit net exists (22 port nets + 14 body nets) and no implicit-net or
// binding error is reported for identifiers declared late in the body.
TEST_F(FramingTopTest, NoImplicitNets) {
  const hldb::Module *const module = definition();
  ASSERT_NE(module, nullptr);
  ASSERT_NE(module->getNets(), nullptr);
  EXPECT_EQ(module->getNets()->size(), 36U);
  for (const hldb::Net *const net : *module->getNets()) {
    EXPECT_FALSE(net->getImplicitDecl()) << net->getName();
  }
  for (std::string_view name : {"douta", "rx_wr", "tx_fcs_reg", "rx_fcs_reg", "m_enb", "tx_axis_tuser",
                                "mac_gmii_tx_en", "rx_clk", "tx_fcs_reg_rev", "rx_fcs_reg_rev"}) {
    EXPECT_EQ(findError(ErrorDefinition::ELAB_ILLEGAL_IMPLICIT_NET, name), nullptr) << name;
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr) << name;
  }
}

// Sec 23.2.2: ANSI port list, order / direction / packed width.
TEST_F(FramingTopTest, Ports) {
  const std::vector<PortSpec> specs = {
      {"msoc_clk", vpiInput, -1, 0},     {"core_lsu_addr", vpiInput, 14, 0},  {"core_lsu_wdata", vpiInput, 63, 0},
      {"core_lsu_be", vpiInput, 7, 0},   {"ce_d", vpiInput, -1, 0},           {"we_d", vpiInput, -1, 0},
      {"framing_sel", vpiInput, -1, 0},  {"framing_rdata", vpiOutput, 63, 0}, {"clk_int", vpiInput, -1, 0},
      {"rst_int", vpiInput, -1, 0},      {"clk90_int", vpiInput, -1, 0},      {"clk_200_int", vpiInput, -1, 0},
      {"phy_rx_clk", vpiInput, -1, 0},   {"phy_rxd", vpiInput, 3, 0},         {"phy_rx_ctl", vpiInput, -1, 0},
      {"phy_tx_clk", vpiOutput, -1, 0},  {"phy_txd", vpiOutput, 3, 0},        {"phy_tx_ctl", vpiOutput, -1, 0},
      {"phy_reset_n", vpiOutput, -1, 0}, {"phy_int_n", vpiInput, -1, 0},      {"phy_pme_n", vpiInput, -1, 0},
      {"phy_mdio_i", vpiInput, -1, 0},   {"phy_mdio_o", vpiOutput, -1, 0},    {"phy_mdio_oe", vpiOutput, -1, 0},
      {"phy_mdc", vpiOutput, -1, 0},     {"eth_irq", vpiOutput, -1, 0},
  };
  checkPorts(definition(), specs);
}

// Sec 23.2.2.3: "wire" ports are nets; "output reg" / "output logic" ports
// have an explicit data type and are variables.
TEST_F(FramingTopTest, PortNetsAndVariables) {
  const hldb::Module *const module = definition();
  ASSERT_NE(module, nullptr);
  for (std::string_view name :
       {"msoc_clk", "ce_d", "we_d", "framing_sel", "clk_int", "rst_int", "clk90_int", "clk_200_int", "phy_rx_clk",
        "phy_rx_ctl", "phy_tx_clk", "phy_tx_ctl", "phy_reset_n", "phy_int_n", "phy_pme_n", "phy_mdio_i", "phy_mdc"}) {
    expectNet(module, name, -1, 0);
  }
  expectNet(module, "core_lsu_addr", 14, 0);
  expectNet(module, "core_lsu_wdata", 63, 0);
  expectNet(module, "core_lsu_be", 7, 0);
  expectNet(module, "phy_rxd", 3, 0);
  expectNet(module, "phy_txd", 3, 0);

  expectVariable(module, "framing_rdata", 63, 0);
  expectVariable(module, "phy_mdio_o", -1, 0);
  expectVariable(module, "phy_mdio_oe", -1, 0);
  expectVariable(module, "eth_irq", -1, 0);
}

// Sec 6.5, 6.11.2: body logic / reg declarations are variables.
TEST_F(FramingTopTest, BodyVariables) {
  const hldb::Module *const module = definition();
  ASSERT_NE(module, nullptr);
  struct Var {
    std::string_view m_name;
    int32_t m_msb;
    int32_t m_lsb;
  };
  for (const Var &v : {Var{"core_lsu_addr_dly", 14, 0},
                       Var{"tx_enable_i", -1, 0},
                       Var{"mac_gmii_tx_en", -1, 0},
                       Var{"mac_address", 47, 0},
                       Var{"rx_dest_mac", 47, 0},
                       Var{"tx_frame_addr", 10, 0},
                       Var{"tx_packet_length", 10, 0},
                       Var{"axis_tx_frame_size", 12, 0},
                       Var{"ce_d_dly", -1, 0},
                       Var{"avail", -1, 0},
                       Var{"framing_rdata_pkt", 63, 0},
                       Var{"framing_wdata_pkt", 63, 0},
                       Var{"tx_enable_dly", 3, 0},
                       Var{"firstbuf", 3, 0},
                       Var{"nextbuf", 3, 0},
                       Var{"lastbuf", 3, 0},
                       Var{"last", 2, 0},
                       Var{"byte_sync", -1, 0},
                       Var{"sync", -1, 0},
                       Var{"irq_en", -1, 0},
                       Var{"tx_busy", -1, 0},
                       Var{"phy_mdclk", -1, 0},
                       Var{"cooked", -1, 0},
                       Var{"tx_enable_old", -1, 0},
                       Var{"loopback", -1, 0},
                       Var{"promiscuous", -1, 0},
                       Var{"spare", 3, 0},
                       Var{"rx_addr_axis", 10, 0},
                       Var{"tx_axis_tvalid", -1, 0},
                       Var{"tx_axis_tvalid_dly", -1, 0},
                       Var{"tx_axis_tlast", -1, 0},
                       Var{"douta", 15, 0}}) {
    expectVariable(module, v.m_name, v.m_msb, v.m_lsb);
  }
}

// Sec 7.4.2: logic [10:0] rx_length_axis[0:7] is an unpacked array variable
// of 8 elements of logic [10:0].
TEST_F(FramingTopTest, UnpackedArrayVariable) {
  const hldb::Module *const module = definition();
  ASSERT_NE(module, nullptr);
  const hldb::Variable *const var = hldb::findByName<hldb::Variable>("rx_length_axis", module->getVariables());
  ASSERT_NE(var, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("rx_length_axis", module->getNets()), nullptr);
  ASSERT_NE(var->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const arr = any_cast<hldb::ArrayTypespec>(var->getTypespec()->getActual());
  ASSERT_NE(arr, nullptr);
  EXPECT_FALSE(arr->getPacked());
  EXPECT_EQ(arr->getArrayType(), vpiStaticArray);
  ASSERT_NE(arr->getRange(), nullptr);
  EXPECT_EQ(constValue(arr->getRange()->getLeftExpr()), "0");
  EXPECT_EQ(constValue(arr->getRange()->getRightExpr()), "7");
  checkLogicRange(arr->getElemTypespec(), 10, 0, "rx_length_axis element");
}

// Sec 6.7: explicitly declared body nets.
TEST_F(FramingTopTest, BodyNets) {
  const hldb::Module *const module = definition();
  ASSERT_NE(module, nullptr);
  expectNet(module, "m_enb", 7, 0);
  expectNet(module, "tx_axis_tdata", 7, 0);
  expectNet(module, "tx_axis_tready", -1, 0);
  expectNet(module, "tx_axis_tuser", -1, 0);
  expectNet(module, "rx_clk", -1, 0);
  expectNet(module, "rx_axis_tdata", 7, 0);
  expectNet(module, "rx_axis_tvalid", -1, 0);
  expectNet(module, "rx_axis_tlast", -1, 0);
  expectNet(module, "rx_axis_tuser", -1, 0);
  expectNet(module, "tx_fcs_reg_rev", 31, 0);
  expectNet(module, "rx_fcs_reg_rev", 31, 0);
  expectNet(module, "rx_wr", 1, 0);
  expectNet(module, "tx_fcs_reg", 31, 0);
  expectNet(module, "rx_fcs_reg", 31, 0);
}

// Sec 10.3.1: net declaration assignments m_enb, tx_axis_tuser, rx_wr.
TEST_F(FramingTopTest, NetDeclarationAssigns) {
  GTEST_SKIP() << "HLC 786 does not model the net declaration assignment 'wire x = expr;' as a continuous assignment. "
                  "Per IEEE 1800-2023 Sec 10.3.1 a net declaration assignment is a continuous assignment. Fix pending.";
  const hldb::Module *const module = definition();
  ASSERT_NE(module, nullptr);
  for (std::string_view name : {"m_enb", "tx_axis_tuser", "rx_wr"}) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(name, module->getNets());
    ASSERT_NE(net, nullptr) << name;
    EXPECT_TRUE(net->getNetDeclAssign()) << name;
    const hldb::ContAssign *const ca = findContAssign(module, name);
    ASSERT_NE(ca, nullptr) << name;
    EXPECT_TRUE(ca->getNetDeclAssign()) << name;
  }

  // wire [7:0] m_enb = (we_d ? core_lsu_be : 8'hFF);
  const hldb::ContAssign *const enb = findContAssign(module, "m_enb");
  ASSERT_NE(enb, nullptr);
  EXPECT_EQ(opType(enb->getRhs()), vpiConditionOp);
  ASSERT_EQ(operandCount(enb->getRhs()), 3U);
  EXPECT_EQ(selectBaseName(operandAt(enb->getRhs(), 0)->getName()), "we_d");
  EXPECT_EQ(selectBaseName(operandAt(enb->getRhs(), 1)->getName()), "core_lsu_be");
  const hldb::Constant *const ff = any_cast<hldb::Constant>(operandAt(enb->getRhs(), 2));
  ASSERT_NE(ff, nullptr);
  EXPECT_EQ(ff->getConstType(), vpiHexConst);
  EXPECT_EQ(ff->getSize(), 8);

  // wire tx_axis_tuser = 1'b0;
  const hldb::ContAssign *const tuser = findContAssign(module, "tx_axis_tuser");
  ASSERT_NE(tuser, nullptr);
  const hldb::Constant *const zero = any_cast<hldb::Constant>(tuser->getRhs());
  ASSERT_NE(zero, nullptr);
  EXPECT_EQ(zero->getSize(), 1);
  EXPECT_EQ(constValue(zero), "0");

  // wire [1:0] rx_wr = rx_axis_tvalid << rx_addr_axis[2];
  const hldb::ContAssign *const wr = findContAssign(module, "rx_wr");
  ASSERT_NE(wr, nullptr);
  EXPECT_EQ(opType(wr->getRhs()), vpiLShiftOp);
  ASSERT_EQ(operandCount(wr->getRhs()), 2U);
  EXPECT_EQ(selectBaseName(operandAt(wr->getRhs(), 0)->getName()), "rx_axis_tvalid");
  const hldb::BitSelect *const amount = any_cast<hldb::BitSelect>(operandAt(wr->getRhs(), 1));
  ASSERT_NE(amount, nullptr);
  EXPECT_EQ(selectBaseName(amount->getName()), "rx_addr_axis");
  EXPECT_EQ(constValue(amount->getIndex()), "2");
}

// Sec 6.20.1: "parameter dly = 0;" in the body of a module with no parameter
// port list is an ordinary (overridable) parameter, not a localparam.
TEST_F(FramingTopTest, BodyParameterIsOverridable) {
  const hldb::Module *const module = definition();
  ASSERT_NE(module, nullptr);
  const hldb::Parameter *const dly = hldb::findByName<hldb::Parameter>("dly", module->getParameters());
  ASSERT_NE(dly, nullptr);
  EXPECT_FALSE(dly->getLocalParam());
  EXPECT_EQ(paramValue(module, "dly"), "0");
  EXPECT_EQ(paramValue(top(), "dly"), "0");
}

// Sec 10.3.2, 11.4.10, 11.4.12: the four plain continuous assignments.
TEST_F(FramingTopTest, ContinuousAssigns) {
  const hldb::Module *const module = definition();
  ASSERT_NE(module, nullptr);

  // assign tx_axis_tdata = douta >> {tx_frame_addr[2],3'b000};
  const hldb::ContAssign *const tdata = findContAssign(module, "tx_axis_tdata");
  ASSERT_NE(tdata, nullptr);
  EXPECT_FALSE(tdata->getNetDeclAssign());
  EXPECT_EQ(opType(tdata->getRhs()), vpiRShiftOp);
  ASSERT_EQ(operandCount(tdata->getRhs()), 2U);
  EXPECT_EQ(selectBaseName(operandAt(tdata->getRhs(), 0)->getName()), "douta");
  const hldb::Any *const shamt = operandAt(tdata->getRhs(), 1);
  EXPECT_EQ(opType(shamt), vpiConcatOp);
  ASSERT_EQ(operandCount(shamt), 2U);
  const hldb::BitSelect *const bit2 = any_cast<hldb::BitSelect>(operandAt(shamt, 0));
  ASSERT_NE(bit2, nullptr);
  EXPECT_EQ(selectBaseName(bit2->getName()), "tx_frame_addr");
  EXPECT_EQ(constValue(bit2->getIndex()), "2");
  const hldb::Constant *const zeros = any_cast<hldb::Constant>(operandAt(shamt, 1));
  ASSERT_NE(zeros, nullptr);
  EXPECT_EQ(zeros->getSize(), 3);

  // assign phy_mdc = phy_mdclk;
  const hldb::ContAssign *const mdc = findContAssign(module, "phy_mdc");
  ASSERT_NE(mdc, nullptr);
  ASSERT_NE(mdc->getRhs(), nullptr);
  EXPECT_EQ(selectBaseName(mdc->getRhs()->getName()), "phy_mdclk");

  // Bit reversal: operand i of the concatenation is <src>[i].
  struct Rev {
    std::string_view m_lhs;
    std::string_view m_src;
  };
  for (const Rev &r : {Rev{"tx_fcs_reg_rev", "tx_fcs_reg"}, Rev{"rx_fcs_reg_rev", "rx_fcs_reg"}}) {
    const hldb::ContAssign *const ca = findContAssign(module, r.m_lhs);
    ASSERT_NE(ca, nullptr) << r.m_lhs;
    EXPECT_FALSE(ca->getNetDeclAssign()) << r.m_lhs;
    EXPECT_EQ(opType(ca->getRhs()), vpiConcatOp) << r.m_lhs;
    ASSERT_EQ(operandCount(ca->getRhs()), 32U) << r.m_lhs;
    for (size_t i = 0; i < 32; ++i) {
      const hldb::BitSelect *const bit = any_cast<hldb::BitSelect>(operandAt(ca->getRhs(), i));
      ASSERT_NE(bit, nullptr) << r.m_lhs << " operand " << i;
      EXPECT_EQ(selectBaseName(bit->getName()), r.m_src) << r.m_lhs << " operand " << i;
      EXPECT_EQ(constValue(bit->getIndex()), std::to_string(i)) << r.m_lhs << " operand " << i;
    }
  }
}

// always @(posedge rx_clk) if (rst_int == 1'b1) ... else ...; the first
// condition "nextbuf != (firstbuf+lastbuf)&15" parses as
// "(nextbuf != (firstbuf+lastbuf)) & 15" (Sec 11.3.2).
TEST_F(FramingTopTest, ByteSyncAlways) {
  const hldb::Always *const always = findAlways(definition(), "byte_sync");
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);
  const hldb::IfElse *const rstIf = any_cast<hldb::IfElse>(clockedBody(always, "rx_clk"));
  ASSERT_NE(rstIf, nullptr);
  EXPECT_EQ(opType(rstIf->getCondition()), vpiEqOp);
  ASSERT_EQ(operandCount(rstIf->getCondition()), 2U);
  EXPECT_EQ(selectBaseName(operandAt(rstIf->getCondition(), 0)->getName()), "rst_int");
  EXPECT_EQ(constValue(operandAt(rstIf->getCondition(), 1)), "1");
  ASSERT_EQ(stmtCount(rstIf->getStmt()), 1U);
  expectAssign(stmtAt(rstIf->getStmt(), 0), "byte_sync", false);

  const hldb::Any *const elseBlk = rstIf->getElseStmt();
  ASSERT_EQ(stmtCount(elseBlk), 2U);
  const hldb::IfStmt *const startIf = any_cast<hldb::IfStmt>(stmtAt(elseBlk, 0));
  ASSERT_NE(startIf, nullptr);
  EXPECT_EQ(opType(startIf->getCondition()), vpiLogAndOp);
  const hldb::Any *const third = lastOperand(startIf->getCondition());
  EXPECT_EQ(opType(third), vpiBitAndOp) << "'!=' binds tighter than '&'";
  ASSERT_EQ(operandCount(third), 2U);
  const hldb::Any *const neq = operandAt(third, 0);
  EXPECT_EQ(opType(neq), vpiNeqOp);
  ASSERT_EQ(operandCount(neq), 2U);
  EXPECT_EQ(selectBaseName(operandAt(neq, 0)->getName()), "nextbuf");
  EXPECT_EQ(opType(operandAt(neq, 1)), vpiAddOp);
  EXPECT_EQ(constValue(operandAt(third, 1)), "15");
  const hldb::Assignment *const set = firstAssignment(startIf->getStmt());
  ASSERT_NE(set, nullptr);
  EXPECT_EQ(selectBaseName(set->getLhs()->getName()), "byte_sync");
  EXPECT_FALSE(set->getBlocking());

  // if (rx_axis_tlast && byte_sync) last <= 1 else if ((last>0)&&(last<7)) ... else last <= 0
  const hldb::IfElse *const lastIf = any_cast<hldb::IfElse>(stmtAt(elseBlk, 1));
  ASSERT_NE(lastIf, nullptr);
  EXPECT_EQ(opType(lastIf->getCondition()), vpiLogAndOp);
  const hldb::Assignment *const setLast = firstAssignment(lastIf->getStmt());
  ASSERT_NE(setLast, nullptr);
  EXPECT_EQ(selectBaseName(setLast->getLhs()->getName()), "last");
  const hldb::IfElse *const countIf = any_cast<hldb::IfElse>(lastIf->getElseStmt());
  ASSERT_NE(countIf, nullptr);
  EXPECT_EQ(opType(countIf->getCondition()), vpiLogAndOp);
  ASSERT_EQ(operandCount(countIf->getCondition()), 2U);
  EXPECT_EQ(opType(operandAt(countIf->getCondition(), 0)), vpiGtOp);
  EXPECT_EQ(opType(operandAt(countIf->getCondition(), 1)), vpiLtOp);
  ASSERT_EQ(stmtCount(countIf->getStmt()), 2U);
  expectAssign(stmtAt(countIf->getStmt(), 0), "byte_sync", false);
  const hldb::Assignment *const incLast = expectAssign(stmtAt(countIf->getStmt(), 1), "last", false);
  ASSERT_NE(incLast, nullptr);
  EXPECT_EQ(opType(incLast->getRhs()), vpiAddOp);
  const hldb::Assignment *const clrLast = firstAssignment(countIf->getElseStmt());
  ASSERT_NE(clrLast, nullptr);
  EXPECT_EQ(selectBaseName(clrLast->getLhs()->getName()), "last");
}

// always @(posedge msoc_clk): 20 reset statements (19 nonblocking, the last
// one "avail = 1'b0" blocking, Sec 10.4.1) and a 7-statement else branch
// that again uses a blocking "avail = nextbuf != firstbuf".
TEST_F(FramingTopTest, MsocClkAlways) {
  const hldb::Always *const always = findAlways(definition(), "core_lsu_addr_dly");
  ASSERT_NE(always, nullptr);
  const hldb::IfElse *const rstIf = any_cast<hldb::IfElse>(clockedBody(always, "msoc_clk"));
  ASSERT_NE(rstIf, nullptr);
  ASSERT_NE(rstIf->getCondition(), nullptr);
  EXPECT_EQ(selectBaseName(rstIf->getCondition()->getName()), "rst_int");

  const std::vector<std::string_view> resetOrder = {
      "core_lsu_addr_dly", "mac_address", "tx_packet_length", "tx_enable_dly", "cooked",  "loopback", "spare",
      "promiscuous",       "phy_mdio_oe", "phy_mdio_o",       "phy_mdclk",     "sync",    "firstbuf", "lastbuf",
      "nextbuf",           "eth_irq",     "irq_en",           "ce_d_dly",      "tx_busy", "avail"};
  ASSERT_EQ(stmtCount(rstIf->getStmt()), resetOrder.size());
  for (size_t i = 0; i < resetOrder.size(); ++i) {
    expectAssign(stmtAt(rstIf->getStmt(), i), resetOrder[i], resetOrder[i] == "avail");
  }
  const hldb::Assignment *const mac = any_cast<hldb::Assignment>(stmtAt(rstIf->getStmt(), 1));
  ASSERT_NE(mac, nullptr);
  const hldb::Constant *const macValue = any_cast<hldb::Constant>(mac->getRhs());
  ASSERT_NE(macValue, nullptr);
  EXPECT_EQ(macValue->getConstType(), vpiHexConst);
  EXPECT_EQ(macValue->getSize(), 48);

  const hldb::Any *const elseBlk = rstIf->getElseStmt();
  ASSERT_EQ(stmtCount(elseBlk), 7U);
  expectAssign(stmtAt(elseBlk, 0), "core_lsu_addr_dly", false);
  expectAssign(stmtAt(elseBlk, 1), "ce_d_dly", false);
  const hldb::Assignment *const avail = expectAssign(stmtAt(elseBlk, 2), "avail", true);
  ASSERT_NE(avail, nullptr);
  EXPECT_EQ(opType(avail->getRhs()), vpiNeqOp);
  ASSERT_EQ(operandCount(avail->getRhs()), 2U);
  EXPECT_EQ(selectBaseName(operandAt(avail->getRhs(), 0)->getName()), "nextbuf");
  EXPECT_EQ(selectBaseName(operandAt(avail->getRhs(), 1)->getName()), "firstbuf");
  const hldb::Assignment *const irq = expectAssign(stmtAt(elseBlk, 3), "eth_irq", false);
  ASSERT_NE(irq, nullptr);
  EXPECT_EQ(opType(irq->getRhs()), vpiBitAndOp);
  ASSERT_EQ(operandCount(irq->getRhs()), 2U);
  EXPECT_EQ(selectBaseName(operandAt(irq->getRhs(), 0)->getName()), "avail");
  EXPECT_EQ(selectBaseName(operandAt(irq->getRhs(), 1)->getName()), "irq_en");

  // if ((last > 0) && ~sync) ... else if (!last) ...
  const hldb::IfElse *const syncIf = any_cast<hldb::IfElse>(stmtAt(elseBlk, 5));
  ASSERT_NE(syncIf, nullptr);
  EXPECT_EQ(opType(syncIf->getCondition()), vpiLogAndOp);
  ASSERT_EQ(operandCount(syncIf->getCondition()), 2U);
  EXPECT_EQ(opType(operandAt(syncIf->getCondition(), 0)), vpiGtOp);
  EXPECT_EQ(opType(operandAt(syncIf->getCondition(), 1)), vpiBitNegOp);
  const hldb::IfStmt *const notLast = any_cast<hldb::IfStmt>(syncIf->getElseStmt());
  ASSERT_NE(notLast, nullptr);
  EXPECT_EQ(opType(notLast->getCondition()), vpiNotOp);

  // if (mac_gmii_tx_en && tx_enable_i) ... else if (1'b1 == |tx_enable_dly) ... else if (~mac_gmii_tx_en) ...
  const hldb::IfElse *const txIf = any_cast<hldb::IfElse>(stmtAt(elseBlk, 6));
  ASSERT_NE(txIf, nullptr);
  EXPECT_EQ(opType(txIf->getCondition()), vpiLogAndOp);
  const hldb::IfElse *const busyIf = any_cast<hldb::IfElse>(txIf->getElseStmt());
  ASSERT_NE(busyIf, nullptr);
  EXPECT_EQ(opType(busyIf->getCondition()), vpiEqOp);
  ASSERT_EQ(operandCount(busyIf->getCondition()), 2U);
  EXPECT_EQ(opType(operandAt(busyIf->getCondition(), 1)), vpiUnaryOrOp);
  ASSERT_EQ(stmtCount(busyIf->getStmt()), 2U);
  const hldb::Assignment *const dlyInc = expectAssign(stmtAt(busyIf->getStmt(), 1), "tx_enable_dly", false);
  ASSERT_NE(dlyInc, nullptr);
  EXPECT_EQ(opType(dlyInc->getRhs()), vpiAddOp);
  const hldb::Any *const notAll = operandAt(dlyInc->getRhs(), 1);
  EXPECT_EQ(opType(notAll), vpiNotOp);
  EXPECT_EQ(opType(operandAt(notAll, 0)), vpiUnaryAndOp);
  const hldb::IfStmt *const idleIf = any_cast<hldb::IfStmt>(busyIf->getElseStmt());
  ASSERT_NE(idleIf, nullptr);
  EXPECT_EQ(opType(idleIf->getCondition()), vpiBitNegOp);
}

// Sec 12.5: "case (core_lsu_addr[6:3])" with items 0..6 and default, guarded
// by "if (framing_sel & we_d & (&core_lsu_be[3:0]) & (...))".
TEST_F(FramingTopTest, MsocClkCaseStatement) {
  const hldb::Always *const always = findAlways(definition(), "core_lsu_addr_dly");
  ASSERT_NE(always, nullptr);
  const hldb::IfElse *const rstIf = any_cast<hldb::IfElse>(clockedBody(always, "msoc_clk"));
  ASSERT_NE(rstIf, nullptr);
  const hldb::IfStmt *const wrIf = any_cast<hldb::IfStmt>(stmtAt(rstIf->getElseStmt(), 4));
  ASSERT_NE(wrIf, nullptr);
  EXPECT_EQ(opType(wrIf->getCondition()), vpiBitAndOp);
  const hldb::Any *const addrEq = lastOperand(wrIf->getCondition());
  EXPECT_EQ(opType(addrEq), vpiEqOp);

  const hldb::CaseStmt *const cs = any_cast<hldb::CaseStmt>(wrIf->getStmt());
  ASSERT_NE(cs, nullptr);
  EXPECT_EQ(cs->getCaseType(), vpiCaseExact);
  const hldb::PartSelect *const sel = any_cast<hldb::PartSelect>(cs->getCondition());
  ASSERT_NE(sel, nullptr);
  EXPECT_EQ(selectBaseName(sel->getName()), "core_lsu_addr");
  ASSERT_NE(sel->getRange(), nullptr);
  EXPECT_EQ(constValue(sel->getRange()->getLeftExpr()), "6");
  EXPECT_EQ(constValue(sel->getRange()->getRightExpr()), "3");

  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_EQ(cs->getCaseItems()->size(), 8U);
  for (size_t i = 0; i < 7; ++i) {
    const hldb::CaseItem *const item = cs->getCaseItems()->at(i);
    ASSERT_NE(item->getExprs(), nullptr) << "item " << i;
    ASSERT_EQ(item->getExprs()->size(), 1U) << "item " << i;
    EXPECT_EQ(constValue(item->getExprs()->at(0)), std::to_string(i)) << "item " << i;
  }
  const hldb::CaseItem *const deflt = cs->getCaseItems()->at(7);
  EXPECT_TRUE((deflt->getExprs() == nullptr) || deflt->getExprs()->empty());

  // 1: {irq_en,promiscuous,spare,loopback,cooked,mac_address[47:32]} <= core_lsu_wdata;
  const hldb::Assignment *const concatLhs = any_cast<hldb::Assignment>(cs->getCaseItems()->at(1)->getStmt());
  ASSERT_NE(concatLhs, nullptr);
  EXPECT_FALSE(concatLhs->getBlocking());
  EXPECT_EQ(opType(concatLhs->getLhs()), vpiConcatOp);
  EXPECT_EQ(operandCount(concatLhs->getLhs()), 6U);
  // 2: begin tx_enable_dly <= 10; tx_packet_length <= core_lsu_wdata; end
  const hldb::Any *const item2 = cs->getCaseItems()->at(2)->getStmt();
  ASSERT_EQ(stmtCount(item2), 2U);
  const hldb::Assignment *const ten = expectAssign(stmtAt(item2, 0), "tx_enable_dly", false);
  ASSERT_NE(ten, nullptr);
  EXPECT_EQ(constValue(ten->getRhs()), "10");
  expectAssign(stmtAt(item2, 1), "tx_packet_length", false);
  // 4: begin {phy_mdio_oe,phy_mdio_o,phy_mdclk} <= core_lsu_wdata; end
  const hldb::Any *const item4 = cs->getCaseItems()->at(4)->getStmt();
  ASSERT_EQ(stmtCount(item4), 1U);
  const hldb::Assignment *const mdio = any_cast<hldb::Assignment>(stmtAt(item4, 0));
  ASSERT_NE(mdio, nullptr);
  EXPECT_EQ(opType(mdio->getLhs()), vpiConcatOp);
  EXPECT_EQ(operandCount(mdio->getLhs()), 3U);
}

// always @(posedge clk_int): tx_enable_i control.
TEST_F(FramingTopTest, TxEnableAlways) {
  const hldb::Always *const always = findAlways(definition(), "tx_enable_i");
  ASSERT_NE(always, nullptr);
  const hldb::IfElse *const rstIf = any_cast<hldb::IfElse>(clockedBody(always, "clk_int"));
  ASSERT_NE(rstIf, nullptr);
  ASSERT_NE(rstIf->getCondition(), nullptr);
  EXPECT_EQ(selectBaseName(rstIf->getCondition()->getName()), "rst_int");
  ASSERT_EQ(stmtCount(rstIf->getStmt()), 1U);
  expectAssign(stmtAt(rstIf->getStmt(), 0), "tx_enable_i", false);
  ASSERT_EQ(stmtCount(rstIf->getElseStmt()), 1U);
  const hldb::IfElse *const lastIf = any_cast<hldb::IfElse>(stmtAt(rstIf->getElseStmt(), 0));
  ASSERT_NE(lastIf, nullptr);
  EXPECT_EQ(opType(lastIf->getCondition()), vpiLogAndOp);
  const hldb::IfStmt *const fullIf = any_cast<hldb::IfStmt>(lastIf->getElseStmt());
  ASSERT_NE(fullIf, nullptr);
  EXPECT_EQ(opType(fullIf->getCondition()), vpiEqOp);
  ASSERT_EQ(operandCount(fullIf->getCondition()), 2U);
  EXPECT_EQ(constValue(operandAt(fullIf->getCondition(), 0)), "1");
  EXPECT_EQ(opType(operandAt(fullIf->getCondition(), 1)), vpiUnaryAndOp);
  const hldb::Assignment *const set = expectAssign(fullIf->getStmt(), "tx_enable_i", false);
  ASSERT_NE(set, nullptr);
  EXPECT_EQ(constValue(set->getRhs()), "1");
}

// Sec 9.4.2.2, 12.5.1: always @* casez({ce_d_dly, core_lsu_addr_dly[14:3]})
// with 10 wildcard items and a default; all assignments are blocking.
TEST_F(FramingTopTest, ReadMuxCasez) {
  const hldb::Always *const always = findAlways(definition(), "framing_rdata");
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);
  const hldb::EventControl *const ec = any_cast<hldb::EventControl>(always->getStmt());
  ASSERT_NE(ec, nullptr);
  EXPECT_TRUE(ec->getIsImplicitControl());

  const hldb::CaseStmt *const cs = any_cast<hldb::CaseStmt>(ec->getStmt());
  ASSERT_NE(cs, nullptr);
  EXPECT_EQ(cs->getCaseType(), vpiCaseZ);
  EXPECT_EQ(opType(cs->getCondition()), vpiConcatOp);
  ASSERT_EQ(operandCount(cs->getCondition()), 2U);
  EXPECT_EQ(selectBaseName(operandAt(cs->getCondition(), 0)->getName()), "ce_d_dly");
  const hldb::PartSelect *const addr = any_cast<hldb::PartSelect>(operandAt(cs->getCondition(), 1));
  ASSERT_NE(addr, nullptr);
  EXPECT_EQ(selectBaseName(addr->getName()), "core_lsu_addr_dly");
  ASSERT_NE(addr->getRange(), nullptr);
  EXPECT_EQ(constValue(addr->getRange()->getLeftExpr()), "14");
  EXPECT_EQ(constValue(addr->getRange()->getRightExpr()), "3");

  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_EQ(cs->getCaseItems()->size(), 11U);
  for (size_t i = 0; i < 10; ++i) {
    const hldb::CaseItem *const item = cs->getCaseItems()->at(i);
    ASSERT_NE(item->getExprs(), nullptr) << "item " << i;
    ASSERT_EQ(item->getExprs()->size(), 1U) << "item " << i;
    const hldb::Constant *const pattern = any_cast<hldb::Constant>(item->getExprs()->at(0));
    ASSERT_NE(pattern, nullptr) << "item " << i;
    EXPECT_EQ(pattern->getConstType(), vpiBinaryConst) << "item " << i;
    EXPECT_EQ(pattern->getSize(), 13) << "item " << i;
    expectAssign(item->getStmt(), "framing_rdata", true);
  }
  const hldb::CaseItem *const deflt = cs->getCaseItems()->at(10);
  EXPECT_TRUE((deflt->getExprs() == nullptr) || deflt->getExprs()->empty());
  const hldb::Assignment *const zero = expectAssign(deflt->getStmt(), "framing_rdata", true);
  ASSERT_NE(zero, nullptr);
  EXPECT_EQ(constValue(zero->getRhs()), "0");

  // Selected right-hand sides.
  const hldb::Assignment *const a0 = any_cast<hldb::Assignment>(cs->getCaseItems()->at(0)->getStmt());
  ASSERT_NE(a0, nullptr);
  const hldb::PartSelect *const mac = any_cast<hldb::PartSelect>(a0->getRhs());
  ASSERT_NE(mac, nullptr);
  EXPECT_EQ(selectBaseName(mac->getName()), "mac_address");
  const hldb::Assignment *const a1 = any_cast<hldb::Assignment>(cs->getCaseItems()->at(1)->getStmt());
  ASSERT_NE(a1, nullptr);
  EXPECT_EQ(opType(a1->getRhs()), vpiConcatOp);
  EXPECT_EQ(operandCount(a1->getRhs()), 6U);
  const hldb::Assignment *const a2 = any_cast<hldb::Assignment>(cs->getCaseItems()->at(2)->getStmt());
  ASSERT_NE(a2, nullptr);
  EXPECT_EQ(opType(a2->getRhs()), vpiConcatOp);
  EXPECT_EQ(operandCount(a2->getRhs()), 5U);
  const hldb::Assignment *const a7 = any_cast<hldb::Assignment>(cs->getCaseItems()->at(7)->getStmt());
  ASSERT_NE(a7, nullptr);
  ASSERT_NE(a7->getRhs(), nullptr);
  EXPECT_EQ(selectBaseName(a7->getRhs()->getName()), "rx_length_axis");
  const hldb::Assignment *const a9 = any_cast<hldb::Assignment>(cs->getCaseItems()->at(9)->getStmt());
  ASSERT_NE(a9, nullptr);
  ASSERT_NE(a9->getRhs(), nullptr);
  EXPECT_EQ(selectBaseName(a9->getRhs()->getName()), "framing_rdata_pkt");
}

// always @(posedge clk_int): transmit frame address / valid / last.
TEST_F(FramingTopTest, TxFrameAlways) {
  const hldb::Always *const always = findAlways(definition(), "tx_axis_tvalid");
  ASSERT_NE(always, nullptr);
  const hldb::IfElse *const rstIf = any_cast<hldb::IfElse>(clockedBody(always, "clk_int"));
  ASSERT_NE(rstIf, nullptr);
  ASSERT_NE(rstIf->getCondition(), nullptr);
  EXPECT_EQ(selectBaseName(rstIf->getCondition()->getName()), "rst_int");
  ASSERT_EQ(stmtCount(rstIf->getStmt()), 4U);
  expectAssign(stmtAt(rstIf->getStmt(), 0), "tx_axis_tvalid", false);
  expectAssign(stmtAt(rstIf->getStmt(), 1), "tx_axis_tvalid_dly", false);
  expectAssign(stmtAt(rstIf->getStmt(), 2), "tx_frame_addr", false);
  expectAssign(stmtAt(rstIf->getStmt(), 3), "tx_axis_tlast", false);

  const hldb::Any *const elseBlk = rstIf->getElseStmt();
  ASSERT_EQ(stmtCount(elseBlk), 5U);
  expectAssign(stmtAt(elseBlk, 0), "tx_enable_old", false);
  const hldb::IfStmt *const startIf = any_cast<hldb::IfStmt>(stmtAt(elseBlk, 1));
  ASSERT_NE(startIf, nullptr);
  EXPECT_EQ(opType(startIf->getCondition()), vpiBitAndOp);
  const hldb::IfStmt *const readyIf = any_cast<hldb::IfStmt>(stmtAt(elseBlk, 2));
  ASSERT_NE(readyIf, nullptr);
  ASSERT_NE(readyIf->getCondition(), nullptr);
  EXPECT_EQ(selectBaseName(readyIf->getCondition()->getName()), "tx_axis_tready");
  ASSERT_EQ(stmtCount(readyIf->getStmt()), 2U);
  expectAssign(stmtAt(readyIf->getStmt(), 0), "tx_frame_addr", false);
  const hldb::Assignment *const tlast = expectAssign(stmtAt(readyIf->getStmt(), 1), "tx_axis_tlast", false);
  ASSERT_NE(tlast, nullptr);
  EXPECT_EQ(opType(tlast->getRhs()), vpiBitAndOp);
  const hldb::Any *const eq = operandAt(tlast->getRhs(), 0);
  EXPECT_EQ(opType(eq), vpiEqOp);
  EXPECT_EQ(opType(operandAt(eq, 1)), vpiSubOp);
  expectAssign(stmtAt(elseBlk, 3), "tx_axis_tvalid", false);
  const hldb::IfElse *const validIf = any_cast<hldb::IfElse>(stmtAt(elseBlk, 4));
  ASSERT_NE(validIf, nullptr);
  expectAssign(validIf->getStmt(), "tx_axis_tvalid_dly", false);
  const hldb::IfStmt *const notLast = any_cast<hldb::IfStmt>(validIf->getElseStmt());
  ASSERT_NE(notLast, nullptr);
  EXPECT_EQ(opType(notLast->getCondition()), vpiBitNegOp);
  expectAssign(notLast->getStmt(), "tx_axis_tvalid_dly", false);
}

// always @(posedge rx_clk): receive address / destination MAC / length array.
TEST_F(FramingTopTest, RxAddrAlways) {
  const hldb::Always *const always = findAlways(definition(), "rx_addr_axis");
  ASSERT_NE(always, nullptr);
  const hldb::IfElse *const rstIf = any_cast<hldb::IfElse>(clockedBody(always, "rx_clk"));
  ASSERT_NE(rstIf, nullptr);
  ASSERT_EQ(stmtCount(rstIf->getStmt()), 2U);
  expectAssign(stmtAt(rstIf->getStmt(), 0), "rx_addr_axis", false);
  expectAssign(stmtAt(rstIf->getStmt(), 1), "rx_dest_mac", false);

  const hldb::Any *const elseBlk = rstIf->getElseStmt();
  ASSERT_EQ(stmtCount(elseBlk), 2U);
  const hldb::IfStmt *const validIf = any_cast<hldb::IfStmt>(stmtAt(elseBlk, 0));
  ASSERT_NE(validIf, nullptr);
  ASSERT_EQ(stmtCount(validIf->getStmt()), 2U);
  expectAssign(stmtAt(validIf->getStmt(), 0), "rx_addr_axis", false);
  const hldb::IfStmt *const macIf = any_cast<hldb::IfStmt>(stmtAt(validIf->getStmt(), 1));
  ASSERT_NE(macIf, nullptr);
  EXPECT_EQ(opType(macIf->getCondition()), vpiLtOp);
  const hldb::Assignment *const shift = expectAssign(macIf->getStmt(), "rx_dest_mac", false);
  ASSERT_NE(shift, nullptr);
  EXPECT_EQ(opType(shift->getRhs()), vpiConcatOp);
  ASSERT_EQ(operandCount(shift->getRhs()), 2U);
  const hldb::PartSelect *const low = any_cast<hldb::PartSelect>(operandAt(shift->getRhs(), 0));
  ASSERT_NE(low, nullptr);
  EXPECT_EQ(selectBaseName(low->getName()), "rx_dest_mac");
  EXPECT_EQ(selectBaseName(operandAt(shift->getRhs(), 1)->getName()), "rx_axis_tdata");

  const hldb::IfStmt *const lastIf = any_cast<hldb::IfStmt>(stmtAt(elseBlk, 1));
  ASSERT_NE(lastIf, nullptr);
  ASSERT_EQ(stmtCount(lastIf->getStmt()), 2U);
  const hldb::Assignment *const len = expectAssign(stmtAt(lastIf->getStmt(), 0), "rx_length_axis", false);
  ASSERT_NE(len, nullptr);
  EXPECT_EQ(opType(len->getRhs()), vpiAddOp);
  expectAssign(stmtAt(lastIf->getStmt(), 1), "rx_addr_axis", false);
}

// Sec 23.3.2.2: dualmem_widen8 RAMB16_inst_rx connections.
TEST_F(FramingTopTest, RxDualmemInstance) {
  const hldb::RefInstance *const inst = refInstance("RAMB16_inst_rx");
  ASSERT_NE(inst, nullptr);
  EXPECT_TRUE(refersTo(inst, "dualmem_widen8"));
  ASSERT_NE(inst->getArguments(), nullptr);
  EXPECT_EQ(inst->getArguments()->size(), 12U);
  EXPECT_EQ(connName(inst, "clka"), "rx_clk");
  EXPECT_EQ(connName(inst, "clkb"), "msoc_clk");
  const hldb::NamedArgument *const douta = instConn(inst, "douta");
  ASSERT_NE(douta, nullptr);
  EXPECT_EQ(douta->getHighConn(), nullptr) << ".douta() is unconnected";
  EXPECT_EQ(connName(inst, "ena"), "rx_axis_tvalid");
  EXPECT_EQ(connName(inst, "wea"), "rx_wr");
  EXPECT_EQ(connName(inst, "doutb"), "framing_rdata_pkt");
  EXPECT_EQ(connName(inst, "dinb"), "core_lsu_wdata");

  const hldb::NamedArgument *const addra = instConn(inst, "addra");
  ASSERT_NE(addra, nullptr);
  EXPECT_EQ(opType(addra->getHighConn()), vpiConcatOp);
  ASSERT_EQ(operandCount(addra->getHighConn()), 3U);
  EXPECT_EQ(selectBaseName(operandAt(addra->getHighConn(), 0)->getName()), "nextbuf");
  EXPECT_EQ(selectBaseName(operandAt(addra->getHighConn(), 1)->getName()), "rx_addr_axis");
  EXPECT_EQ(selectBaseName(operandAt(addra->getHighConn(), 2)->getName()), "rx_addr_axis");

  const hldb::NamedArgument *const dina = instConn(inst, "dina");
  ASSERT_NE(dina, nullptr);
  EXPECT_EQ(opType(dina->getHighConn()), vpiConcatOp);
  ASSERT_EQ(operandCount(dina->getHighConn()), 2U);
  EXPECT_EQ(selectBaseName(operandAt(dina->getHighConn(), 0)->getName()), "rx_axis_tdata");
  EXPECT_EQ(selectBaseName(operandAt(dina->getHighConn(), 1)->getName()), "rx_axis_tdata");

  const hldb::NamedArgument *const addrb = instConn(inst, "addrb");
  ASSERT_NE(addrb, nullptr);
  const hldb::PartSelect *const addrbSel = any_cast<hldb::PartSelect>(addrb->getHighConn());
  ASSERT_NE(addrbSel, nullptr);
  EXPECT_EQ(selectBaseName(addrbSel->getName()), "core_lsu_addr");

  const hldb::NamedArgument *const enb = instConn(inst, "enb");
  ASSERT_NE(enb, nullptr);
  EXPECT_EQ(opType(enb->getHighConn()), vpiBitAndOp);
  const hldb::BitSelect *const bit14 = any_cast<hldb::BitSelect>(lastOperand(enb->getHighConn()));
  ASSERT_NE(bit14, nullptr);
  EXPECT_EQ(selectBaseName(bit14->getName()), "core_lsu_addr");
  EXPECT_EQ(constValue(bit14->getIndex()), "14");

  const hldb::NamedArgument *const web = instConn(inst, "web");
  ASSERT_NE(web, nullptr);
  EXPECT_EQ(opType(web->getHighConn()), vpiConditionOp);
  ASSERT_EQ(operandCount(web->getHighConn()), 3U);
  EXPECT_EQ(selectBaseName(operandAt(web->getHighConn(), 0)->getName()), "we_d");
  const hldb::Any *const bytes = operandAt(web->getHighConn(), 1);
  EXPECT_EQ(opType(bytes), vpiConcatOp);
  ASSERT_EQ(operandCount(bytes), 2U);
  EXPECT_EQ(opType(operandAt(bytes, 0)), vpiUnaryOrOp);
  EXPECT_EQ(opType(operandAt(bytes, 1)), vpiUnaryOrOp);
  const hldb::Constant *const none = any_cast<hldb::Constant>(operandAt(web->getHighConn(), 2));
  ASSERT_NE(none, nullptr);
  EXPECT_EQ(none->getSize(), 2);
  EXPECT_EQ(constValue(none), "0");
}

// Sec 23.3.2.2: dualmem_widen RAMB16_inst_tx connections.
TEST_F(FramingTopTest, TxDualmemInstance) {
  const hldb::RefInstance *const inst = refInstance("RAMB16_inst_tx");
  ASSERT_NE(inst, nullptr);
  EXPECT_TRUE(refersTo(inst, "dualmem_widen"));
  ASSERT_NE(inst->getArguments(), nullptr);
  EXPECT_EQ(inst->getArguments()->size(), 12U);

  const hldb::NamedArgument *const clka = instConn(inst, "clka");
  ASSERT_NE(clka, nullptr);
  EXPECT_EQ(opType(clka->getHighConn()), vpiBitNegOp);
  ASSERT_EQ(operandCount(clka->getHighConn()), 1U);
  EXPECT_EQ(selectBaseName(operandAt(clka->getHighConn(), 0)->getName()), "clk_int");
  EXPECT_EQ(connName(inst, "clkb"), "msoc_clk");
  EXPECT_EQ(connName(inst, "douta"), "douta");
  EXPECT_EQ(connName(inst, "ena"), "tx_axis_tvalid");
  EXPECT_EQ(connName(inst, "doutb"), "framing_wdata_pkt");

  const hldb::NamedArgument *const addra = instConn(inst, "addra");
  ASSERT_NE(addra, nullptr);
  EXPECT_EQ(opType(addra->getHighConn()), vpiConcatOp);
  ASSERT_EQ(operandCount(addra->getHighConn()), 3U);
  const hldb::Constant *const msb = any_cast<hldb::Constant>(operandAt(addra->getHighConn(), 0));
  ASSERT_NE(msb, nullptr);
  EXPECT_EQ(msb->getSize(), 1);
  EXPECT_EQ(constValue(msb), "0");

  const hldb::NamedArgument *const dina = instConn(inst, "dina");
  ASSERT_NE(dina, nullptr);
  const hldb::Constant *const dinaZero = any_cast<hldb::Constant>(dina->getHighConn());
  ASSERT_NE(dinaZero, nullptr);
  EXPECT_EQ(dinaZero->getSize(), 16);
  EXPECT_EQ(constValue(dinaZero), "0");

  const hldb::NamedArgument *const wea = instConn(inst, "wea");
  ASSERT_NE(wea, nullptr);
  const hldb::Constant *const weaZero = any_cast<hldb::Constant>(wea->getHighConn());
  ASSERT_NE(weaZero, nullptr);
  EXPECT_EQ(weaZero->getSize(), 2);
  EXPECT_EQ(constValue(weaZero), "0");

  const hldb::NamedArgument *const enb = instConn(inst, "enb");
  ASSERT_NE(enb, nullptr);
  EXPECT_EQ(opType(enb->getHighConn()), vpiBitAndOp);
  const hldb::Any *const page = lastOperand(enb->getHighConn());
  EXPECT_EQ(opType(page), vpiEqOp);
  ASSERT_EQ(operandCount(page), 2U);
  const hldb::PartSelect *const pageSel = any_cast<hldb::PartSelect>(operandAt(page, 0));
  ASSERT_NE(pageSel, nullptr);
  EXPECT_EQ(selectBaseName(pageSel->getName()), "core_lsu_addr");
  const hldb::Constant *const pageNo = any_cast<hldb::Constant>(operandAt(page, 1));
  ASSERT_NE(pageNo, nullptr);
  EXPECT_EQ(pageNo->getSize(), 3);
  EXPECT_EQ(constValue(pageNo), "001");
}

// Sec 23.3.2.2: rgmii_soc rgmii_soc1, every port connected by name.
TEST_F(FramingTopTest, RgmiiSocInstance) {
  const hldb::RefInstance *const inst = refInstance("rgmii_soc1");
  ASSERT_NE(inst, nullptr);
  EXPECT_TRUE(refersTo(inst, "rgmii_soc"));
  EXPECT_EQ(instOverrideCount(inst), 0U);
  ASSERT_NE(inst->getArguments(), nullptr);
  EXPECT_EQ(inst->getArguments()->size(), 26U);
  for (std::string_view name :
       {"rst_int",       "clk_int",       "clk90_int",      "clk_200_int",    "rx_clk",         "phy_rx_clk",
        "phy_rxd",       "phy_rx_ctl",    "phy_tx_clk",     "phy_txd",        "phy_tx_ctl",     "phy_reset_n",
        "phy_int_n",     "phy_pme_n",     "mac_gmii_tx_en", "tx_axis_tdata",  "tx_axis_tvalid", "tx_axis_tready",
        "tx_axis_tlast", "tx_axis_tuser", "rx_axis_tdata",  "rx_axis_tvalid", "rx_axis_tlast",  "rx_axis_tuser",
        "rx_fcs_reg",    "tx_fcs_reg"}) {
    EXPECT_EQ(connName(inst, name), name);
  }
}

// Sec 22.6: XILINX_ILA_1/2/3 are never defined, so the three `ifdef blocks
// are skipped: no ILA instance exists and no xlnx_ila_* module is looked up.
TEST_F(FramingTopTest, IfdefIlaBlocksNotCompiled) {
  const hldb::Module *const module = definition();
  ASSERT_NE(module, nullptr);
  ASSERT_NE(module->getRefInstances(), nullptr);
  EXPECT_EQ(module->getRefInstances()->size(), 3U);
  const hldb::Module *const elab = top();
  ASSERT_NE(elab, nullptr);
  for (std::string_view inst : {"eth_ila_clk_rx", "eth_ila_clk_int", "eth_ila_clk_msoc"}) {
    EXPECT_EQ(hldb::findByName<hldb::RefInstance>(inst, module->getRefInstances()), nullptr) << inst;
    EXPECT_EQ(childInstance(elab, inst), nullptr) << inst;
  }
  for (std::string_view mod : {"xlnx_ila_1", "xlnx_ila_2", "xlnx_ila_3"}) {
    EXPECT_EQ(findError(ErrorDefinition::ELAB_NO_MODULE_DEFINITION, mod), nullptr) << mod;
  }
}

// Sec 23.3.1: framing_top is the top; its elaborated children.
TEST_F(FramingTopTest, ElaboratedHierarchy) {
  GTEST_SKIP() << "HLC 786 builds no elaborated hierarchy: getTopModules() is empty and child instances / generate "
                  "scopes are not created. "
                  "Per IEEE 1800-2023 Sec 23.10 and 27 elaboration builds the instance tree and expands generate "
                  "blocks. Fix pending.";
  const hldb::Module *const elab = top();
  ASSERT_NE(elab, nullptr);
  EXPECT_EQ(elab->getDefName(), "framing_top");
  ASSERT_NE(elab->getModules(), nullptr);
  EXPECT_EQ(elab->getModules()->size(), 3U);
  const hldb::Module *const rxMem = childInstance(elab, "RAMB16_inst_rx");
  ASSERT_NE(rxMem, nullptr);
  EXPECT_EQ(rxMem->getDefName(), "dualmem_widen8");
  const hldb::Module *const txMem = childInstance(elab, "RAMB16_inst_tx");
  ASSERT_NE(txMem, nullptr);
  EXPECT_EQ(txMem->getDefName(), "dualmem_widen");
  const hldb::Module *const soc = childInstance(elab, "rgmii_soc1");
  ASSERT_NE(soc, nullptr);
  EXPECT_EQ(soc->getDefName(), "rgmii_soc");
  const hldb::Module *const core = childInstance(soc, "core_inst");
  ASSERT_NE(core, nullptr);
  EXPECT_EQ(core->getDefName(), "rgmii_core");
  EXPECT_EQ(paramValue(core, "TARGET"), "XILINX");

  const hldb::Port *const douta = hldb::findByName<hldb::Port>("douta", rxMem->getPorts());
  ASSERT_NE(douta, nullptr);
  EXPECT_EQ(douta->getHighConn(), nullptr) << ".douta() is unconnected";
}

// Sec 23.3: the elaborated design instantiates modules with no definition:
// infer_dpram (dualmem_widen8 / dualmem_widen), IDELAYCTRL / IDELAYE2
// (rgmii_soc) and BUFIO / BUFR / IDDR / ODDR (Xilinx branches below rgmii_core).
TEST_F(FramingTopTest, UndefinedModulesReported) {
  GTEST_SKIP()
      << "HLC 786 reports no error for an elaborated reference to a module or interface that has no definition. "
         "Per IEEE 1800-2023 Sec 23.3.2 and 25.3 such a reference is an error. Fix pending.";
  for (std::string_view name : {"infer_dpram", "IDELAYCTRL", "IDELAYE2", "BUFIO", "BUFR", "IDDR", "ODDR"}) {
    EXPECT_NE(findError(ErrorDefinition::ELAB_NO_MODULE_DEFINITION, name), nullptr) << name;
  }
}

// Sec 22.6, 27.5: RAMB16_S9_S36 sits in the not-taken `ifdef RAMB16 branch;
// BUFG / BUFIO2 / IDDR2 / ODDR2 / Altera primitives sit in unselected generate
// branches. None of them may be reported.
TEST_F(FramingTopTest, UndefinedModulesNotReported) {
  for (std::string_view name : {"RAMB16_S9_S36", "BUFG", "BUFIO2", "IDDR2", "ODDR2", "altddio_in", "altddio_out"}) {
    EXPECT_EQ(findError(ErrorDefinition::ELAB_NO_MODULE_DEFINITION, name), nullptr) << name;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
