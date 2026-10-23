// IEEE 1800-2023 Sec 37.15 vpiGeneric: TRUE for the ref obj of a generic
// interface port ("interface" keyword, Sec 25.3.3), FALSE for a named one.

interface ifc;
  logic x;
  modport mp(input x);
endinterface

module top(interface gen, interface.mp gen_mp, ifc.mp named_mp, ifc named);
endmodule
