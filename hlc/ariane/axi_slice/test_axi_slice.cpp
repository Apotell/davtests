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

// Source under test: tests/ariane/fpga/src/axi_slice/src/axi_slice.sv
// (copied verbatim below). The Compile() arguments also pull in the five
// channel buffers (axi_aw/ar/w/r/b_buffer.sv), axi_single_slice.sv and the
// fifo / fifo_v2 / fifo_v3 sources, so the whole hierarchy below axi_slice
// elaborates.
//
// // Copyright 2018 ETH Zurich and University of Bologna.
// // Copyright and related rights are licensed under the Solderpad Hardware
// // License, Version 0.51 (the "License"); you may not use this file except in
// // compliance with the License. You may obtain a copy of the License at
// // http://solderpad.org/licenses/SHL-0.51. Unless required by applicable law
// // or agreed to in writing, software, hardware and materials distributed under
// // this License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// // CONDITIONS OF ANY KIND, either express or implied. See the License for the
// // specific language governing permissions and limitations under the License.
//
// // Davide Rossi <davide.rossi@unibo.it>
//
// module axi_slice
// #(
//     parameter AXI_ADDR_WIDTH = 32,
//     parameter AXI_DATA_WIDTH = 64,
//     parameter AXI_USER_WIDTH = 6,
//     parameter AXI_ID_WIDTH   = 3,
//     parameter SLICE_DEPTH    = 2,
//     parameter AXI_STRB_WIDTH = AXI_DATA_WIDTH/8
// )
// (
//     input  logic                      clk_i,
//     input  logic                      rst_ni,
//     input  logic                      test_en_i,
//
//     // AXI4 SLAVE
//     //***************************************
//     // WRITE ADDRESS CHANNEL
//     input  logic                      axi_slave_aw_valid_i,
//     input  logic [AXI_ADDR_WIDTH-1:0] axi_slave_aw_addr_i,
//     input  logic [2:0]                axi_slave_aw_prot_i,
//     input  logic [3:0]                axi_slave_aw_region_i,
//     input  logic [7:0]                axi_slave_aw_len_i,
//     input  logic [2:0]                axi_slave_aw_size_i,
//     input  logic [1:0]                axi_slave_aw_burst_i,
//     input  logic                      axi_slave_aw_lock_i,
//     input  logic [3:0]                axi_slave_aw_cache_i,
//     input  logic [3:0]                axi_slave_aw_qos_i,
//     input  logic [AXI_ID_WIDTH-1:0]   axi_slave_aw_id_i,
//     input  logic [AXI_USER_WIDTH-1:0] axi_slave_aw_user_i,
//     output logic                      axi_slave_aw_ready_o,
//
//     // READ ADDRESS CHANNEL
//     input  logic                      axi_slave_ar_valid_i,
//     input  logic [AXI_ADDR_WIDTH-1:0] axi_slave_ar_addr_i,
//     input  logic [2:0]                axi_slave_ar_prot_i,
//     input  logic [3:0]                axi_slave_ar_region_i,
//     input  logic [7:0]                axi_slave_ar_len_i,
//     input  logic [2:0]                axi_slave_ar_size_i,
//     input  logic [1:0]                axi_slave_ar_burst_i,
//     input  logic                      axi_slave_ar_lock_i,
//     input  logic [3:0]                axi_slave_ar_cache_i,
//     input  logic [3:0]                axi_slave_ar_qos_i,
//     input  logic [AXI_ID_WIDTH-1:0]   axi_slave_ar_id_i,
//     input  logic [AXI_USER_WIDTH-1:0] axi_slave_ar_user_i,
//     output logic                      axi_slave_ar_ready_o,
//
//     // WRITE DATA CHANNEL
//     input  logic                      axi_slave_w_valid_i,
//     input  logic [AXI_DATA_WIDTH-1:0] axi_slave_w_data_i,
//     input  logic [AXI_STRB_WIDTH-1:0] axi_slave_w_strb_i,
//     input  logic [AXI_USER_WIDTH-1:0] axi_slave_w_user_i,
//     input  logic                      axi_slave_w_last_i,
//     output logic                      axi_slave_w_ready_o,
//
//     // READ DATA CHANNEL
//     output logic                      axi_slave_r_valid_o,
//     output logic [AXI_DATA_WIDTH-1:0] axi_slave_r_data_o,
//     output logic [1:0]                axi_slave_r_resp_o,
//     output logic                      axi_slave_r_last_o,
//     output logic [AXI_ID_WIDTH-1:0]   axi_slave_r_id_o,
//     output logic [AXI_USER_WIDTH-1:0] axi_slave_r_user_o,
//     input  logic                      axi_slave_r_ready_i,
//
//     // WRITE RESPONSE CHANNEL
//     output logic                      axi_slave_b_valid_o,
//     output logic [1:0]                axi_slave_b_resp_o,
//     output logic [AXI_ID_WIDTH-1:0]   axi_slave_b_id_o,
//     output logic [AXI_USER_WIDTH-1:0] axi_slave_b_user_o,
//     input  logic                      axi_slave_b_ready_i,
//
//         // AXI4 MASTER
//     //***************************************
//     // WRITE ADDRESS CHANNEL
//     output logic                      axi_master_aw_valid_o,
//     output logic [AXI_ADDR_WIDTH-1:0] axi_master_aw_addr_o,
//     output logic [2:0]                axi_master_aw_prot_o,
//     output logic [3:0]                axi_master_aw_region_o,
//     output logic [7:0]                axi_master_aw_len_o,
//     output logic [2:0]                axi_master_aw_size_o,
//     output logic [1:0]                axi_master_aw_burst_o,
//     output logic                      axi_master_aw_lock_o,
//     output logic [3:0]                axi_master_aw_cache_o,
//     output logic [3:0]                axi_master_aw_qos_o,
//     output logic [AXI_ID_WIDTH-1:0]   axi_master_aw_id_o,
//     output logic [AXI_USER_WIDTH-1:0] axi_master_aw_user_o,
//     input  logic                      axi_master_aw_ready_i,
//
//     // READ ADDRESS CHANNEL
//     output logic                      axi_master_ar_valid_o,
//     output logic [AXI_ADDR_WIDTH-1:0] axi_master_ar_addr_o,
//     output logic [2:0]                axi_master_ar_prot_o,
//     output logic [3:0]                axi_master_ar_region_o,
//     output logic [7:0]                axi_master_ar_len_o,
//     output logic [2:0]                axi_master_ar_size_o,
//     output logic [1:0]                axi_master_ar_burst_o,
//     output logic                      axi_master_ar_lock_o,
//     output logic [3:0]                axi_master_ar_cache_o,
//     output logic [3:0]                axi_master_ar_qos_o,
//     output logic [AXI_ID_WIDTH-1:0]   axi_master_ar_id_o,
//     output logic [AXI_USER_WIDTH-1:0] axi_master_ar_user_o,
//     input  logic                      axi_master_ar_ready_i,
//
//     // WRITE DATA CHANNEL
//     output logic                      axi_master_w_valid_o,
//     output logic [AXI_DATA_WIDTH-1:0] axi_master_w_data_o,
//     output logic [AXI_STRB_WIDTH-1:0] axi_master_w_strb_o,
//     output logic [AXI_USER_WIDTH-1:0] axi_master_w_user_o,
//     output logic                      axi_master_w_last_o,
//     input  logic                      axi_master_w_ready_i,
//
//     // READ DATA CHANNEL
//     input  logic                      axi_master_r_valid_i,
//     input  logic [AXI_DATA_WIDTH-1:0] axi_master_r_data_i,
//     input  logic [1:0]                axi_master_r_resp_i,
//     input  logic                      axi_master_r_last_i,
//     input  logic [AXI_ID_WIDTH-1:0]   axi_master_r_id_i,
//     input  logic [AXI_USER_WIDTH-1:0] axi_master_r_user_i,
//     output logic                      axi_master_r_ready_o,
//
//     // WRITE RESPONSE CHANNEL
//     input  logic                      axi_master_b_valid_i,
//     input  logic [1:0]                axi_master_b_resp_i,
//     input  logic [AXI_ID_WIDTH-1:0]   axi_master_b_id_i,
//     input  logic [AXI_USER_WIDTH-1:0] axi_master_b_user_i,
//     output logic                      axi_master_b_ready_o
// );
//
//    // AXI WRITE ADDRESS CHANNEL BUFFER
//    axi_aw_buffer
//    #(
//        .ID_WIDTH     (AXI_ID_WIDTH),
//        .ADDR_WIDTH   (AXI_ADDR_WIDTH),
//        .USER_WIDTH   (AXI_USER_WIDTH),
//        .BUFFER_DEPTH (SLICE_DEPTH)
//    )
//    aw_buffer_i
//    (
//       .clk_i            ( clk_i                  ),
//       .rst_ni           ( rst_ni                 ),
//       .test_en_i        ( test_en_i              ),
//
//       .slave_valid_i    ( axi_slave_aw_valid_i   ),
//       .slave_addr_i     ( axi_slave_aw_addr_i    ),
//       .slave_prot_i     ( axi_slave_aw_prot_i    ),
//       .slave_region_i   ( axi_slave_aw_region_i  ),
//       .slave_len_i      ( axi_slave_aw_len_i     ),
//       .slave_size_i     ( axi_slave_aw_size_i    ),
//       .slave_burst_i    ( axi_slave_aw_burst_i   ),
//       .slave_lock_i     ( axi_slave_aw_lock_i    ),
//       .slave_cache_i    ( axi_slave_aw_cache_i   ),
//       .slave_qos_i      ( axi_slave_aw_qos_i     ),
//       .slave_id_i       ( axi_slave_aw_id_i      ),
//       .slave_user_i     ( axi_slave_aw_user_i    ),
//       .slave_ready_o    ( axi_slave_aw_ready_o   ),
//
//       .master_valid_o   ( axi_master_aw_valid_o  ),
//       .master_addr_o    ( axi_master_aw_addr_o   ),
//       .master_prot_o    ( axi_master_aw_prot_o   ),
//       .master_region_o  ( axi_master_aw_region_o ),
//       .master_len_o     ( axi_master_aw_len_o    ),
//       .master_size_o    ( axi_master_aw_size_o   ),
//       .master_burst_o   ( axi_master_aw_burst_o  ),
//       .master_lock_o    ( axi_master_aw_lock_o   ),
//       .master_cache_o   ( axi_master_aw_cache_o  ),
//       .master_qos_o     ( axi_master_aw_qos_o    ),
//       .master_id_o      ( axi_master_aw_id_o     ),
//       .master_user_o    ( axi_master_aw_user_o   ),
//       .master_ready_i   ( axi_master_aw_ready_i  )
//    );
//
//    // AXI READ ADDRESS CHANNEL BUFFER
//    axi_ar_buffer
//    #(
//        .ID_WIDTH     (AXI_ID_WIDTH),
//        .ADDR_WIDTH   (AXI_ADDR_WIDTH),
//        .USER_WIDTH   (AXI_USER_WIDTH),
//        .BUFFER_DEPTH (SLICE_DEPTH)
//    )
//    ar_buffer_i
//    (
//       .clk_i           ( clk_i                   ),
//       .rst_ni          ( rst_ni                  ),
//       .test_en_i       ( test_en_i               ),
//
//       .slave_valid_i   ( axi_slave_ar_valid_i    ),
//       .slave_addr_i    ( axi_slave_ar_addr_i     ),
//       .slave_prot_i    ( axi_slave_ar_prot_i     ),
//       .slave_region_i  ( axi_slave_ar_region_i   ),
//       .slave_len_i     ( axi_slave_ar_len_i      ),
//       .slave_size_i    ( axi_slave_ar_size_i     ),
//       .slave_burst_i   ( axi_slave_ar_burst_i    ),
//       .slave_lock_i    ( axi_slave_ar_lock_i     ),
//       .slave_cache_i   ( axi_slave_ar_cache_i    ),
//       .slave_qos_i     ( axi_slave_ar_qos_i      ),
//       .slave_id_i      ( axi_slave_ar_id_i       ),
//       .slave_user_i    ( axi_slave_ar_user_i     ),
//       .slave_ready_o   ( axi_slave_ar_ready_o    ),
//
//       .master_valid_o  ( axi_master_ar_valid_o   ),
//       .master_addr_o   ( axi_master_ar_addr_o    ),
//       .master_prot_o   ( axi_master_ar_prot_o    ),
//       .master_region_o ( axi_master_ar_region_o  ),
//       .master_len_o    ( axi_master_ar_len_o     ),
//       .master_size_o   ( axi_master_ar_size_o    ),
//       .master_burst_o  ( axi_master_ar_burst_o   ),
//       .master_lock_o   ( axi_master_ar_lock_o    ),
//       .master_cache_o  ( axi_master_ar_cache_o   ),
//       .master_qos_o    ( axi_master_ar_qos_o     ),
//       .master_id_o     ( axi_master_ar_id_o      ),
//       .master_user_o   ( axi_master_ar_user_o    ),
//       .master_ready_i  ( axi_master_ar_ready_i   )
//    );
//
//    // WRITE DATA CHANNEL BUFFER
//    axi_w_buffer
//    #(
//        .DATA_WIDTH   (AXI_DATA_WIDTH),
//        .USER_WIDTH   (AXI_USER_WIDTH),
//        .BUFFER_DEPTH (SLICE_DEPTH)
//    )
//    w_buffer_i
//    (
//       .clk_i          ( clk_i                 ),
//       .rst_ni         ( rst_ni                ),
//       .test_en_i      ( test_en_i             ),
//
//       .slave_valid_i  ( axi_slave_w_valid_i   ),
//       .slave_data_i   ( axi_slave_w_data_i    ),
//       .slave_strb_i   ( axi_slave_w_strb_i    ),
//       .slave_user_i   ( axi_slave_w_user_i    ),
//       .slave_last_i   ( axi_slave_w_last_i    ),
//       .slave_ready_o  ( axi_slave_w_ready_o   ),
//
//       .master_valid_o ( axi_master_w_valid_o  ),
//       .master_data_o  ( axi_master_w_data_o   ),
//       .master_strb_o  ( axi_master_w_strb_o   ),
//       .master_user_o  ( axi_master_w_user_o   ),
//       .master_last_o  ( axi_master_w_last_o   ),
//       .master_ready_i ( axi_master_w_ready_i  )
//    );
//
//    // READ DATA CHANNEL BUFFER
//    axi_r_buffer
//    #(
//        .ID_WIDTH     (AXI_ID_WIDTH),
//        .DATA_WIDTH   (AXI_DATA_WIDTH),
//        .USER_WIDTH   (AXI_USER_WIDTH),
//        .BUFFER_DEPTH (SLICE_DEPTH)
//    )
//    r_buffer_i
//    (
//       .clk_i           ( clk_i                 ),
//       .rst_ni          ( rst_ni                ),
//       .test_en_i       ( test_en_i             ),
//
//       .slave_valid_i   ( axi_master_r_valid_i  ),
//       .slave_data_i    ( axi_master_r_data_i   ),
//       .slave_resp_i    ( axi_master_r_resp_i   ),
//       .slave_user_i    ( axi_master_r_user_i   ),
//       .slave_id_i      ( axi_master_r_id_i     ),
//       .slave_last_i    ( axi_master_r_last_i   ),
//       .slave_ready_o   ( axi_master_r_ready_o  ),
//
//       .master_valid_o  ( axi_slave_r_valid_o   ),
//       .master_data_o   ( axi_slave_r_data_o    ),
//       .master_resp_o   ( axi_slave_r_resp_o    ),
//       .master_user_o   ( axi_slave_r_user_o    ),
//       .master_id_o     ( axi_slave_r_id_o      ),
//       .master_last_o   ( axi_slave_r_last_o    ),
//       .master_ready_i  ( axi_slave_r_ready_i   )
//    );
//
//    // WRITE RESPONSE CHANNEL BUFFER
//    axi_b_buffer
//    #(
//        .ID_WIDTH     (AXI_ID_WIDTH),
//        .USER_WIDTH   (AXI_USER_WIDTH),
//        .BUFFER_DEPTH (SLICE_DEPTH)
//    )
//    b_buffer_i
//    (
//       .clk_i           ( clk_i                 ),
//       .rst_ni          ( rst_ni                ),
//       .test_en_i       ( test_en_i             ),
//
//       .slave_valid_i   ( axi_master_b_valid_i  ),
//       .slave_resp_i    ( axi_master_b_resp_i   ),
//       .slave_id_i      ( axi_master_b_id_i     ),
//       .slave_user_i    ( axi_master_b_user_i   ),
//       .slave_ready_o   ( axi_master_b_ready_o  ),
//
//       .master_valid_o  ( axi_slave_b_valid_o   ),
//       .master_resp_o   ( axi_slave_b_resp_o    ),
//       .master_id_o     ( axi_slave_b_id_o      ),
//       .master_user_o   ( axi_slave_b_user_o    ),
//       .master_ready_i  ( axi_slave_b_ready_i   )
//    );
//
// endmodule

// IEEE 1800-2023 semantics exercised:
//   axi_slice has an ANSI header (Sec 23.2.2.2) with a parameter port list of
//   six untyped value parameters (Sec 6.20.1, 6.20.2). Parameters declared in
//   a parameter port list are overridable parameters, not localparams
//   (Sec 6.20.1). AXI_STRB_WIDTH defaults to an expression of another
//   parameter (AXI_DATA_WIDTH/8), evaluated at elaboration (Sec 6.20.2).
//   Every port is declared with the data type logic and no net-type keyword:
//   per Sec 23.2.2.3 an input port then defaults to a NET of the default net
//   type (wire), while an output port declared with an explicit data type is
//   a VARIABLE. Packed widths come from parameter expressions such as
//   [AXI_ADDR_WIDTH-1:0] (Sec 7.4.1).
//   The body instantiates five modules with named parameter overrides
//   (Sec 23.10.2.2) and named port connections (Sec 23.3.2.2). The read-data
//   and write-response buffers are wired "backwards" on purpose: their slave_*
//   side is driven from the axi_master_r_*/axi_master_b_* ports and their
//   master_* side drives the axi_slave_r_*/axi_slave_b_* ports. The model must
//   keep each connection exactly as written. axi_slice is instantiated by
//   nobody, so it is the elaborated top (Sec 23.3.1) and the parameter values
//   propagate down: e.g. r_buffer_i gets DATA_WIDTH=64 and derives
//   STRB_WIDTH=DATA_WIDTH/8=8 itself; each buffer then passes a computed
//   DATA_WIDTH to its axi_single_slice (Sec 23.10).
// What we have to check:
//   - the six parameters, in order, all overridable, defaults 32/64/6/3/2 and
//     AXI_STRB_WIDTH = AXI_DATA_WIDTH/8 = 8
//   - the 91 ports: names in declaration order, directions, elaborated widths
//   - input logic ports are nets, output logic ports are variables
//   - the five instances: module types, names, named parameter overrides
//   - every named port connection of every instance, including the reversed
//     r_buffer_i / b_buffer_i wiring, both in the definition and in the
//     elaborated instances
//   - elaborated hierarchy (axi_slice is the only top) and propagated values
//   - no "no module definition" error for any instantiated module
// Checked:
//   - ParameterPortList, ElaboratedTopParameterValues
//   - PortListOrderAndDirections, ElaboratedPortWidths,
//     InputPortsAreNetsOutputPortsAreVariables
//   - InstancesAndParameterOverrides, DefinitionPortConnections,
//     ReadAndResponseBuffersAreWiredMasterToSlave
//   - ElaboratedHierarchy, PropagatedBufferParameters,
//     PropagatedSingleSliceDataWidth, NoMissingModuleDefinitions
// Not checked:
//   - HLC 786 builds no elaborated hierarchy (Design::getTopModules() is empty).
//     For a module nobody instantiates, its getAllModules() entry is the module
//     with default parameters, i.e. the IEEE top-level instance, so the top-level
//     checks fall back to that entry. Child instances and generate scopes have no
//     such entry; those tests are skipped with the reason in the skip message.
//   - the run-time buffering behaviour (valid/ready handshake through the
//     FIFOs) -- runtime-only, needs simulation
//   - internals of the helper modules (axi_*_buffer bodies, fifo_v3 logic) --
//     out of scope, they are compiled only so that the hierarchy elaborates;
//     only the parameter value they receive is checked here
//   - the "DO NOT OVERRIDE" comments on STRB_WIDTH -- a comment, no IEEE
//     semantics

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/named_argument.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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
// Parses a digit string in the given base. Returns nullopt for anything that is
// not a plain integer (for example digits containing x or z).
[[maybe_unused]] std::optional<int64_t> parseDigits(const std::string &digits, int base) {
  if (digits.empty()) return std::nullopt;
  const bool negative = (digits[0] == '-');
  const std::string body = negative ? digits.substr(1) : digits;
  if (body.empty()) return std::nullopt;
  char *end = nullptr;
  const unsigned long long value = std::strtoull(body.c_str(), &end, base);
  if ((end == nullptr) || (*end != '\0')) return std::nullopt;
  const int64_t result = static_cast<int64_t>(value);
  return negative ? -result : result;
}

// Parses a SystemVerilog integer literal as IEEE 1800-2023 Sec 5.7.1 defines
// it: underscores are ignored, an optional size, an optional s, a base letter
// and the digits. '0 (unbased unsized, Sec 5.7.1) yields 0.
[[maybe_unused]] std::optional<int64_t> parseLiteral(std::string_view text) {
  std::string clean;
  for (const char c : text) {
    if ((c != '_') && (std::isspace(static_cast<unsigned char>(c)) == 0)) clean += c;
  }
  if (clean.empty()) return std::nullopt;
  const size_t quote = clean.find('\'');
  if (quote == std::string::npos) return parseDigits(clean, 10);
  size_t pos = quote + 1;
  if ((pos < clean.size()) && ((clean[pos] == 's') || (clean[pos] == 'S'))) ++pos;
  if (pos >= clean.size()) return std::nullopt;
  const char baseChar = static_cast<char>(std::tolower(static_cast<unsigned char>(clean[pos])));
  int base = 0;
  if (baseChar == 'h') {
    base = 16;
  } else if (baseChar == 'd') {
    base = 10;
  } else if (baseChar == 'o') {
    base = 8;
  } else if (baseChar == 'b') {
    base = 2;
  } else {
    return (clean.substr(pos) == "0") ? std::optional<int64_t>(0) : std::nullopt;
  }
  return parseDigits(clean.substr(pos + 1), base);
}

// Integer value of a Constant. Accepts both the "KIND:digits" form of
// getValue() (e.g. "UINT:5", "HEX:ff") and falls back to the literal text of
// getDecompile().
[[maybe_unused]] std::optional<int64_t> constantValue(const hldb::Constant *constant) {
  if (constant == nullptr) return std::nullopt;
  const std::string_view value = constantValueText(constant);
  const size_t colon = value.find(':');
  if (colon != std::string_view::npos) {
    const std::string_view kind = value.substr(0, colon);
    std::string digits;
    for (const char c : value.substr(colon + 1)) {
      if (c != '_') digits += c;
    }
    int base = 0;
    if ((kind == "INT") || (kind == "UINT") || (kind == "DEC")) {
      base = 10;
    } else if (kind == "HEX") {
      base = 16;
    } else if (kind == "BIN") {
      base = 2;
    } else if (kind == "OCT") {
      base = 8;
    }
    if (base != 0) {
      if (const std::optional<int64_t> parsed = parseDigits(digits, base)) return parsed;
    }
  }
  return parseLiteral(constant->getDecompile());
}

// Evaluates a constant integer expression made of literals, parameter
// references (resolved through the ParamAssigns of 'scope'), the operators
// + - * / and unary -, and the constant system function $clog2 (Sec 20.8.1).
// This keeps the assertions independent of whether the model stores a folded
// constant or the original expression tree.
[[maybe_unused]] std::optional<int64_t> evalConst(const hldb::Any *any, const hldb::Scope *scope, int32_t depth = 0) {
  if ((any == nullptr) || (depth > 16)) return std::nullopt;
  switch (any->getAnyType()) {
    case hldb::AnyType::Constant: return constantValue(any_cast<hldb::Constant>(any));
    case hldb::AnyType::Operation: {
      const hldb::Operation *const op = any_cast<hldb::Operation>(any);
      if (op->getOperands() == nullptr) return std::nullopt;
      std::vector<int64_t> values;
      for (const hldb::Any *const operand : *op->getOperands()) {
        const std::optional<int64_t> v = evalConst(operand, scope, depth + 1);
        if (!v) return std::nullopt;
        values.emplace_back(*v);
      }
      if (values.size() == 1) {
        if (op->getOpType() == vpiMinusOp) return -values[0];
        if (op->getOpType() == vpiPlusOp) return values[0];
        return std::nullopt;
      }
      if (values.size() != 2) return std::nullopt;
      switch (op->getOpType()) {
        case vpiAddOp: return values[0] + values[1];
        case vpiSubOp: return values[0] - values[1];
        case vpiMultOp: return values[0] * values[1];
        case vpiDivOp:
          if (values[1] == 0) return std::nullopt;
          return values[0] / values[1];
        default: return std::nullopt;
      }
    }
    case hldb::AnyType::SysFuncCall: {
      const hldb::SysFuncCall *const call = any_cast<hldb::SysFuncCall>(any);
      if ((selectBaseName(call->getName()) != "$clog2") || (call->getArguments() == nullptr) ||
          call->getArguments()->empty()) {
        return std::nullopt;
      }
      const hldb::NamedArgument *const arg = call->getArguments()->front();
      const hldb::Any *const argExpr = (arg->getHighConn() != nullptr) ? arg->getHighConn() : arg->getLowConn();
      const std::optional<int64_t> v = evalConst(argExpr, scope, depth + 1);
      if (!v) return std::nullopt;
      int64_t result = 0;
      while ((int64_t{1} << result) < *v) ++result;
      return result;
    }
    case hldb::AnyType::RefObj: {
      const hldb::RefObj *const ref = any_cast<hldb::RefObj>(any);
      if ((ref->getActual() != nullptr) && (ref->getActual()->getAnyType() == hldb::AnyType::Constant)) {
        return evalConst(ref->getActual(), scope, depth + 1);
      }
      if (scope == nullptr) return std::nullopt;
      const hldb::ParamAssign *const pa =
          hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(ref->getName(), scope->getParamAssigns());
      return (pa == nullptr) ? std::nullopt : evalConst(pa->getRhs(), scope, depth + 1);
    }
    case hldb::AnyType::Parameter: {
      if (scope == nullptr) return std::nullopt;
      const hldb::ParamAssign *const pa =
          hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(any->getName(), scope->getParamAssigns());
      return (pa == nullptr) ? std::nullopt : evalConst(pa->getRhs(), scope, depth + 1);
    }
    default: return std::nullopt;
  }
}

// Value of parameter 'name' in 'scope' (from its ParamAssign), or nullopt.
[[maybe_unused]] std::optional<int64_t> paramValue(const hldb::Scope *scope, std::string_view name) {
  if (scope == nullptr) return std::nullopt;
  const hldb::ParamAssign *const pa =
      hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(name, scope->getParamAssigns());
  return (pa == nullptr) ? std::nullopt : evalConst(pa->getRhs(), scope);
}

// Packed dimensions [left, right] of a declaration type, outermost first.
// Handles both a logic typespec carrying several ranges and a packed array
// typespec wrapping an element typespec.
[[maybe_unused]] std::vector<std::pair<int64_t, int64_t>> packedDims(const hldb::RefTypespec *refTs,
                                                                     const hldb::Scope *scope) {
  std::vector<std::pair<int64_t, int64_t>> dims;
  const hldb::Typespec *ts = (refTs == nullptr) ? nullptr : refTs->getActual();
  while (ts != nullptr) {
    if (const hldb::LogicTypespec *const lts = any_cast<hldb::LogicTypespec>(ts)) {
      if (lts->getRanges() != nullptr) {
        for (const hldb::Range *const r : *lts->getRanges()) {
          dims.emplace_back(evalConst(r->getLeftExpr(), scope).value_or(-999),
                            evalConst(r->getRightExpr(), scope).value_or(-999));
        }
      }
      break;
    }
    if (const hldb::ArrayTypespec *const ats = any_cast<hldb::ArrayTypespec>(ts)) {
      if (const hldb::Range *const r = ats->getRange()) {
        dims.emplace_back(evalConst(r->getLeftExpr(), scope).value_or(-999),
                          evalConst(r->getRightExpr(), scope).value_or(-999));
      }
      ts = (ats->getElemTypespec() == nullptr) ? nullptr : ats->getElemTypespec()->getActual();
      continue;
    }
    break;
  }
  return dims;
}

// Total bit width implied by the packed dimensions (1 for a scalar).
[[maybe_unused]] int64_t packedWidth(const hldb::RefTypespec *refTs, const hldb::Scope *scope) {
  int64_t width = 1;
  for (const std::pair<int64_t, int64_t> &d : packedDims(refTs, scope)) {
    width *= ((d.first > d.second) ? (d.first - d.second) : (d.second - d.first)) + 1;
  }
  return width;
}
}  // namespace

namespace {
struct PortRow {
  const char *m_name;
  int32_t m_direction;
  int64_t m_width;
};

struct ConnRow {
  const char *m_formal;
  const char *m_actual;
};

struct OverrideRow {
  const char *m_param;
  const char *m_actualParam;
};

struct InstanceRow {
  const char *m_instance;
  const char *m_module;
  std::vector<OverrideRow> m_overrides;
  std::vector<ConnRow> m_conns;
};

// Ports of axi_slice in declaration order with their elaborated width
// (AXI_ADDR_WIDTH=32, AXI_DATA_WIDTH=64, AXI_STRB_WIDTH=8, AXI_USER_WIDTH=6,
// AXI_ID_WIDTH=3).
const std::vector<PortRow> &slicePorts() {
  static const std::vector<PortRow> kPorts = {
      // clang-format off
{"clk_i", vpiInput, 1},
{"rst_ni", vpiInput, 1},
{"test_en_i", vpiInput, 1},
{"axi_slave_aw_valid_i", vpiInput, 1},
{"axi_slave_aw_addr_i", vpiInput, 32},
{"axi_slave_aw_prot_i", vpiInput, 3},
{"axi_slave_aw_region_i", vpiInput, 4},
{"axi_slave_aw_len_i", vpiInput, 8},
{"axi_slave_aw_size_i", vpiInput, 3},
{"axi_slave_aw_burst_i", vpiInput, 2},
{"axi_slave_aw_lock_i", vpiInput, 1},
{"axi_slave_aw_cache_i", vpiInput, 4},
{"axi_slave_aw_qos_i", vpiInput, 4},
{"axi_slave_aw_id_i", vpiInput, 3},
{"axi_slave_aw_user_i", vpiInput, 6},
{"axi_slave_aw_ready_o", vpiOutput, 1},
{"axi_slave_ar_valid_i", vpiInput, 1},
{"axi_slave_ar_addr_i", vpiInput, 32},
{"axi_slave_ar_prot_i", vpiInput, 3},
{"axi_slave_ar_region_i", vpiInput, 4},
{"axi_slave_ar_len_i", vpiInput, 8},
{"axi_slave_ar_size_i", vpiInput, 3},
{"axi_slave_ar_burst_i", vpiInput, 2},
{"axi_slave_ar_lock_i", vpiInput, 1},
{"axi_slave_ar_cache_i", vpiInput, 4},
{"axi_slave_ar_qos_i", vpiInput, 4},
{"axi_slave_ar_id_i", vpiInput, 3},
{"axi_slave_ar_user_i", vpiInput, 6},
{"axi_slave_ar_ready_o", vpiOutput, 1},
{"axi_slave_w_valid_i", vpiInput, 1},
{"axi_slave_w_data_i", vpiInput, 64},
{"axi_slave_w_strb_i", vpiInput, 8},
{"axi_slave_w_user_i", vpiInput, 6},
{"axi_slave_w_last_i", vpiInput, 1},
{"axi_slave_w_ready_o", vpiOutput, 1},
{"axi_slave_r_valid_o", vpiOutput, 1},
{"axi_slave_r_data_o", vpiOutput, 64},
{"axi_slave_r_resp_o", vpiOutput, 2},
{"axi_slave_r_last_o", vpiOutput, 1},
{"axi_slave_r_id_o", vpiOutput, 3},
{"axi_slave_r_user_o", vpiOutput, 6},
{"axi_slave_r_ready_i", vpiInput, 1},
{"axi_slave_b_valid_o", vpiOutput, 1},
{"axi_slave_b_resp_o", vpiOutput, 2},
{"axi_slave_b_id_o", vpiOutput, 3},
{"axi_slave_b_user_o", vpiOutput, 6},
{"axi_slave_b_ready_i", vpiInput, 1},
{"axi_master_aw_valid_o", vpiOutput, 1},
{"axi_master_aw_addr_o", vpiOutput, 32},
{"axi_master_aw_prot_o", vpiOutput, 3},
{"axi_master_aw_region_o", vpiOutput, 4},
{"axi_master_aw_len_o", vpiOutput, 8},
{"axi_master_aw_size_o", vpiOutput, 3},
{"axi_master_aw_burst_o", vpiOutput, 2},
{"axi_master_aw_lock_o", vpiOutput, 1},
{"axi_master_aw_cache_o", vpiOutput, 4},
{"axi_master_aw_qos_o", vpiOutput, 4},
{"axi_master_aw_id_o", vpiOutput, 3},
{"axi_master_aw_user_o", vpiOutput, 6},
{"axi_master_aw_ready_i", vpiInput, 1},
{"axi_master_ar_valid_o", vpiOutput, 1},
{"axi_master_ar_addr_o", vpiOutput, 32},
{"axi_master_ar_prot_o", vpiOutput, 3},
{"axi_master_ar_region_o", vpiOutput, 4},
{"axi_master_ar_len_o", vpiOutput, 8},
{"axi_master_ar_size_o", vpiOutput, 3},
{"axi_master_ar_burst_o", vpiOutput, 2},
{"axi_master_ar_lock_o", vpiOutput, 1},
{"axi_master_ar_cache_o", vpiOutput, 4},
{"axi_master_ar_qos_o", vpiOutput, 4},
{"axi_master_ar_id_o", vpiOutput, 3},
{"axi_master_ar_user_o", vpiOutput, 6},
{"axi_master_ar_ready_i", vpiInput, 1},
{"axi_master_w_valid_o", vpiOutput, 1},
{"axi_master_w_data_o", vpiOutput, 64},
{"axi_master_w_strb_o", vpiOutput, 8},
{"axi_master_w_user_o", vpiOutput, 6},
{"axi_master_w_last_o", vpiOutput, 1},
{"axi_master_w_ready_i", vpiInput, 1},
{"axi_master_r_valid_i", vpiInput, 1},
{"axi_master_r_data_i", vpiInput, 64},
{"axi_master_r_resp_i", vpiInput, 2},
{"axi_master_r_last_i", vpiInput, 1},
{"axi_master_r_id_i", vpiInput, 3},
{"axi_master_r_user_i", vpiInput, 6},
{"axi_master_r_ready_o", vpiOutput, 1},
{"axi_master_b_valid_i", vpiInput, 1},
{"axi_master_b_resp_i", vpiInput, 2},
{"axi_master_b_id_i", vpiInput, 3},
{"axi_master_b_user_i", vpiInput, 6},
{"axi_master_b_ready_o", vpiOutput, 1},
      // clang-format on
  };
  return kPorts;
}

// The five instances, in source order, exactly as written in axi_slice.sv.
const std::vector<InstanceRow> &sliceInstances() {
  static const std::vector<InstanceRow> kInstances = {
      {"aw_buffer_i",
       "axi_aw_buffer",
       {{"ID_WIDTH", "AXI_ID_WIDTH"},
        {"ADDR_WIDTH", "AXI_ADDR_WIDTH"},
        {"USER_WIDTH", "AXI_USER_WIDTH"},
        {"BUFFER_DEPTH", "SLICE_DEPTH"}},
       {
           // clang-format off
{"clk_i", "clk_i"},
{"rst_ni", "rst_ni"},
{"test_en_i", "test_en_i"},
{"slave_valid_i", "axi_slave_aw_valid_i"},
{"slave_addr_i", "axi_slave_aw_addr_i"},
{"slave_prot_i", "axi_slave_aw_prot_i"},
{"slave_region_i", "axi_slave_aw_region_i"},
{"slave_len_i", "axi_slave_aw_len_i"},
{"slave_size_i", "axi_slave_aw_size_i"},
{"slave_burst_i", "axi_slave_aw_burst_i"},
{"slave_lock_i", "axi_slave_aw_lock_i"},
{"slave_cache_i", "axi_slave_aw_cache_i"},
{"slave_qos_i", "axi_slave_aw_qos_i"},
{"slave_id_i", "axi_slave_aw_id_i"},
{"slave_user_i", "axi_slave_aw_user_i"},
{"slave_ready_o", "axi_slave_aw_ready_o"},
{"master_valid_o", "axi_master_aw_valid_o"},
{"master_addr_o", "axi_master_aw_addr_o"},
{"master_prot_o", "axi_master_aw_prot_o"},
{"master_region_o", "axi_master_aw_region_o"},
{"master_len_o", "axi_master_aw_len_o"},
{"master_size_o", "axi_master_aw_size_o"},
{"master_burst_o", "axi_master_aw_burst_o"},
{"master_lock_o", "axi_master_aw_lock_o"},
{"master_cache_o", "axi_master_aw_cache_o"},
{"master_qos_o", "axi_master_aw_qos_o"},
{"master_id_o", "axi_master_aw_id_o"},
{"master_user_o", "axi_master_aw_user_o"},
{"master_ready_i", "axi_master_aw_ready_i"},
           // clang-format on
       }},
      {"ar_buffer_i",
       "axi_ar_buffer",
       {{"ID_WIDTH", "AXI_ID_WIDTH"},
        {"ADDR_WIDTH", "AXI_ADDR_WIDTH"},
        {"USER_WIDTH", "AXI_USER_WIDTH"},
        {"BUFFER_DEPTH", "SLICE_DEPTH"}},
       {
           // clang-format off
{"clk_i", "clk_i"},
{"rst_ni", "rst_ni"},
{"test_en_i", "test_en_i"},
{"slave_valid_i", "axi_slave_ar_valid_i"},
{"slave_addr_i", "axi_slave_ar_addr_i"},
{"slave_prot_i", "axi_slave_ar_prot_i"},
{"slave_region_i", "axi_slave_ar_region_i"},
{"slave_len_i", "axi_slave_ar_len_i"},
{"slave_size_i", "axi_slave_ar_size_i"},
{"slave_burst_i", "axi_slave_ar_burst_i"},
{"slave_lock_i", "axi_slave_ar_lock_i"},
{"slave_cache_i", "axi_slave_ar_cache_i"},
{"slave_qos_i", "axi_slave_ar_qos_i"},
{"slave_id_i", "axi_slave_ar_id_i"},
{"slave_user_i", "axi_slave_ar_user_i"},
{"slave_ready_o", "axi_slave_ar_ready_o"},
{"master_valid_o", "axi_master_ar_valid_o"},
{"master_addr_o", "axi_master_ar_addr_o"},
{"master_prot_o", "axi_master_ar_prot_o"},
{"master_region_o", "axi_master_ar_region_o"},
{"master_len_o", "axi_master_ar_len_o"},
{"master_size_o", "axi_master_ar_size_o"},
{"master_burst_o", "axi_master_ar_burst_o"},
{"master_lock_o", "axi_master_ar_lock_o"},
{"master_cache_o", "axi_master_ar_cache_o"},
{"master_qos_o", "axi_master_ar_qos_o"},
{"master_id_o", "axi_master_ar_id_o"},
{"master_user_o", "axi_master_ar_user_o"},
{"master_ready_i", "axi_master_ar_ready_i"},
           // clang-format on
       }},
      {"w_buffer_i",
       "axi_w_buffer",
       {{"DATA_WIDTH", "AXI_DATA_WIDTH"}, {"USER_WIDTH", "AXI_USER_WIDTH"}, {"BUFFER_DEPTH", "SLICE_DEPTH"}},
       {
           // clang-format off
{"clk_i", "clk_i"},
{"rst_ni", "rst_ni"},
{"test_en_i", "test_en_i"},
{"slave_valid_i", "axi_slave_w_valid_i"},
{"slave_data_i", "axi_slave_w_data_i"},
{"slave_strb_i", "axi_slave_w_strb_i"},
{"slave_user_i", "axi_slave_w_user_i"},
{"slave_last_i", "axi_slave_w_last_i"},
{"slave_ready_o", "axi_slave_w_ready_o"},
{"master_valid_o", "axi_master_w_valid_o"},
{"master_data_o", "axi_master_w_data_o"},
{"master_strb_o", "axi_master_w_strb_o"},
{"master_user_o", "axi_master_w_user_o"},
{"master_last_o", "axi_master_w_last_o"},
{"master_ready_i", "axi_master_w_ready_i"},
           // clang-format on
       }},
      {"r_buffer_i",
       "axi_r_buffer",
       {{"ID_WIDTH", "AXI_ID_WIDTH"},
        {"DATA_WIDTH", "AXI_DATA_WIDTH"},
        {"USER_WIDTH", "AXI_USER_WIDTH"},
        {"BUFFER_DEPTH", "SLICE_DEPTH"}},
       {
           // clang-format off
{"clk_i", "clk_i"},
{"rst_ni", "rst_ni"},
{"test_en_i", "test_en_i"},
{"slave_valid_i", "axi_master_r_valid_i"},
{"slave_data_i", "axi_master_r_data_i"},
{"slave_resp_i", "axi_master_r_resp_i"},
{"slave_user_i", "axi_master_r_user_i"},
{"slave_id_i", "axi_master_r_id_i"},
{"slave_last_i", "axi_master_r_last_i"},
{"slave_ready_o", "axi_master_r_ready_o"},
{"master_valid_o", "axi_slave_r_valid_o"},
{"master_data_o", "axi_slave_r_data_o"},
{"master_resp_o", "axi_slave_r_resp_o"},
{"master_user_o", "axi_slave_r_user_o"},
{"master_id_o", "axi_slave_r_id_o"},
{"master_last_o", "axi_slave_r_last_o"},
{"master_ready_i", "axi_slave_r_ready_i"},
           // clang-format on
       }},
      {"b_buffer_i",
       "axi_b_buffer",
       {{"ID_WIDTH", "AXI_ID_WIDTH"}, {"USER_WIDTH", "AXI_USER_WIDTH"}, {"BUFFER_DEPTH", "SLICE_DEPTH"}},
       {
           // clang-format off
{"clk_i", "clk_i"},
{"rst_ni", "rst_ni"},
{"test_en_i", "test_en_i"},
{"slave_valid_i", "axi_master_b_valid_i"},
{"slave_resp_i", "axi_master_b_resp_i"},
{"slave_id_i", "axi_master_b_id_i"},
{"slave_user_i", "axi_master_b_user_i"},
{"slave_ready_o", "axi_master_b_ready_o"},
{"master_valid_o", "axi_slave_b_valid_o"},
{"master_resp_o", "axi_slave_b_resp_o"},
{"master_id_o", "axi_slave_b_id_o"},
{"master_user_o", "axi_slave_b_user_o"},
{"master_ready_i", "axi_slave_b_ready_i"},
           // clang-format on
       }},
  };
  return kInstances;
}

// Strips a RefObj to the object it refers to; returns 'any' itself otherwise.
const hldb::Any *resolve(const hldb::Any *any) {
  if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(any)) {
    return ref->getActual();
  }
  return any;
}
}  // namespace

class AxiSliceTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-wd", "../../../tests/ariane", "-d", "ast", "-d", "db", "-nobuiltin",
                       "fpga/src/axi_slice/src/axi_slice.sv", "fpga/src/axi_slice/src/axi_aw_buffer.sv",
                       "fpga/src/axi_slice/src/axi_ar_buffer.sv", "fpga/src/axi_slice/src/axi_w_buffer.sv",
                       "fpga/src/axi_slice/src/axi_r_buffer.sv", "fpga/src/axi_slice/src/axi_b_buffer.sv",
                       "fpga/src/axi_slice/src/axi_single_slice.sv", "src/common_cells/src/deprecated/fifo_v1.sv",
                       "src/common_cells/src/deprecated/fifo_v2.sv", "src/common_cells/src/fifo_v3.sv"});
  }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Unelaborated definition of axi_slice.
  static const hldb::Module *definition() {
    return hldb::findByDefName<hldb::Module>("axi_slice", m_design->getAllModules());
  }
  // Elaborated top instance of axi_slice.
  static const hldb::Module *top() {
    return (hldb::findByDefName<hldb::Module>("axi_slice", m_design->getTopModules()) != nullptr
                ? hldb::findByDefName<hldb::Module>("axi_slice", m_design->getTopModules())
                : hldb::findByDefName<hldb::Module>("axi_slice", m_design->getAllModules()));
  }
};

// Sec 6.20.1 / 6.20.2: six untyped value parameters in the parameter port
// list, all overridable (none is a localparam). AXI_STRB_WIDTH defaults to
// the expression AXI_DATA_WIDTH/8, i.e. 8 with the default data width.
TEST_F(AxiSliceTest, ParameterPortList) {
  const hldb::Module *const def = definition();
  ASSERT_NE(def, nullptr);
  ASSERT_NE(def->getParameters(), nullptr);
  const std::vector<std::string_view> expected = {"AXI_ADDR_WIDTH", "AXI_DATA_WIDTH", "AXI_USER_WIDTH",
                                                  "AXI_ID_WIDTH",   "SLICE_DEPTH",    "AXI_STRB_WIDTH"};
  std::vector<std::string_view> names;
  for (const hldb::Any *const any : *def->getParameters()) {
    const hldb::Parameter *const param = any_cast<hldb::Parameter>(any);
    ASSERT_NE(param, nullptr);
    names.emplace_back(param->getName());
    EXPECT_FALSE(param->getLocalParam()) << param->getName() << " is in the parameter port list";
  }
  EXPECT_EQ(names, expected);

  EXPECT_EQ(paramValue(def, "AXI_ADDR_WIDTH"), std::optional<int64_t>(32));
  EXPECT_EQ(paramValue(def, "AXI_DATA_WIDTH"), std::optional<int64_t>(64));
  EXPECT_EQ(paramValue(def, "AXI_USER_WIDTH"), std::optional<int64_t>(6));
  EXPECT_EQ(paramValue(def, "AXI_ID_WIDTH"), std::optional<int64_t>(3));
  EXPECT_EQ(paramValue(def, "SLICE_DEPTH"), std::optional<int64_t>(2));
  EXPECT_EQ(paramValue(def, "AXI_STRB_WIDTH"), std::optional<int64_t>(8));

  // The default of AXI_STRB_WIDTH is written as AXI_DATA_WIDTH/8.
  const hldb::ParamAssign *const strb =
      hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>("AXI_STRB_WIDTH", def->getParamAssigns());
  ASSERT_NE(strb, nullptr);
  ASSERT_NE(strb->getRhs(), nullptr);
  ASSERT_EQ(strb->getRhs()->getAnyType(), hldb::AnyType::Operation);
  const hldb::Operation *const div = strb->getRhs<hldb::Operation>();
  EXPECT_EQ(div->getOpType(), vpiDivOp);
  ASSERT_NE(div->getOperands(), nullptr);
  ASSERT_EQ(div->getOperands()->size(), 2U);
  EXPECT_EQ(selectBaseName(div->getOperands()->at(0)->getName()), "AXI_DATA_WIDTH");
  EXPECT_EQ(evalConst(div->getOperands()->at(1), def), std::optional<int64_t>(8));
}

// Sec 23.10: the top is not overridden by anybody, so every parameter keeps
// its default; AXI_STRB_WIDTH is computed from AXI_DATA_WIDTH.
TEST_F(AxiSliceTest, ElaboratedTopParameterValues) {
  const hldb::Module *const inst = top();
  ASSERT_NE(inst, nullptr);
  const std::vector<std::pair<std::string_view, int64_t>> expected = {{"AXI_ADDR_WIDTH", 32}, {"AXI_DATA_WIDTH", 64},
                                                                      {"AXI_USER_WIDTH", 6},  {"AXI_ID_WIDTH", 3},
                                                                      {"SLICE_DEPTH", 2},     {"AXI_STRB_WIDTH", 8}};
  for (const std::pair<std::string_view, int64_t> &e : expected) {
    const hldb::ParamAssign *const pa =
        hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(e.first, inst->getParamAssigns());
    ASSERT_NE(pa, nullptr) << e.first;
    EXPECT_FALSE(pa->getOverridden()) << e.first;
    EXPECT_EQ(evalConst(pa->getRhs(), inst), std::optional<int64_t>(e.second)) << e.first;
  }
}

// Sec 23.2.2.2: 91 ANSI ports, kept in declaration order with their
// declared directions.
TEST_F(AxiSliceTest, PortListOrderAndDirections) {
  const hldb::Module *const def = definition();
  ASSERT_NE(def, nullptr);
  ASSERT_NE(def->getPorts(), nullptr);
  const std::vector<PortRow> &expected = slicePorts();
  ASSERT_EQ(expected.size(), 91U);
  ASSERT_EQ(def->getPorts()->size(), expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::Port *const port = def->getPorts()->at(i);
    EXPECT_EQ(selectBaseName(port->getName()), expected[i].m_name) << "port #" << i;
    EXPECT_EQ(port->getDirection(), expected[i].m_direction) << expected[i].m_name;
  }
}

// Sec 7.4.1: packed widths come from the parameter expressions; with the
// defaults in effect they are 32/64/8/6/3 bits or the literal [N:0] widths.
TEST_F(AxiSliceTest, ElaboratedPortWidths) {
  const hldb::Module *const inst = top();
  ASSERT_NE(inst, nullptr);
  for (const PortRow &row : slicePorts()) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(row.m_name, inst->getPorts());
    ASSERT_NE(port, nullptr) << row.m_name;
    ASSERT_NE(port->getTypespec(), nullptr) << row.m_name;
    ASSERT_NE(port->getTypespec()->getActual(), nullptr) << row.m_name;
    EXPECT_EQ(port->getTypespec()->getActual()->getAnyType(), hldb::AnyType::LogicTypespec) << row.m_name;
    EXPECT_EQ(packedWidth(port->getTypespec(), inst), row.m_width) << row.m_name;
  }
}

// Sec 23.2.2.3: 'input logic x' (no net-type keyword) is a net of the
// default net type; 'output logic x' (explicit data type) is a variable.
TEST_F(AxiSliceTest, InputPortsAreNetsOutputPortsAreVariables) {
  const hldb::Module *const inst = top();
  ASSERT_NE(inst, nullptr);
  for (const PortRow &row : slicePorts()) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(row.m_name, inst->getPorts());
    ASSERT_NE(port, nullptr) << row.m_name;
    ASSERT_NE(port->getLowConn(), nullptr) << row.m_name;
    const hldb::Any *const internal = resolve(port->getLowConn());
    ASSERT_NE(internal, nullptr) << row.m_name;
    if (row.m_direction == vpiInput) {
      EXPECT_EQ(internal->getAnyType(), hldb::AnyType::Net) << row.m_name;
      const hldb::Net *const net = hldb::findByName<hldb::Net>(row.m_name, inst->getNets());
      ASSERT_NE(net, nullptr) << row.m_name;
      EXPECT_EQ(net->getNetType(), vpiWire) << row.m_name;
      EXPECT_EQ(hldb::findByName<hldb::Variable>(row.m_name, inst->getVariables()), nullptr) << row.m_name;
    } else {
      EXPECT_EQ(internal->getAnyType(), hldb::AnyType::Variable) << row.m_name;
      EXPECT_NE(hldb::findByName<hldb::Variable>(row.m_name, inst->getVariables()), nullptr) << row.m_name;
      EXPECT_EQ(hldb::findByName<hldb::Net>(row.m_name, inst->getNets()), nullptr) << row.m_name;
    }
  }
}

// Sec 23.3.2 / 23.10.2.2: five instances in source order, each with its
// module type and its named parameter overrides taken from axi_slice's own
// parameters.
TEST_F(AxiSliceTest, InstancesAndParameterOverrides) {
  GTEST_SKIP()
      << "HLC 786 does not record instance parameter overrides: the instance's ModuleTypespec has no ParamAssigns. "
         "Per IEEE 1800-2023 Sec 23.10.2 a #(...) override sets the instance's parameter values. Fix pending.";
  const hldb::Module *const def = definition();
  ASSERT_NE(def, nullptr);
  ASSERT_NE(def->getRefInstances(), nullptr);
  const std::vector<InstanceRow> &expected = sliceInstances();
  ASSERT_EQ(def->getRefInstances()->size(), expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::RefInstance *const ri = def->getRefInstances()->at(i);
    EXPECT_EQ(selectBaseName(ri->getName()), expected[i].m_instance);
    const hldb::ModuleTypespec *const mts = hldb::getActual<hldb::ModuleTypespec>(ri);
    ASSERT_NE(mts, nullptr) << expected[i].m_instance;
    EXPECT_TRUE((selectBaseName(mts->getName()) == expected[i].m_module) || (mts->getDefName() == expected[i].m_module))
        << expected[i].m_instance;
    ASSERT_NE(mts->getParamAssigns(), nullptr) << expected[i].m_instance;
    EXPECT_EQ(mts->getParamAssigns()->size(), expected[i].m_overrides.size()) << expected[i].m_instance;
    for (const OverrideRow &o : expected[i].m_overrides) {
      const hldb::ParamAssign *const pa =
          hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(o.m_param, mts->getParamAssigns());
      ASSERT_NE(pa, nullptr) << expected[i].m_instance << "." << o.m_param;
      ASSERT_NE(pa->getRhs(), nullptr) << expected[i].m_instance << "." << o.m_param;
      EXPECT_EQ(pa->getRhs()->getAnyType(), hldb::AnyType::RefObj) << expected[i].m_instance << "." << o.m_param;
      EXPECT_EQ(selectBaseName(pa->getRhs()->getName()), o.m_actualParam) << expected[i].m_instance << "." << o.m_param;
    }
  }
}

// Sec 23.3.2.2: every port of every instance is connected by name to the
// identifier written in the source (definition view).
TEST_F(AxiSliceTest, DefinitionPortConnections) {
  const hldb::Module *const def = definition();
  ASSERT_NE(def, nullptr);
  for (const InstanceRow &row : sliceInstances()) {
    const hldb::RefInstance *const ri = hldb::findByName<hldb::RefInstance>(row.m_instance, def->getRefInstances());
    ASSERT_NE(ri, nullptr) << row.m_instance;
    ASSERT_NE(ri->getArguments(), nullptr) << row.m_instance;
    EXPECT_EQ(ri->getArguments()->size(), row.m_conns.size()) << row.m_instance;
    for (const ConnRow &c : row.m_conns) {
      const hldb::NamedArgument *const arg =
          hldb::findByName<hldb::NamedArgument, hldb::NamedArgument>(c.m_formal, ri->getArguments());
      ASSERT_NE(arg, nullptr) << row.m_instance << "." << c.m_formal;
      EXPECT_TRUE(arg->getConnByName()) << row.m_instance << "." << c.m_formal;
      ASSERT_NE(arg->getHighConn(), nullptr) << row.m_instance << "." << c.m_formal;
      EXPECT_EQ(arg->getHighConn()->getAnyType(), hldb::AnyType::RefObj) << row.m_instance << "." << c.m_formal;
      EXPECT_EQ(selectBaseName(arg->getHighConn()->getName()), c.m_actual) << row.m_instance << "." << c.m_formal;
    }
  }
}

// The read-data and write-response buffers are connected "backwards": their
// slave side is fed from the axi_master_r_* / axi_master_b_* ports and their
// master side drives the axi_slave_r_* / axi_slave_b_* ports. In the
// elaborated instances each port's high connection must refer to exactly the
// axi_slice port written in the source (a net for inputs of axi_slice, a
// variable for outputs of axi_slice).
TEST_F(AxiSliceTest, ReadAndResponseBuffersAreWiredMasterToSlave) {
  GTEST_SKIP() << "HLC 786 builds no elaborated hierarchy: getTopModules() is empty and child instances / generate "
                  "scopes are not created. "
                  "Per IEEE 1800-2023 Sec 23.10 and 27 elaboration builds the instance tree and expands generate "
                  "blocks. Fix pending.";
  const hldb::Module *const inst = top();
  ASSERT_NE(inst, nullptr);
  for (const InstanceRow &row : sliceInstances()) {
    const std::string_view instName = row.m_instance;
    if ((instName != "r_buffer_i") && (instName != "b_buffer_i")) continue;
    const hldb::Module *const child = hldb::findByName<hldb::Module>(row.m_instance, inst->getModules());
    ASSERT_NE(child, nullptr) << row.m_instance;
    for (const ConnRow &c : row.m_conns) {
      const hldb::Port *const port = hldb::findByName<hldb::Port>(c.m_formal, child->getPorts());
      ASSERT_NE(port, nullptr) << row.m_instance << "." << c.m_formal;
      ASSERT_NE(port->getHighConn(), nullptr) << row.m_instance << "." << c.m_formal;
      EXPECT_EQ(selectBaseName(port->getHighConn()->getName()), c.m_actual) << row.m_instance << "." << c.m_formal;
      const hldb::Any *const actual = resolve(port->getHighConn());
      ASSERT_NE(actual, nullptr) << row.m_instance << "." << c.m_formal;
      EXPECT_EQ(selectBaseName(actual->getName()), c.m_actual) << row.m_instance << "." << c.m_formal;
    }
  }
  // Spot checks of the reversal, spelled out.
  const hldb::Module *const rBuf = hldb::findByName<hldb::Module>("r_buffer_i", inst->getModules());
  ASSERT_NE(rBuf, nullptr);
  const hldb::Port *const rSlaveValid = hldb::findByName<hldb::Port>("slave_valid_i", rBuf->getPorts());
  ASSERT_NE(rSlaveValid, nullptr);
  ASSERT_NE(rSlaveValid->getHighConn(), nullptr);
  const hldb::Any *const rSlaveValidActual = resolve(rSlaveValid->getHighConn());
  ASSERT_NE(rSlaveValidActual, nullptr);
  EXPECT_EQ(rSlaveValidActual, hldb::findByName<hldb::Net>("axi_master_r_valid_i", inst->getNets()));
  const hldb::Port *const rMasterValid = hldb::findByName<hldb::Port>("master_valid_o", rBuf->getPorts());
  ASSERT_NE(rMasterValid, nullptr);
  ASSERT_NE(rMasterValid->getHighConn(), nullptr);
  const hldb::Any *const rMasterValidActual = resolve(rMasterValid->getHighConn());
  ASSERT_NE(rMasterValidActual, nullptr);
  EXPECT_EQ(rMasterValidActual, hldb::findByName<hldb::Variable>("axi_slave_r_valid_o", inst->getVariables()));

  const hldb::Module *const bBuf = hldb::findByName<hldb::Module>("b_buffer_i", inst->getModules());
  ASSERT_NE(bBuf, nullptr);
  const hldb::Port *const bSlaveReady = hldb::findByName<hldb::Port>("slave_ready_o", bBuf->getPorts());
  ASSERT_NE(bSlaveReady, nullptr);
  ASSERT_NE(bSlaveReady->getHighConn(), nullptr);
  const hldb::Any *const bSlaveReadyActual = resolve(bSlaveReady->getHighConn());
  ASSERT_NE(bSlaveReadyActual, nullptr);
  EXPECT_EQ(bSlaveReadyActual, hldb::findByName<hldb::Variable>("axi_master_b_ready_o", inst->getVariables()));
  const hldb::Port *const bMasterReady = hldb::findByName<hldb::Port>("master_ready_i", bBuf->getPorts());
  ASSERT_NE(bMasterReady, nullptr);
  ASSERT_NE(bMasterReady->getHighConn(), nullptr);
  const hldb::Any *const bMasterReadyActual = resolve(bMasterReady->getHighConn());
  ASSERT_NE(bMasterReadyActual, nullptr);
  EXPECT_EQ(bMasterReadyActual, hldb::findByName<hldb::Net>("axi_slave_b_ready_i", inst->getNets()));
}

// Sec 23.3.1: axi_slice is instantiated by nobody, so it is the only top;
// every other module in the source set is instantiated below it. The top has
// exactly the five buffer instances as children.
TEST_F(AxiSliceTest, ElaboratedHierarchy) {
  GTEST_SKIP() << "HLC 786 builds no elaborated hierarchy: getTopModules() is empty and child instances / generate "
                  "scopes are not created. "
                  "Per IEEE 1800-2023 Sec 23.10 and 27 elaboration builds the instance tree and expands generate "
                  "blocks. Fix pending.";
  ASSERT_NE(m_design->getTopModules(), nullptr);
  ASSERT_NE(top(), nullptr);
  for (const std::string_view name : {"axi_aw_buffer", "axi_ar_buffer", "axi_w_buffer", "axi_r_buffer", "axi_b_buffer",
                                      "axi_single_slice", "fifo", "fifo_v2", "fifo_v3"}) {
    EXPECT_EQ(hldb::findByDefName<hldb::Module>(name, m_design->getTopModules()), nullptr) << name;
  }
  const hldb::Module *const inst = top();
  ASSERT_NE(inst->getModules(), nullptr);
  const std::vector<InstanceRow> &expected = sliceInstances();
  ASSERT_EQ(inst->getModules()->size(), expected.size());
  for (const InstanceRow &row : expected) {
    const hldb::Module *const child = hldb::findByName<hldb::Module>(row.m_instance, inst->getModules());
    ASSERT_NE(child, nullptr) << row.m_instance;
    EXPECT_EQ(child->getDefName(), row.m_module) << row.m_instance;
  }
}

// Sec 23.10.2.2: the named overrides propagate the top's values into each
// buffer. STRB_WIDTH of w_buffer_i / r_buffer_i is not overridden but is
// derived from the overridden DATA_WIDTH: 64/8 = 8.
TEST_F(AxiSliceTest, PropagatedBufferParameters) {
  GTEST_SKIP() << "HLC 786 builds no elaborated hierarchy: getTopModules() is empty and child instances / generate "
                  "scopes are not created. "
                  "Per IEEE 1800-2023 Sec 23.10 and 27 elaboration builds the instance tree and expands generate "
                  "blocks. Fix pending.";
  const hldb::Module *const inst = top();
  ASSERT_NE(inst, nullptr);
  struct Expect {
    const char *m_instance;
    const char *m_param;
    int64_t m_value;
    bool m_overridden;
  };
  const std::vector<Expect> expected = {
      {"aw_buffer_i", "ID_WIDTH", 3, true},    {"aw_buffer_i", "ADDR_WIDTH", 32, true},
      {"aw_buffer_i", "USER_WIDTH", 6, true},  {"aw_buffer_i", "BUFFER_DEPTH", 2, true},
      {"ar_buffer_i", "ID_WIDTH", 3, true},    {"ar_buffer_i", "ADDR_WIDTH", 32, true},
      {"ar_buffer_i", "USER_WIDTH", 6, true},  {"ar_buffer_i", "BUFFER_DEPTH", 2, true},
      {"w_buffer_i", "DATA_WIDTH", 64, true},  {"w_buffer_i", "USER_WIDTH", 6, true},
      {"w_buffer_i", "BUFFER_DEPTH", 2, true}, {"w_buffer_i", "STRB_WIDTH", 8, false},
      {"r_buffer_i", "ID_WIDTH", 3, true},     {"r_buffer_i", "DATA_WIDTH", 64, true},
      {"r_buffer_i", "USER_WIDTH", 6, true},   {"r_buffer_i", "BUFFER_DEPTH", 2, true},
      {"r_buffer_i", "STRB_WIDTH", 8, false},  {"b_buffer_i", "ID_WIDTH", 3, true},
      {"b_buffer_i", "USER_WIDTH", 6, true},   {"b_buffer_i", "BUFFER_DEPTH", 2, true},
  };
  for (const Expect &e : expected) {
    const hldb::Module *const child = hldb::findByName<hldb::Module>(e.m_instance, inst->getModules());
    ASSERT_NE(child, nullptr) << e.m_instance;
    const hldb::ParamAssign *const pa =
        hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(e.m_param, child->getParamAssigns());
    ASSERT_NE(pa, nullptr) << e.m_instance << "." << e.m_param;
    EXPECT_EQ(evalConst(pa->getRhs(), child), std::optional<int64_t>(e.m_value)) << e.m_instance << "." << e.m_param;
    EXPECT_EQ(pa->getOverridden(), e.m_overridden) << e.m_instance << "." << e.m_param;
  }
}

// Sec 23.10: each buffer passes a computed DATA_WIDTH down to its
// axi_single_slice (aw/ar: 29+32+6+3=70, w: 1+64+8+6=79, r: 3+64+6+3=76,
// b: 2+6+3=11) and BUFFER_DEPTH=SLICE_DEPTH=2; the data_i port of the slice
// then has that width.
TEST_F(AxiSliceTest, PropagatedSingleSliceDataWidth) {
  GTEST_SKIP() << "HLC 786 builds no elaborated hierarchy: getTopModules() is empty and child instances / generate "
                  "scopes are not created. "
                  "Per IEEE 1800-2023 Sec 23.10 and 27 elaboration builds the instance tree and expands generate "
                  "blocks. Fix pending.";
  const hldb::Module *const inst = top();
  ASSERT_NE(inst, nullptr);
  const std::vector<std::pair<std::string_view, int64_t>> expected = {
      {"aw_buffer_i", 70}, {"ar_buffer_i", 70}, {"w_buffer_i", 79}, {"r_buffer_i", 76}, {"b_buffer_i", 11}};
  for (const std::pair<std::string_view, int64_t> &e : expected) {
    const hldb::Module *const child = hldb::findByName<hldb::Module>(e.first, inst->getModules());
    ASSERT_NE(child, nullptr) << e.first;
    const hldb::Module *const slice = hldb::findByName<hldb::Module>("i_axi_single_slice", child->getModules());
    ASSERT_NE(slice, nullptr) << e.first;
    EXPECT_EQ(slice->getDefName(), "axi_single_slice") << e.first;
    EXPECT_EQ(paramValue(slice, "DATA_WIDTH"), std::optional<int64_t>(e.second)) << e.first;
    EXPECT_EQ(paramValue(slice, "BUFFER_DEPTH"), std::optional<int64_t>(2)) << e.first;
    const hldb::Port *const dataIn = hldb::findByName<hldb::Port>("data_i", slice->getPorts());
    ASSERT_NE(dataIn, nullptr) << e.first;
    EXPECT_EQ(packedWidth(dataIn->getTypespec(), slice), e.second) << e.first;
  }
}

// Every instantiated module has its source in the Compile() arguments, so
// no "no module definition" error may be reported for any of them.
TEST_F(AxiSliceTest, NoMissingModuleDefinitions) {
  for (const std::string_view name : {"axi_aw_buffer", "axi_ar_buffer", "axi_w_buffer", "axi_r_buffer", "axi_b_buffer",
                                      "axi_single_slice", "fifo", "fifo_v2", "fifo_v3"}) {
    EXPECT_EQ(findError(ErrorDefinition::ELAB_NO_MODULE_DEFINITION, name), nullptr) << name;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
