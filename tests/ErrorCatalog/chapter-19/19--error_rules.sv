/*
:name: chapter19_error_rules
:description: IEEE 1800-2023 Clause 19 (Functional coverage) error scenarios
:tags: 19.3 19.5 19.5.1.1 19.5.1.2 19.5.7 19.6 19.6.1.2 19.7 19.7.1 19.8.1
*/

// catalog row 672 | 19.3 | COMP
// When a covergroup specifies a list of formal arguments, its instances
// shall provide to the new operator all the actual arguments that are not
// defaulted.
module r672_m;
  bit clk;
  covergroup r672_cg (int a, int b) @(posedge clk);
    coverpoint a;
  endgroup
  r672_cg ci = new(1);
endmodule

// catalog row 680 | 19.5 | COMP
// A coverpoint name has limited visibility: an identifier may refer to a
// coverpoint only in the coverpoint list of a cross declaration, in a
// hierarchical name prefixed by a covergroup variable, or after :: where the
// left operand refers to a covergroup. Any other use is illegal.
module r680_m;
  bit clk;
  int x;
  covergroup r680_cg @(posedge clk);
    r680_e: coverpoint x;
    r680_e.option.weight = 2;
  endgroup
endmodule

// catalog row 682 | 19.5 | LINT
// Global and instance constants referenced from a covergroup_expression
// shall be members of the enclosing class.
class r682_Other;
  const int lim = 7;
endclass
class r682_C;
  bit e;
  int x;
  r682_Other o;
  covergroup r682_cv @e;
    coverpoint x { bins b = {[0:o.lim]}; }
  endgroup
endclass

// catalog row 684 | 19.5 | LINT
// Functions participating in a covergroup_expression shall not contain
// output, inout, or ref arguments (const ref is allowed).
module r684_m;
  bit clk;
  int x;
  function int r684_f(ref int r); return r; endfunction
  int v;
  covergroup r684_cg @(posedge clk);
    coverpoint x { bins b = {[0:r684_f(v)]}; }
  endgroup
endmodule

// catalog row 685 | 19.5 | LINT
// Functions participating in a covergroup_expression shall be automatic (or
// preserve no state information) and have no side effects.
module r685_m;
  bit clk;
  int x;
  function int r685_f(); static int cnt; cnt++; return cnt; endfunction
  covergroup r685_cg @(posedge clk);
    coverpoint x { bins b = {[0:r685_f()]}; }
  endgroup
endmodule

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

// catalog row 694 | 19.5.1.1 | COMP
// When a coverpoint name is used in place of the covergroup_range_list of a
// bin, only the name of the coverpoint containing the bin being defined
// shall be allowed; no other coverpoint names shall be permitted.
module r694_m;
  bit clk;
  int x, y;
  covergroup r694_cg @(posedge clk);
    r694_a: coverpoint x;
    r694_b: coverpoint y { bins f[] = r694_a with (item > 0); }
  endgroup
endmodule

// catalog row 696 | 19.5.1.2 | COMP
// Identifiers declared within the covergroup (such as coverpoint identifiers
// and bin identifiers) are not visible in a set_covergroup_expression.
module r696_m;
  bit clk;
  int x, y;
  covergroup r696_cg @(posedge clk);
    r696_a: coverpoint x;
    r696_b: coverpoint y { bins s[] = r696_a; }
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

// catalog row 706 | 19.5.7 | COMP
// An implementation shall issue a warning if assigning a bins expression b
// to a variable of the effective type of the coverpoint expression would
// yield a value that is not equal to b under normal == comparison rules.
module r706_m;
  bit clk;
  bit [2:0] p1;
  covergroup r706_g1 @(posedge clk);
    coverpoint p1 { bins b1 = {1, [2:5], [6:10]}; }
  endgroup
endmodule

// catalog row 707 | 19.5.7 | COMP
// An implementation shall issue a warning if a bins expression b yields a
// value with any x or z bits (this rule does not apply to wildcard bins).
module r707_m;
  bit clk;
  logic [3:0] p;
  covergroup r707_g1 @(posedge clk);
    coverpoint p { bins bx = {4'b10x1}; }
  endgroup
endmodule

// catalog row 711 | 19.6 | COMP
// A cross name has limited visibility: an identifier may refer to a cross
// only in a hierarchical name prefixed by a covergroup variable, or after ::
// where the left operand refers to a covergroup.
module r711_m;
  bit clk;
  int a, b;
  covergroup r711_cg @(posedge clk);
    r711_ca: coverpoint a;
    r711_cb: coverpoint b;
    r711_crs: cross r711_ca, r711_cb;
    r711_crs.option.weight = 2;
  endgroup
endmodule

// catalog row 713 | 19.6.1.2 | COMP
// When a cross_identifier is used as a select_expression, only the
// cross_identifier of the enclosing cross may be used; other
// cross_identifiers shall be disallowed.
module r713_m;
  bit clk;
  int a, b;
  covergroup r713_cg @(posedge clk);
    r713_ca: coverpoint a;
    r713_cb: coverpoint b;
    r713_X: cross r713_ca, r713_cb;
    r713_Y: cross r713_ca, r713_cb { bins p = r713_X with (r713_ca < r713_cb); }
  endgroup
endmodule

// catalog row 719 | 19.7 | COMP
// The weight specified for option.weight (and type_option.weight) shall be a
// non-negative integral value.
module r719_m;
  bit clk;
  int x;
  covergroup r719_cg @(posedge clk);
    option.weight = -1;
    coverpoint x;
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

// catalog row 721 | 19.7 | COMP
// The auto_bin_max, detect_overlap, and cross_retain_auto_bins options can
// only be set in the covergroup or coverpoint definition, not assigned
// procedurally.
module r721_m;
  bit clk;
  int x;
  covergroup r721_cg @(posedge clk);
    r721_a: coverpoint x;
  endgroup
  r721_cg ci = new;
  initial ci.r721_a.option.auto_bin_max = 128;
endmodule

// catalog row 722 | 19.7 | COMP
// Each instance coverage option may only be specified at the syntactic
// levels permitted by Table 19-2 (e.g., name and per_instance only at the
// covergroup level).
module r722_m;
  bit clk;
  int a, b;
  covergroup r722_cg @(posedge clk);
    r722_ca: coverpoint a { option.name = "cp"; }
    r722_cb: coverpoint b;
  endgroup
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

// catalog row 725 | 19.7.1 | COMP
// The strobe and real_interval type options can only be set in the
// covergroup definition, not assigned procedurally.
module r725_m;
  bit clk;
  int x;
  covergroup r725_cg @(posedge clk);
    coverpoint x;
  endgroup
  initial r725_cg::type_option.strobe = 1;
endmodule

// catalog row 726 | 19.7.1 | COMP
// Each covergroup type option may only be specified at the syntactic levels
// permitted by Table 19-4; real_interval is not allowed in a cross.
module r726_m;
  bit clk;
  int a, b;
  covergroup r726_cg @(posedge clk);
    r726_ca: coverpoint a;
    r726_cb: coverpoint b;
    r726_x: cross r726_ca, r726_cb { type_option.real_interval = 0.5; }
  endgroup
endmodule

// catalog row 728 | 19.8.1 | COMP
// It shall be an error to use a formal argument of an overridden sample
// method in any context other than a coverpoint or a conditional guard
// expression.
module r728_m;
  covergroup r728_C1 with function sample (bit b);
    option.per_instance = b;
  endgroup
endmodule
