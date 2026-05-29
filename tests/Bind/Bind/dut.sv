// Comprehensive bind_directive test — IEEE 1800-2017 §23.11
//
// Surelog grammar (SV3_1aParser.g4):
//   bind_directive :
//     attribute_instance* BIND identifier COLON bind_target_instance
//       (COMMA bind_target_instance)* bind_instantiation       // Form 1 with instance list
//   | attribute_instance* BIND bind_target_instance bind_instantiation ;  // Form 2 / scope-only
//
//   bind_target_instance :
//     hierarchical_identifier constant_bit_select              // Form 2: full path (a.b.c)
//   | identifier constant_bit_select ;                         // Form 1 list entry or simple Form 2
//
//   bind_instantiation : module_instantiation | checker_instantiation ;
//
// Parse-time model (pre-elaboration):
//   BindDirective
//     vpiBindTargetScope       → Identifier: scope name (Form 1), or first simple id (Form 2),
//                                or empty for hierarchical Form 2 paths
//     vpiBindTargetInstance[]  → RefObj | BitSelect: instance list (Form 1 with COLON only)
//     vpiBindSourceInstance    → RefModule | CheckerInst
//
// Elaboration resolves Form 1 / Form 2 ambiguity for single-identifier targets.

// =========================================================================
// Shared type definitions
// =========================================================================

// Module to bind INTO (the bind target)
module TargetMod (input logic clk, input logic [7:0] rxd);
endmodule

// Assertion module — used as bind_instantiation (module_instantiation form)
module AsrtMod (input logic clk, input logic [7:0] rxd);
endmodule

// Parameterized assertion module
module AsrtParamMod
    #(parameter int WIDTH = 8)
    (input logic clk, input logic [WIDTH-1:0] rxd);
endmodule

// Checker — used as bind_instantiation (checker_instantiation form)
checker AsrtChecker (input logic clk, input logic [7:0] rxd);
endchecker

// =========================================================================
// Module that instantiates TargetMod (provides named instances for
// instance-list tests: u0, u1, u_arr[0], u_arr[1])
// =========================================================================
module top_instances;
  logic clk;
  logic [7:0] rxd;
  TargetMod u0(.clk(clk), .rxd(rxd));
  TargetMod u1(.clk(clk), .rxd(rxd));
  TargetMod u_arr[1:0](.clk(clk), .rxd(rxd));
endmodule

// =========================================================================
// TOP-LEVEL bind directives  (attached to Design, not any module)
// =========================================================================

// ── Form 1: bind to ALL instances ────────────────────────────────────────

// T1  module_instantiation, named ports
//     Expected: BindDirective { scope:"TargetMod", instances:[], source:RefModule "bd_t1" }
bind TargetMod AsrtMod bd_t1(.clk(clk), .rxd(rxd));

// T2  module_instantiation, ordered ports
//     Expected: BindDirective { scope:"TargetMod", instances:[], source:RefModule "bd_t2" }
bind TargetMod AsrtMod bd_t2(clk, rxd);

// T3  module_instantiation, dotstar port connection
//     Expected: BindDirective { scope:"TargetMod", instances:[], source:RefModule "bd_t3" }
bind TargetMod AsrtMod bd_t3(.*);

// T4  module_instantiation, no ports
//     Expected: BindDirective { scope:"TargetMod", instances:[], source:RefModule "bd_t4" }
bind TargetMod AsrtMod bd_t4();

// T5  module_instantiation with parameter override
//     Expected: BindDirective { scope:"TargetMod", instances:[], source:RefModule "bd_t5" }
bind TargetMod AsrtParamMod #(.WIDTH(8)) bd_t5(.clk(clk), .rxd(rxd));

// T6  checker_instantiation, named ports
//     Expected: BindDirective { scope:"TargetMod", instances:[], source:CheckerInst "bd_t6" }
bind TargetMod AsrtChecker bd_t6(.clk(clk), .rxd(rxd));

// ── Form 1: bind to SPECIFIC instances (COLON required for instance list) ─

// T7  single instance in list (COLON makes it explicit), named ports
//     Expected: instances:[RefObj "u0"]
bind TargetMod : u0 AsrtMod bd_t7(.clk(clk), .rxd(rxd));

// T8  two instances in list, named ports
//     Expected: instances:[RefObj "u0", RefObj "u1"]
bind TargetMod : u0, u1 AsrtMod bd_t8(.clk(clk), .rxd(rxd));

// T9  single bit-selected instance (1D array index)
//     Expected: instances:[BitSelect{index:0, prefix:RefObj "u_arr"}]
bind TargetMod : u_arr[0] AsrtMod bd_t9(.clk(clk), .rxd(rxd));

// T10  two bit-selected instances (1D)
//     Expected: instances:[BitSelect{0,RefObj"u_arr"}, BitSelect{1,RefObj"u_arr"}]
bind TargetMod : u_arr[0], u_arr[1] AsrtMod bd_t10(.clk(clk), .rxd(rxd));

// T11  2D bit-select: u_arr[1][0] (chained constant_bit_select)
//     Expected: instances:[BitSelect{0, prefix:BitSelect{1, prefix:RefObj"u_arr"}}]
bind TargetMod : u_arr[1][0] AsrtMod bd_t11(.clk(clk), .rxd(rxd));

// T12  mixed list: plain name + bit-selected instances
//     Expected: instances:[RefObj"u0", BitSelect{0,"u_arr"}, BitSelect{1,"u_arr"}]
bind TargetMod : u0, u_arr[0], u_arr[1] AsrtMod bd_t12(.clk(clk), .rxd(rxd));

// T13  instance list + checker_instantiation
//     Expected: instances:[RefObj"u0"], source:CheckerInst "bd_t13"
bind TargetMod : u0 AsrtChecker bd_t13(.clk(clk), .rxd(rxd));

// T14  instance list + checker + bit-select
//     Expected: instances:[BitSelect{0,"u_arr"}, BitSelect{1,"u_arr"}], source:CheckerInst
bind TargetMod : u_arr[0], u_arr[1] AsrtChecker bd_t14(.clk(clk), .rxd(rxd));

// ── Form 1/2 ambiguous — single identifier, no COLON ─────────────────────

// T15  parse: bind_target_scope = "u0", instances = []
//      elab:  "u0" is an instance → Form 2 (bind into that specific instance)
//             elaboration should move "u0" to bind_target_instances, clear scope
bind u0 AsrtMod bd_t15(.clk(clk), .rxd(rxd));

// T16  Form 2 with hierarchical path (new bind_target_instance sub-rule)
//      Grammar: bind_target_instance → hierarchical_identifier constant_bit_select
//      Expected: bind_target_scope="" (hierarchical path not extracted at parse time;
//                elaboration resolves), instances:[], source:RefModule "bd_t16"
bind top_instances.u0 AsrtMod bd_t16(.clk(clk), .rxd(rxd));

// =========================================================================
// MODULE-LEVEL bind directives (inside module body; attached to the module)
// =========================================================================

module top_module_binds;
  logic clk;
  logic [7:0] rxd;
  TargetMod ma0(.clk(clk), .rxd(rxd));
  TargetMod ma1(.clk(clk), .rxd(rxd));
  TargetMod ma_arr[1:0](.clk(clk), .rxd(rxd));

  // M1  Form 1 — all instances of TargetMod, module_instantiation
  //     Expected: Module.vpiBindDirective → BindDirective { scope:"TargetMod" }
  bind TargetMod AsrtMod bd_m1(.clk(clk), .rxd(rxd));

  // M2  Form 1 — specific instances, named ports
  //     Expected: instances:[RefObj"ma0", RefObj"ma1"]
  bind TargetMod : ma0, ma1 AsrtMod bd_m2(.clk(clk), .rxd(rxd));

  // M3  Form 1 — bit-selected instance
  //     Expected: instances:[BitSelect{0, RefObj"ma_arr"}]
  bind TargetMod : ma_arr[0] AsrtMod bd_m3(.clk(clk), .rxd(rxd));

  // M4  Form 1 — checker_instantiation
  //     Expected: source:CheckerInst "bd_m4"
  bind TargetMod AsrtChecker bd_m4(.clk(clk), .rxd(rxd));

  // M5  Form 1 — parameterized module
  bind TargetMod AsrtParamMod #(.WIDTH(4)) bd_m5(.clk(clk), .rxd(rxd[3:0]));

endmodule

// =========================================================================
// Bind inside a generate block (gen_scope parent)
// =========================================================================

module top_generate_binds;
  logic clk;
  logic [7:0] rxd;
  TargetMod gb0(.clk(clk), .rxd(rxd));
  TargetMod gb1(.clk(clk), .rxd(rxd));

  // G1  Form 1 inside generate-if (always-true)
  //     Expected: GenScope.vpiBindDirective → BindDirective { scope:"TargetMod" }
  if (1) begin : gen_all
    bind TargetMod AsrtMod bd_g1(.clk(clk), .rxd(rxd));
  end

  // G2  Form 1 with instance list inside generate-if
  //     Expected: instances:[RefObj"gb0", RefObj"gb1"]
  if (1) begin : gen_inst
    bind TargetMod : gb0, gb1 AsrtMod bd_g2(.clk(clk), .rxd(rxd));
  end

  // G3  Form 1 checker inside generate-if
  if (1) begin : gen_chk
    bind TargetMod AsrtChecker bd_g3(.clk(clk), .rxd(rxd));
  end

endmodule
