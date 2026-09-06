/*
:name: chapter19_error_rules
:description: IEEE 1800-2023 Clause 19 (Functional coverage) error scenarios
:tags: 19.5 19.5.7 19.7
*/

// catalog row 688 | 19.5 | COMP
// Bins shall not be automatically created for coverpoints of real
// expressions; therefore a coverpoint of a real expression shall specify at
// least one explicit bins construct.
module r688_m;
  bit clk;
  real r;
  covergroup r688_cg @(posedge clk);
    cpr: coverpoint r;
  endgroup
endmodule

// catalog row 705 | 19.5.7 | COMP
// An implementation shall issue a warning if the effective type of the
// coverpoint expression is unsigned and a bins expression b is signed with a
// negative value; the offending element does not participate in the bins
// values.
module r705_m;
  bit clk;
  bit [2:0] p1;
  covergroup r705_g1 @(posedge clk);
    coverpoint p1 { bins b2 = {-1, [1:7]}; }
  endgroup
endmodule

// catalog row 720 | 19.7 | COMP
// The per_instance and get_inst_coverage options can only be set in the
// covergroup definition; they cannot be assigned procedurally after
// instantiation.
module r720_m;
  bit clk;
  int x;
  covergroup r720_cg @(posedge clk);
    coverpoint x;
  endgroup
  r720_cg ci = new;
  initial ci.option.per_instance = 1;
endmodule

// catalog row 723 | 19.7 | COMP
// When option.detect_overlap is true, a warning is issued if there is an
// overlap between the range list (or transition list) of two bins of a
// coverpoint.
module r723_m;
  bit clk;
  bit [7:0] v;
  covergroup r723_cg @(posedge clk);
    coverpoint v {
      option.detect_overlap = 1;
      bins b1 = {[0:10]};
      bins b2 = {[5:20]};
    }
  endgroup
endmodule
