// ============================================================
// interface_identifier - coverage of every grammar permutation
// Source: SV3_1aParser.g4 (IEEE 1800 SystemVerilog)
//
// Grammar sites covered
//   1  interface_port_header        line  279  ANSI port declarations
//   2  interface_port_declaration   line  663  non-ANSI port declarations
//   3  virtual-interface data_type  line  785  variable declarations
//   4  function_body_declaration    line 1118  out-of-block function impl
//   5  task_body_declaration        line 1159  out-of-block task impl
//   6  interface_instantiation      line 1788  module-level instantiation
//   7  specify_terminal_descriptor  line 2719  specify path endpoints
//   8  interface_identifier rule    line 3587  $root / indexed-path forms
// ============================================================


// ================================================================
// Prerequisite interfaces
// ================================================================

// Parameterised bus interface with modports and extern methods
interface BusIf #(parameter int W = 8) (input logic clk, input logic rst_n);
    logic [W-1:0] data;
    logic         valid;
    logic         ready;

    modport master (output data, valid, input  ready, clk, rst_n);
    modport slave  (input  data, valid, output ready, input clk, rst_n);

    extern task     drive    (input logic [W-1:0] d);
    extern task     drive_v2 ();
    extern task     noop     ();
    extern function logic [W-1:0] capture ();
    extern function void dummy ();
endinterface

// Minimal sub-interface (used in specify and hierarchical examples)
interface SubIf;
    logic sig;
    modport src (output sig);
    modport dst (input  sig);
endinterface


// ================================================================
// 1  interface_port_header - ANSI-style port declarations
//
//   Grammar (line 279):
//     interface_port_header
//       : identifier (DOT identifier)?   // named interface type
//       | INTERFACE (DOT identifier)?    // generic interface keyword
//       ;
//   Used in:  ansi_port_declaration -> list_of_port_declarations
// ================================================================

// 1a. interface name only  (no modport)
module mod_ansi_plain_port (
    BusIf       bus,
    input logic clk
);
endmodule

// 1b. interface name DOT identifier  (with modport)
module mod_ansi_modport_port (
    BusIf.master mst,
    BusIf.slave  slv,
    input  logic clk
);
endmodule

// 1c. INTERFACE keyword only  (generic / untyped interface port)
module mod_ansi_generic_port (
    interface    gen_if,
    input logic  clk
);
endmodule

// 1d. INTERFACE DOT identifier  (generic port with modport)
module mod_ansi_generic_modport (
    interface.master gen_mst,
    input logic      clk
);
endmodule

// 1e. interface name + unpacked_dimension  (port array, no modport)
module mod_ansi_array_port (
    BusIf      bus [4],
    input logic clk
);
endmodule

// 1f. interface name DOT identifier + unpacked_dimension  (modport port array)
module mod_ansi_modport_array_port (
    BusIf.master mst [2],
    BusIf.slave  slv [2],
    input logic  clk
);
endmodule


// ================================================================
// 2  interface_port_declaration - non-ANSI port declarations
//
//   Grammar (line 663):
//     interface_port_declaration
//       : identifier interface_identifier_list
//       | identifier DOT identifier interface_identifier_list
//       ;
//   interface_identifier_list = identifier unpacked_dimension*
//                               { COMMA identifier unpacked_dimension* }
//   Appears in the module body after the non-ANSI port list header.
// ================================================================

// 2a. interface name  identifier  (single port, no modport)
module mod_nonansi_single (bus, clk);
    BusIf        bus;
    input logic  clk;
endmodule

// 2b. interface name  identifier, identifier  (multiple ports, no modport)
module mod_nonansi_multi (busA, busB, clk);
    BusIf        busA, busB;
    input logic  clk;
endmodule

// 2c. interface name DOT identifier  identifier  (with modport, single)
module mod_nonansi_modport (mst, slv, clk);
    BusIf.master mst;
    BusIf.slave  slv;
    input logic  clk;
endmodule

// 2d. interface name DOT identifier  identifier, identifier  (with modport, multiple)
module mod_nonansi_modport_multi (mstA, mstB, slv0, clk);
    BusIf.master mstA, mstB;
    BusIf.slave  slv0;
    input logic  clk;
endmodule

// 2e. interface name  identifier[dim]  (port array, no modport)
module mod_nonansi_array (bus_arr, clk);
    BusIf        bus_arr [4];
    input logic  clk;
endmodule

// 2f. interface name DOT identifier  identifier[dim]  (modport port array)
module mod_nonansi_modport_array (mst_arr, clk);
    BusIf.master mst_arr [2];
    input logic  clk;
endmodule


// ================================================================
// 3  Virtual-interface data_type
//
//   Grammar (line 785):
//     data_type alternative:
//       VIRTUAL INTERFACE? identifier
//         parameter_value_assignment? (DOT identifier)?
//
//   Permutation axes:
//     INTERFACE keyword - optional
//     parameter_value_assignment - optional (#(...) or #value)
//     DOT identifier (modport) - optional
// ================================================================

module mod_virtual_ifs;
    // 3a. virtual interface name
    virtual BusIf                    vif_bare;

    // 3b. virtual INTERFACE interface name  (explicit INTERFACE keyword)
    virtual interface BusIf          vif_kw;

    // 3c. virtual interface name DOT identifier  (modport, no INTERFACE kw)
    virtual BusIf.master             vif_mst;
    virtual BusIf.slave              vif_slv;

    // 3d. virtual INTERFACE interface name DOT identifier  (modport + INTERFACE kw)
    virtual interface BusIf.master   vif_kw_mst;
    virtual interface BusIf.slave    vif_kw_slv;

    // 3e. virtual interface name parameter_value_assignment  (named param)
    virtual BusIf #(.W(16))          vif_param_named;

    // 3f. virtual interface name parameter_value_assignment  (positional param)
    virtual BusIf #(32)              vif_param_pos;

    // 3g. virtual INTERFACE interface name parameter_value_assignment
    virtual interface BusIf #(.W(4)) vif_kw_param;

    // 3h. virtual interface name parameter_value_assignment DOT identifier
    //     (named param + modport)
    virtual BusIf #(.W(32)) .master  vif_param_mst;

    // 3i. virtual INTERFACE interface name parameter_value_assignment DOT identifier
    //     (INTERFACE kw + param + modport - all optional tokens present)
    virtual interface BusIf #(.W(8)) .slave  vif_kw_param_slv;

    // 3j. Arrays of virtual interfaces  (unpacked dimension on the variable itself)
    virtual BusIf                vif_arr    [4];
    virtual BusIf.master         vif_mst_2d [2][2];
    virtual interface BusIf      vif_kw_arr [3];
endmodule


// ================================================================
// 4  Out-of-block function body declaration
//
//   Grammar (lines 1118-1130):
//     function_body_declaration
//       : function_data_type_or_implicit
//         (identifier DOT | class_scope)?   // <-- site of interest
//         identifier
//         SEMICOLON tf_item_declaration_list           // alternative A
//         function_statement_or_null* ENDFUNCTION (COLON identifier)?
//       | function_data_type_or_implicit
//         (identifier DOT | class_scope)?   // <-- site of interest
//         identifier
//         (tf_port_item_list? | OPEN_PARENS CLOSE_PARENS)  // alternative B
//         SEMICOLON block_item_declaration*
//         function_statement_or_null* ENDFUNCTION (COLON identifier)?
//       ;
//
//   identifier DOT implements an extern function declared inside the named interface.
// ================================================================

// 4a. Alt-A  semicolon-style (no ports - tf_item_declaration_list is empty)
function automatic logic [7:0] BusIf.capture;
    return data;
endfunction

// 4b. Alt-B  empty parentheses  ()
function void BusIf.dummy ();
endfunction

// 4c. Alt-A  with ENDFUNCTION label  (COLON identifier)
function automatic logic [7:0] BusIf.capture_labeled;
    return data;
endfunction : capture_labeled

// 4d. Alt-B  with tf_port_item_list (non-empty inline ports)
function void BusIf.dummy_v2 (input logic unused);
endfunction

// 4e. Alt-B  with ENDFUNCTION label
function void BusIf.dummy_labeled ();
endfunction : dummy_labeled


// ================================================================
// 5  Out-of-block task body declaration
//
//   Grammar (lines 1159-1166):
//     task_body_declaration
//       : (identifier DOT | class_scope)?   // <-- site of interest
//         identifier
//         SEMICOLON tf_item_declaration_list           // alternative A
//         statement_or_null* ENDTASK (COLON identifier)?
//       | (identifier DOT | class_scope)?   // <-- site of interest
//         identifier
//         (tf_port_item_list? | OPEN_PARENS CLOSE_PARENS)  // alternative B
//         SEMICOLON block_item_declaration*
//         statement_or_null* ENDTASK (COLON identifier)?
//       ;
// ================================================================

// 5a. Alt-A  semicolon-style port list
task BusIf.drive;
    input logic [7:0] d;
    data  = d;
    valid = 1'b1;
    @(posedge clk);
    valid = 1'b0;
endtask

// 5b. Alt-A  no ports at all  (tf_item_declaration_list is empty)
task BusIf.noop;
endtask

// 5c. Alt-B  ANSI-style parenthesised port list
task automatic BusIf.drive_v2 (input logic [W-1:0] d);
    data  = d;
    valid = 1'b1;
endtask

// 5d. Alt-B  empty parentheses
task BusIf.noop_v2 ();
endtask

// 5e. Alt-A  with ENDTASK label  (COLON identifier)
task BusIf.drive_labeled;
    input logic [W-1:0] d;
    data = d;
endtask : drive_labeled

// 5f. Alt-B  with ENDTASK label
task automatic BusIf.drive_v2_labeled (input logic [W-1:0] d);
    data = d;
endtask : drive_v2_labeled


// ================================================================
// 6  interface_instantiation
//
//   Grammar (line 1788):
//     interface_instantiation
//       : identifier parameter_value_assignment?
//         hierarchical_instance
//         (COMMA hierarchical_instance)* SEMICOLON
//       ;
// ================================================================

module top_instantiation;
    logic clk, rst_n;

    // 6a. interface name only  (no parameter_value_assignment, single instance)
    BusIf u_bus (.clk(clk), .rst_n(rst_n));

    // 6b. interface name + named parameter_value_assignment
    BusIf #(.W(16)) u_bus16 (.clk(clk), .rst_n(rst_n));

    // 6c. interface name + positional parameter_value_assignment
    BusIf #(32) u_bus32 (.clk(clk), .rst_n(rst_n));

    // 6d. Single instance with explicit port connections
    BusIf u_bus_wc (.clk(clk), .rst_n(rst_n));

    // 6e. Multiple instances in one statement  (COMMA hierarchical_instance)+
    BusIf u_bus_a (.clk(clk), .rst_n(rst_n)),
          u_bus_b (.clk(clk), .rst_n(rst_n));

    // 6f. Parameterized + multiple instances
    BusIf #(.W(4)) u_narrow_a (.clk(clk), .rst_n(rst_n)),
                   u_narrow_b (.clk(clk), .rst_n(rst_n));

    // 6g. Array-of-interfaces instantiation via unpacked dimension on instance name
    BusIf u_bus_arr [4] (.clk(clk), .rst_n(rst_n));

    // 6h. Sub-interface instantiation
    SubIf u_sub ();

    // Wire up to consumers
    mod_ansi_plain_port   u_plain (.bus(u_bus),   .clk(clk));
    mod_ansi_modport_port u_mp    (.mst(u_bus16), .slv(u_bus), .clk(clk));
endmodule


// ================================================================
// 7  specify_terminal_descriptor
//
//   Grammar (line 2719):
//     specify_terminal_descriptor
//       : (identifier | interface_identifier DOT identifier)
//         constant_range_expression?
//       ;
//
//   Appears as the endpoint of a path delay statement inside a
//   specify block.  The interface port name precedes DOT; the signal
//   within it follows.
// ================================================================

module mod_specify_paths (
    BusIf.slave  in_bus,
    BusIf.master out_bus,
    input  logic clk
);
    assign out_bus.data  = in_bus.data;
    assign out_bus.valid = in_bus.valid;
    assign in_bus.ready  = out_bus.ready;

    specify
        // 7a. identifier form - plain port signal, no range
        (clk => out_bus.valid) = (1, 1);

        // 7b. interface port DOT identifier - interface member, no range
        (in_bus.valid *> out_bus.valid) = (2, 2);

        // 7c. interface port DOT identifier WITH constant_range_expression (bit range)
        (in_bus.data[7:0] *> out_bus.data[7:0]) = (3, 3);

        // 7d. interface port DOT identifier WITH constant_range_expression (single bit)
        (in_bus.data[0] *> out_bus.data[0]) = (1, 1);
    endspecify
endmodule


// ================================================================
// 8  interface_identifier rule - $root and indexed-path forms
//    (NOTE: these are non-standard extensions beyond LRM BNF;
//     included here for grammar coverage documentation only)
//
//   Grammar (line 3580):
//     interface_identifier
//       : dollar_root_keyword? identifier
//         (constant_bit_select DOT identifier)*
//       ;
//   where:
//     dollar_root_keyword  = "$root" "."
//     constant_bit_select  = "[" constant_expression "]"
//
//   The base form (bare identifier) is used in every section above.
//   This section exercises the extended forms.
// ================================================================

module mod_intf_id_extended_forms;
    logic clk, rst_n;

    // Instantiate an array of BusIf so the indexed path forms are grounded
    BusIf bus_arr [4] (.clk(clk), .rst_n(rst_n));

    // 8a. Simple identifier - baseline, covered throughout all sections above
    virtual BusIf vif_simple;

    // 8b. dollar_root_keyword prefix: "$root." identifier  (non-standard)
    virtual $root.BusIf vif_from_root;

    // 8c. dollar_root_keyword + modport  (non-standard)
    virtual $root.BusIf.master vif_root_mst;

    // 8d. dollar_root_keyword + parameter_value_assignment  (non-standard)
    virtual $root.BusIf #(.W(16)) vif_root_param;

    // 8e. identifier "[" constant_expression "]" "." identifier  (non-standard)
    //     (constant_bit_select DOT identifier) - one level of array indexing.
    virtual bus_arr[0].SubIf vif_indexed;

    // 8f. Chained: identifier[i].identifier[j].identifier  (non-standard)
    //     Multiple (constant_bit_select DOT identifier)* iterations.
    // virtual deep_arr[1].mid_arr[0].LeafIf vif_deep;

    // 8g. $root + indexed path  (non-standard)
    virtual $root.bus_arr[2].SubIf vif_root_indexed;

endmodule

// ================================================================
// END OF FILE
// ================================================================
