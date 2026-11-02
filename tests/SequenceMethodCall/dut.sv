// IEEE 1800-2023 Sec 16.13.6 sequence methods (triggered / matched) used inside
// a sequence_expr, with and without a repetition.
//
//   sequence_expr ::= ... | sequence_method_call [ boolean_abbrev ] | ...   (Annex A.2.10)
//   sequence_method_call ::= sequence_instance . method_identifier

module sequence_method_call;

  logic clk, a, b, c;

  sequence s1;
    @(posedge clk) a ##1 b;
  endsequence

  // method call as a plain sequence operand
  sequence trig_plain;
    s1.triggered ##1 c;
  endsequence

  // method call with a consecutive repetition
  sequence trig_rep;
    s1.triggered[*2] ##1 c;
  endsequence

  // matched (multiclock form, Sec 16.13.5) with a goto repetition
  sequence match_goto;
    @(posedge clk) s1.matched[->1] ##1 c;
  endsequence

endmodule
