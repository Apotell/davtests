/*
:name: chapter12_error_rules
:description: IEEE 1800-2023 Clause 12 (Procedural programming statements) error scenarios
:tags: 12.5 12.6 12.7.1 12.8
*/

// catalog row 378 | 12.5 | COMP
// The default case_item is optional, but the use of multiple default
// statements in one case statement shall be illegal.
module r378_m;
  logic [1:0] d;
  int r;
  initial
    case (d)
      2'b00: r = 0;
      default: r = 1;
      default: r = 2;
    endcase
endmodule

// catalog row 380 | 12.6 | COMP
// A constant expression used as a leaf pattern shall be of integral type.
typedef union tagged { real R; int I; } r380_u;
module r380_m;
  r380_u u;
  initial
    case (u) matches
      tagged R 1.5 : ;
      default: ;
    endcase
endmodule

// catalog row 384 | 12.7.1 | COMP
// In a for-loop initialization, either all or none of the control variables
// shall be locally declared; mixing a locally declared variable with an
// assignment to a non-local one is illegal.
module r384_m;
  int x;
  initial
    for (x = 0, int y = 0; x < 4; x++) ;
endmodule

// catalog row 392 | 12.8 | COMP
// In a function returning a value, the return statement shall have an
// expression of the correct type.
module r392_m;
  function int f();
    return;
  endfunction
endmodule
