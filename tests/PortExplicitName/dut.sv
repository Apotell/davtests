// IEEE 1800-2023 Sec 37.14 vpiExplicitName: true for a port written in the
// ".port_identifier ( [port_expression] )" form (Sec 23.2.2.1, 23.2.2.2).

// Non-ANSI list of ports.
module nonansi_ports(a, .b(c), .d());
  input a, c;
endmodule

// ANSI list of port declarations.
module ansi_ports(input wire e, output .f(g), output .h(), output i);
  wire g;
endmodule
