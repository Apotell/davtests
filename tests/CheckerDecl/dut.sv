// =============================================================================
// Surelog checker_declaration DUT
// Exercises every grammar production in IEEE 1800-2017 §17 / Annex A
// =============================================================================

// ---------------------------------------------------------------------------
// C1: Minimal — no port list, empty body
// checker_declaration: CHECKER identifier SEMICOLON ENDCHECKER
// ---------------------------------------------------------------------------
checker C1_NoPortsNoBody;
endchecker


// ---------------------------------------------------------------------------
// C2: All checker_port_item variants
//   data-typed input  : logic, with packed dimension
//   no-direction port : data_type_or_implicit (direction keyword omitted)
//   sequence formal   : formal type SEQUENCE
//   property formal   : formal type PROPERTY
//   untyped formal    : formal type UNTYPED
//   output direction  : OUTPUT data_type
//   unpacked dim      : identifier variable_dimension*
//   default value     : ASSIGN_OP property_actual_arg
// ---------------------------------------------------------------------------
checker C2_PortFormalTypes(
    input  logic            clk,
    input  logic            rst_n,
    input  logic [7:0]      data,
           logic            no_dir_port,          // direction omitted
    input  sequence         seq_in,
    input  property         prop_in,
    input  untyped          untyped_in,
    output logic            flag_out,
    input  logic [3:0]      vec_in  [2],           // unpacked dim on port
    input  logic            dflt_in = 1'b0          // default value
);
endchecker : C2_PortFormalTypes


// ---------------------------------------------------------------------------
// C3: checker_or_generate_item_declaration: RAND* data_declaration
//   plain variables + rand-qualified variables
// ---------------------------------------------------------------------------
checker C3_DataDecl(input logic clk, input logic en);
    logic        state;
    rand  logic  r_state;
    int          counter;
    bit   [7:0]  byte_val;
endchecker : C3_DataDecl


// ---------------------------------------------------------------------------
// C4: assertion_item_declaration (property, sequence, let)
//     + assertion_item via concurrent_assertion_item:
//       assert_property_statement, assume_property_statement,
//       cover_property_statement
// ---------------------------------------------------------------------------
checker C4_AssertionItemDecl(
    input logic clk,
    input logic a,
    input logic b
);
    // assertion_item_declaration: sequence_declaration
    sequence s_ab;
        a ##1 b;
    endsequence

    // assertion_item_declaration: property_declaration
    property p_ab;
        @(posedge clk) a |-> ##1 b;
    endproperty

    // assertion_item_declaration: let_declaration
    let L_and(x, y) = x && y;

    // assertion_item: concurrent_assertion_item -> assert_property_statement
    assert_ab : assert property (p_ab);

    // assertion_item: concurrent_assertion_item -> assume_property_statement
    assume_a  : assume property (@(posedge clk) a);

    // assertion_item: concurrent_assertion_item -> cover_property_statement
    cover_ab  : cover  property (p_ab);
endchecker : C4_AssertionItemDecl


// ---------------------------------------------------------------------------
// C5: concurrent_assertion_statement — restrict_property_statement
//     + assertion_item -> concurrent_assertion_item ->
//       cover_sequence_statement
// ---------------------------------------------------------------------------
checker C5_RestrictCoverSeq(input logic clk, input logic a, input logic b);
    sequence s_a_then_b;
        a ##[1:3] b;
    endsequence

    // concurrent_assertion_statement: restrict_property_statement
    restrict property (@(posedge clk) a |-> b);

    // concurrent_assertion_statement: cover_sequence_statement
    cover_seq : cover sequence (
        @(posedge clk) s_a_then_b
    ) $display("C5: covered");
endchecker : C5_RestrictCoverSeq


// ---------------------------------------------------------------------------
// C6: checker_or_generate_item_declaration: function_declaration
//     + checker_or_generate_item: continuous_assign
// ---------------------------------------------------------------------------
checker C6_FunctionDecl(input logic clk, input logic [7:0] data);
    // checker_or_generate_item_declaration: function_declaration
    function automatic logic [7:0] invert_byte(input logic [7:0] val);
        return ~val;
    endfunction

    logic [7:0] inv_data;

    // checker_or_generate_item: continuous_assign
    assign inv_data = invert_byte(data);

    assert property (@(posedge clk) inv_data != data);
endchecker : C6_FunctionDecl


// ---------------------------------------------------------------------------
// C7: checker_or_generate_item: initial_construct, always_construct,
//     final_construct
// ---------------------------------------------------------------------------
checker C7_ProceduralItems(input logic clk);
    int cnt;

    // checker_or_generate_item: initial_construct
    initial cnt = 0;

    // checker_or_generate_item: always_construct
    always @(posedge clk) cnt <= cnt + 1;

    // checker_or_generate_item: final_construct
    final $display("C7: final count = %0d", cnt);
endchecker : C7_ProceduralItems


// ---------------------------------------------------------------------------
// C8: checker_or_generate_item_declaration: clocking_declaration
//     + DEFAULT CLOCKING identifier SEMICOLON
//     + DEFAULT DISABLE IFF expression_or_dist SEMICOLON
// ---------------------------------------------------------------------------
checker C8_ClockingDefault(
    input logic clk,
    input logic rst,
    input logic a,
    input logic b
);
    // checker_or_generate_item_declaration: clocking_declaration
    clocking cb @(posedge clk);
        input a, b;
    endclocking

    // checker_or_generate_item_declaration: DEFAULT CLOCKING identifier SEMICOLON
    default clocking cb;

    // checker_or_generate_item_declaration: DEFAULT DISABLE IFF expression SEMICOLON
    default disable iff rst;

    assert property (a |-> b);
endchecker : C8_ClockingDefault


// ---------------------------------------------------------------------------
// C9: checker_or_generate_item_declaration: covergroup_declaration
// ---------------------------------------------------------------------------
checker C9_CovergroupDecl(input logic clk, input logic [1:0] mode);
    // checker_or_generate_item_declaration: covergroup_declaration
    covergroup cg_mode @(posedge clk);
        cp_mode : coverpoint mode {
            bins b_low  = {2'b00, 2'b01};
            bins b_high = {2'b10, 2'b11};
        }
    endgroup

    cg_mode cg_inst = new();
endchecker : C9_CovergroupDecl


// ---------------------------------------------------------------------------
// C10: checker_or_generate_item_declaration: genvar_declaration
//      checker_generate_item: loop_generate_construct
// ---------------------------------------------------------------------------
checker C10_GenvarLoop(input logic clk, input logic [3:0] en);
    // checker_or_generate_item_declaration: genvar_declaration
    genvar gi;

    // checker_generate_item: loop_generate_construct (generate_region wrapper)
    generate
        for (gi = 0; gi < 4; gi++) begin : gen_en_check
            assert property (@(posedge clk) en[gi]);
        end
    endgenerate
endchecker : C10_GenvarLoop


// ---------------------------------------------------------------------------
// C11: checker_generate_item: conditional_generate_construct
// ---------------------------------------------------------------------------
checker C11_ConditionalGen(input logic clk, input logic a, input logic b);
    // checker_generate_item: conditional_generate_construct
    generate
        if (1) begin : gen_strict
            assert property (@(posedge clk) a |-> ##1 b);
        end else begin : gen_loose
            assert property (@(posedge clk) a |-> ##[1:5] b);
        end
    endgenerate
endchecker : C11_ConditionalGen


// ---------------------------------------------------------------------------
// C12: checker_or_generate_item_declaration: nested checker_declaration
//      + checker_instantiation as concurrent_assertion_item
// ---------------------------------------------------------------------------
checker C12_NestedChecker(input logic clk, input logic a, input logic b);
    // checker_or_generate_item_declaration: checker_declaration (nested)
    checker Inner(input logic clk_i, input logic sig_i);
        assert property (@(posedge clk_i) sig_i);
    endchecker : Inner

    // concurrent_assertion_item: checker_instantiation (via isCheckerElem pred)
    Inner ic_a(.clk_i(clk), .sig_i(a));
    Inner ic_b(.clk_i(clk), .sig_i(b));
endchecker : C12_NestedChecker


// ---------------------------------------------------------------------------
// C13: assertion_item: deferred_immediate_assertion_item
//      — #0 form and final form
// ---------------------------------------------------------------------------
checker C13_DeferredImmediate(input logic clk, input logic a);
    // deferred_immediate_assertion_item: assert #0 (expression)
    deferred_0   : assert #0    (a) else $error("C13: a low at #0");

    // deferred_immediate_assertion_item: assert final (expression)
    deferred_fin : assert final (a) else $error("C13: a low at final");
endchecker : C13_DeferredImmediate


// ---------------------------------------------------------------------------
// C14: checker_or_generate_item_declaration: SEMICOLON (empty item)
//      end-label on ENDCHECKER
// ---------------------------------------------------------------------------
checker C14_EmptySemicolons(input logic clk, input logic a);
    // checker_or_generate_item_declaration: SEMICOLON
    ;

    assert property (@(posedge clk) a);

    // another empty item
    ;
endchecker : C14_EmptySemicolons


// ---------------------------------------------------------------------------
// C15: attribute_instance* on checker_or_generate_item
// ---------------------------------------------------------------------------
checker C15_AttributedItem(input logic clk, input logic a, input logic b);
    (* synthesis, keep *)
    assert property (@(posedge clk) a |-> b);

    (* checker_attr = "val" *)
    sequence s_x;
        a ##2 b;
    endsequence
endchecker : C15_AttributedItem


// ---------------------------------------------------------------------------
// C16: checker with no body items (only empty port list)
// ---------------------------------------------------------------------------
checker C16_EmptyBody(input logic clk);
endchecker : C16_EmptyBody


// ---------------------------------------------------------------------------
// C17: DEFAULT DISABLE IFF with dist clause (expression_or_dist with DIST)
//      Exercises the dist_item / dist_list path inside a checker body.
// ---------------------------------------------------------------------------
checker C17_DisableIffDist(
    input logic clk,
    input logic [1:0] mode,
    input logic a,
    input logic b
);
    // default disable iff with a distribution:
    //   mode dist { 2'b01 := 80, 2'b10 := 20 }
    // IEEE §17.3: the checker is disabled when the expression_or_dist is true.
    // Note: no parens — grammar is "DEFAULT DISABLE IFF expression_or_dist SEMICOLON".
    default disable iff mode dist { 2'b01 := 80, 2'b10 := 20 };

    assert property (@(posedge clk) a |-> b);
endchecker : C17_DisableIffDist


// ---------------------------------------------------------------------------
// top: module that instantiates all checkers
// NOTE: checker_port_connection_list grammar allows at most 2 ordered
//       connections; named connections use DOT-STAR (.*) where port names
//       in the checker match signal names in scope.
// ---------------------------------------------------------------------------
module top;
    logic        clk;
    logic        rst_n;
    logic [7:0]  data;
    logic        a, b;
    logic [3:0]  en;
    logic [1:0]  mode;
    logic [3:0]  vec_arr [2];

    initial clk = 0;
    always  #5  clk = ~clk;

    initial begin
        rst_n = 0; a = 0; b = 0;
        en    = 4'b1010;
        mode  = 2'b01;
        data  = 8'hAB;
        vec_arr[0] = 4'h3;
        vec_arr[1] = 4'h7;
        #10  rst_n = 1;
        #10  a = 1;
        #5   b = 1;
        #50  $finish;
    end

    // C1: no ports — empty connection list
    C1_NoPortsNoBody c1();

    // C2: port formal types — named dot-star; sequence/property formals
    //     are unbound in the .* form (grammar-level test only)
    C2_PortFormalTypes c2(.*);

    // C3: two ports — ordered connections (grammar allows 2)
    C3_DataDecl c3(clk, a);

    // C4: three ports — named dot-star
    C4_AssertionItemDecl c4(.*);

    // C5: three ports — named dot-star
    C5_RestrictCoverSeq c5(.*);

    // C6: two ports — ordered connections
    C6_FunctionDecl c6(clk, data);

    // C7: one port — ordered connection
    C7_ProceduralItems c7(clk);

    // C8: four ports — named dot-star
    C8_ClockingDefault c8(.*);

    // C9: two ports — named dot-star
    C9_CovergroupDecl c9(.*);

    // C10: two ports — named dot-star
    C10_GenvarLoop c10(.*);

    // C11: three ports — named dot-star
    C11_ConditionalGen c11(.*);

    // C12: three ports — named dot-star
    C12_NestedChecker c12(.*);

    // C13: two ports — named dot-star
    C13_DeferredImmediate c13(.*);

    // C14: two ports — named dot-star
    C14_EmptySemicolons c14(.*);

    // C15: three ports — named dot-star
    C15_AttributedItem c15(.*);

    // C16: one port — ordered connection
    C16_EmptyBody c16(clk);

    // C17: default disable iff with dist
    C17_DisableIffDist c17(.*);

endmodule


checker my_checker_global2(property p, sequence s);
  logic checker_clk;
  global clocking checker_clocking @(checker_clk); endclocking
  assert property (p);
  cover property (s);
endchecker