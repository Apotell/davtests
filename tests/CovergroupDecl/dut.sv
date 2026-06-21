// IEEE 1800-2017 Section 19 - Covergroup declarations inside a module.
// Exercises key covergroup grammar forms to validate Phase2ModelBuilder:
//   1. cg_basic   : coverpoints with all bin keyword types + cross coverage.
//                   coverage_option items interleaved with cover_points to
//                   verify the stmt ordered collection preserves source order.
//                   cp_addr includes a bin using ASSIGN_OP expression to
//                   exercise cover_bin.value (single-value bin form).
//                   cx_op_addr   : 2-way cross (minimal cross_item_list).
//                   cx_three_way : 3-way cross (cross_item_list with 3 items)
//                                  exercises the repeating cross_item rule.
//   2. cg_clocked : covergroup with @(posedge clk) sampling event and
//                   type_option (option_type = 1).

module covergroup_dut (
  input logic       clk,
  input logic [3:0] addr,
  input logic [1:0] op,
  input logic       valid
);

  // -----------------------------------------------------------------
  // 1. Covergroup with no sampling event (manually sampled).
  //
  // stmt collection order (indices 0..6):
  //   [0] coverage_option  option.per_instance
  //   [1] cover_point      cp_op
  //   [2] coverage_option  option.comment      (interleaved option)
  //   [3] cover_point      cp_addr
  //   [4] cover_point      cp_valid
  //   [5] cover_cross      cx_op_addr   (2 cross_items)
  //   [6] cover_cross      cx_three_way (3 cross_items)
  // -----------------------------------------------------------------
  covergroup cg_basic;
    option.per_instance = 1;          // stmt[0]: coverage_option (option_type=0)

    cp_op : coverpoint op {           // stmt[1]: cover_point
      bins         rd      = {2'b00};
      bins         wr      = {2'b01};
      wildcard bins any_rd = {2'b?0};
      illegal_bins  ill_op = {2'b11};
      ignore_bins   ign_op = {2'b10};
      bins          other  = default;
    }

    option.comment = "addr coverage"; // stmt[2]: coverage_option between points

    cp_addr : coverpoint addr {       // stmt[3]: cover_point
      bins lo    = {[4'h0:4'h7]};
      bins hi    = {[4'h8:4'hF]};
      bins exact = 4'hA;              // ASSIGN_OP expression: cover_bin.value
    }

    // A 1-bit coverpoint; enables the 3-way cross below.
    cp_valid : coverpoint valid {     // stmt[4]: cover_point
      bins inactive = {1'b0};
      bins active   = {1'b1};
    }

    // 2-way cross: cross_item_list = cp_op COMMA cp_addr (minimum 2 items).
    cx_op_addr : cross cp_op, cp_addr; // stmt[5]: cover_cross

    // 3-way cross: cross_item_list = cp_op COMMA cp_addr COMMA cp_valid.
    // Exercises the repeating (COMMA cross_item)* part of the grammar rule.
    cx_three_way : cross cp_op, cp_addr, cp_valid; // stmt[6]: cover_cross

  endgroup : cg_basic

  // -----------------------------------------------------------------
  // 2. Covergroup with @(posedge clk) sampling event.
  //    Uses type_option (option_type = 1).
  // -----------------------------------------------------------------
  covergroup cg_clocked @(posedge clk);
    type_option.merge_instances = 1;  // coverage_option (option_type=1)

    cp_state : coverpoint op {
      bins idle = {2'b00};
      bins busy = {2'b01};
    }
  endgroup : cg_clocked

  cg_basic   cg_b_inst;
  cg_clocked cg_c_inst;

endmodule : covergroup_dut

module top;
	logic     clk;
	bit [7:0] v_a, v_b;
	covergroup cg @(posedge clk);
	a: coverpoint v_a
	{
		bins a1 = { [0:63] };
		option.weight = 2;        // option between a1 and a2
		bins a2 = { [64:127] };
		bins a3 = { [128:191] };
		option.at_least = 1;      // option between a3 and a4
		bins a4 = { [192:255] };
	}
	b: coverpoint v_b
	{
		option.weight = 1;        // option before first bin
		bins b1 = {0};
		bins b2 = { [1:84] };
		option.auto_bin_max = 8;  // option in the middle
		bins b3 = { [85:169] };
		bins b4 = { [170:255] };
	}
	c : cross a, b
	{
		bins c1 = ! binsof(a) intersect {[100:200]};// 4 cross products
		bins c2 = binsof(a.a2) || binsof(b.b2);// 7 cross products
		bins c3 = binsof(a.a1) && binsof(b.b4);// 1 cross product
	}
	endgroup
endmodule

// -----------------------------------------------------------------
// module cg_extra
//   Exercises grammar forms not covered by module covergroup_dut:
//     cg_params   : tf_port_item_list + clocking_event (parameters)
//     cg_typed    : typed coverpoint (data_type id COLON coverpoint)
//     cg_iff      : IFF on coverpoint, IFF on cross,
//                   unlabeled coverpoint, unlabeled cross
//     cg_bins     : transition bins, array bins [N], default bins,
//                   default sequence bins, WITH filter on bins,
//                   per-bin IFF, coverage_option inside bins body
//     cg_sampled  : WITH FUNCTION SAMPLE coverage_event
// -----------------------------------------------------------------
module cg_extra (
  input logic       clk,
  input logic       en,
  input logic [3:0] addr,
  input logic [1:0] op
);

  // cg_params: covergroup with parameter list + @(posedge clk) event
  covergroup cg_params(input int lo = 0, input int hi = 15) @(posedge clk);
    cp_pp : coverpoint addr {
      bins all = {[4'h0:4'hF]};
    }
  endgroup : cg_params

  // cg_typed: data_type_or_implicit id COLON COVERPOINT (typed coverpoint)
  covergroup cg_typed;
    logic [3:0] cp_typed : coverpoint addr {
      bins lo = {[4'h0:4'h7]};
    }
  endgroup : cg_typed

  // cg_iff: IFF on coverpoint, IFF on cross, unlabeled coverpoint, unlabeled cross
  covergroup cg_iff;
    cp_guarded : coverpoint addr iff (en) {
      bins lo = {[4'h0:4'h7]};
      bins hi = {[4'h8:4'hF]};
    }
    cp_op2 : coverpoint op {
      bins rd = {2'b00};
      bins wr = {2'b01};
    }
    coverpoint en;                               // unlabeled coverpoint, SEMICOLON body
    cx_iff : cross cp_guarded, cp_op2 iff (en); // cross with IFF guard
    cross cp_guarded, cp_op2;                   // unlabeled cross, SEMICOLON body
  endgroup : cg_iff

  // cg_bins: transition bins, array bins [N], default, default sequence,
  //          WITH filter, per-bin IFF, coverage_option in bins body
  covergroup cg_bins;
    cp_trans : coverpoint op {
      bins rd_to_wr = (2'b00 => 2'b01);     // transition bins
    }
    cp_arr : coverpoint addr {
      bins quarters[4] = {[4'h0:4'hF]};     // array bins with count
    }
    cp_def : coverpoint op {
      bins other   = default;                // default bins
      bins def_seq = default sequence;       // default sequence bins
    }
    cp_with : coverpoint addr {
      bins hi_half = {[4'h0:4'hF]} with (item > 7); // WITH filter
    }
    cp_bin_iff : coverpoint op {
      bins rd = {2'b00} iff (en);            // per-bin IFF guard
    }
    cp_opt_body : coverpoint addr {
      option.auto_bin_max = 8;               // coverage_option inside bins body
      bins lo = {[4'h0:4'h7]};
    }
  endgroup : cg_bins

  // cg_sampled: WITH FUNCTION SAMPLE coverage_event
  covergroup cg_sampled with function sample(
      input logic [3:0] v_a, input logic [1:0] v_o);
    cp_sa : coverpoint v_a {
      bins lo = {[4'h0:4'h7]};
      bins hi = {[4'h8:4'hF]};
    }
  endgroup : cg_sampled

  cg_params  cg_p_inst;
  cg_typed   cg_typed_inst;
  cg_iff     cg_iff_inst;
  cg_bins    cg_bins_inst;
  cg_sampled cg_s_inst;
endmodule : cg_extra
