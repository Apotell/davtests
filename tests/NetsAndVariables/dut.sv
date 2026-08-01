// netvar sample: ANSI and non-ANSI net/variable declaration permutations.
//
// Grammar reference (SV3_1aParser.g4):
//   net_type          : SUPPLY0 | SUPPLY1 | TRI | TRIAND | TRIOR | TRIREG
//                     | TRI0 | TRI1 | UWIRE | WIRE | WAND | WOR
//   net_port_header   : port_direction? net_type? data_type_or_implicit
//   variable_port_header : port_direction? (data_type | var_type data_type_or_implicit)
//   ansi_port_declaration
//                     : (net_port_header | interface_port_header) identifier ...
//                     | variable_port_header? identifier ...
//
// A bare identifier (no header at all) in an ANSI port list inherits the
// direction/kind/type of the immediately preceding port in the same list.
// A data type that cannot be a net (reg, integer, real, string, ...) forces
// a Variable even without an explicit "var" keyword.

// =====================================================================
// Section 1: ANSI-style ports -- net-kind and variable-kind permutations
// =====================================================================
module ansi_ports_net_and_var (
  // -- net-kind ports (net_port_header) --
  input  wire            i_wire,           // explicit net type, implicit 1-bit logic
  input  wire  logic      i_wire_logic,      // explicit net type + explicit data type
  input  tri   [3:0]     i_tri_bus,        // explicit net type + vector
  input  logic           i_logic_default,  // no net_type/var -> defaults to net (wire)
  input  wire  logic      i_a,
                          i_b,              // bare identifier -> inherits "input wire logic"
  output wire            o_wire,
  output wand  logic      o_wand_logic,
  output logic           o_logic_default,  // explicit data_type, no port kind -> defaults to
                                            // Variable (Sec 23.2.2.3), driven by continuous assign
  inout  wire            io_wire,          // inout must stay net-compatible
  inout  tri             io_tri,
  input wire            o_wire1, o_wire2, o_wire3,

  // -- variable-kind ports (variable_port_header) --
  input  var   logic      i_var_logic, i_var_logic2,       // explicit "var" -> forces Variable
  output var   logic      o_var_logic,       // explicit "var" -> forces Variable
  output var   logic      o_c,
                          o_d,               // bare identifier -> inherits "output var logic"
  output reg              o_reg,            // "reg" is variable-only -> Variable w/o var
  output integer          o_integer,        // integer_atom_type -> Variable w/o var
  output real             o_real            // non_integer_type -> Variable w/o var
);

  assign o_wire          = i_wire;
  assign o_wand_logic    = i_wire_logic;
  assign o_logic_default = i_logic_default;
  assign io_wire         = i_tri_bus[0];
  assign io_tri          = io_wire;

  always_comb begin
    o_var_logic = i_var_logic;
    o_c         = i_a;
    o_d         = i_b;
    o_reg       = i_var_logic;
    o_integer   = 32'd0;
    o_real      = 0.0;
  end

endmodule

// =====================================================================
// Section 2: non-ANSI ports -- type given directly on the port_declaration
// =====================================================================
module nonansi_ports_typed (i_wire, i_var, o_reg, o_wire, io_wire);

  input  wire       i_wire;   // net-kind non-ANSI port declaration
  input  var  logic i_var;    // variable-kind non-ANSI port declaration
  output reg        o_reg;    // "reg" forces Variable even without var
  output wire       o_wire;
  inout  wire       io_wire;

  assign o_wire = i_wire;
  assign io_wire = 1'bz;

  always_comb begin
    o_reg = i_var;
  end

endmodule

// =====================================================================
// Section 3: non-ANSI ports -- bare port name in the header, actual
// net/variable object supplied by a separate "companion" declaration
// in the module body (classic Verilog-1995 split style).
// =====================================================================
module nonansi_ports_companion (i1, i2, o1, o2, io1);

  input  i1;   // no type in the port declaration itself
  input  i2;
  output o1;
  output o2;
  inout  io1;

  // Companion declarations resolve the actual net/variable kind.
  wire  i1;    // net companion -> Net
  logic i2;    // var companion (no net_type keyword) -> Variable
  reg   o1;    // var companion -> Variable
  wire  o2;    // net companion -> Net
  tri   io1;   // net companion (inout must be a net) -> Net

  assign o2  = i1;
  assign io1 = 1'bz;

  always_comb begin
    o1 = i2;
  end

endmodule

// =====================================================================
// Section 4: every net_type keyword, plus net + vector/signed permutations
// =====================================================================
module all_net_types;

  supply0 n_supply0;               // net declaration
  supply1 n_supply1;               // net declaration
  tri     n_tri;                   // net declaration
  triand  n_triand;                // net declaration
  trior   n_trior;                 // net declaration
  trireg  n_trireg;                // net declaration
  tri0    n_tri0;                  // net declaration
  tri1    n_tri1;                  // net declaration
  uwire   n_uwire;                 // net declaration
  wire    n_wire;                  // net declaration
  wand    n_wand;                  // net declaration
  wor     n_wor;                   // net declaration

  wire        [7:0] n_wire_bus;    // net_type + vector
  wire  logic        n_wire_logic;  // net_type + explicit data_type
  tri   signed [3:0] n_tri_signed; // net_type + signing + vector

  assign n_wire       = 1'b0;
  assign n_wire_bus    = 8'hFF;
  assign n_wire_logic  = n_wire;
  assign n_uwire       = n_wire ^ n_wire_logic;
  assign n_tri         = 1'bz;
  assign n_triand      = 1'b1;
  assign n_trior       = 1'b0;
  assign n_tri0        = 1'bz;
  assign n_tri1        = 1'bz;
  assign n_wand        = 1'b1;
  assign n_wor         = 1'b0;
  assign n_tri_signed  = 4'sh0;

endmodule

// =====================================================================
// Section 5: every variable data type, plus "var" keyword permutations
// =====================================================================
module all_var_types;

  typedef struct packed {
    logic [3:0] hi;
    logic [3:0] lo;
  } pair_t;

  logic            v_logic;         // variable declaration
  reg              v_reg;           // variable declaration
  bit              v_bit;           // variable declaration
  byte             v_byte;          // variable declaration
  shortint         v_shortint;      // variable declaration
  int              v_int;           // variable declaration
  longint          v_longint;       // variable declaration
  integer          v_integer;       // variable declaration
  time             v_time;          // variable declaration
  real             v_real;          // variable declaration
  realtime         v_realtime;      // variable declaration
  shortreal        v_shortreal;     // variable declaration
  string           v_string;        // variable declaration
  chandle          v_chandle;       // variable declaration
  event            v_event;         // variable declaration
  enum logic [1:0] {V_IDLE, V_BUSY} v_enum; // variable declaration
  pair_t           v_struct;        // variable declaration (user-defined type)
  logic     [3:0]  v_vector;        // variable declaration + packed dimension
  reg       [7:0]  v_reg_vector;    // variable declaration + packed dimension
  logic [3:0][7:0] v_packed_2d;     // variable declaration + multiple packed dims
  int              v_unpacked_array [0:3]; // variable declaration + unpacked dimension
  var logic        v_var_logic;     // explicit "var" + explicit data type
  var              v_var_implicit;  // explicit "var" + implicit data type (-> logic)

  initial begin
    v_logic         = 1'b0;
    v_reg           = v_logic;
    v_bit           = 1'b1;
    v_byte          = 8'sd0;
    v_shortint      = 16'sd0;
    v_int           = 32'sd0;
    v_longint       = 64'sd0;
    v_integer       = 32'sd0;
    v_time          = 64'd0;
    v_real          = 0.0;
    v_realtime      = 0.0;
    v_shortreal     = 0.0;
    v_string        = "netvar";
    v_chandle       = null;
    v_enum          = V_IDLE;
    v_struct.hi     = 4'h0;
    v_struct.lo     = 4'h0;
    v_vector        = 4'h0;
    v_reg_vector    = 8'h0;
    v_packed_2d     = '0;
    v_unpacked_array[0] = 0;
    v_var_logic     = 1'b0;
    v_var_implicit  = 1'b0;
    -> v_event;
  end

endmodule
