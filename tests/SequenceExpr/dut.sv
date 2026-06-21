// IEEE 1800-2017 - 16.8 sequence_expr - all grammar alternatives and permutations.
//
// Grammar alternatives (in order):
//   Alt 1:  cycle_delay_range sequence_expr (cycle_delay_range sequence_expr)*
//   Alt 2:  sequence_expr cycle_delay_range sequence_expr (cycle_delay_range sequence_expr)*
//   Alt 3:  expression_or_dist boolean_abbrev?
//   Alt 4:  ( expression_or_dist (, sequence_match_item)* ) boolean_abbrev?
//   Alt 5:  sequence_instance consecutive_repetition?
//   Alt 6:  ( sequence_expr (, sequence_match_item)* ) consecutive_repetition?
//   Alt 7:  sequence_expr AND sequence_expr
//   Alt 8:  sequence_expr INTERSECT sequence_expr
//   Alt 9:  sequence_expr OR sequence_expr
//   Alt 10: first_match ( sequence_expr (, sequence_match_item)* )
//   Alt 11: expression_or_dist THROUGHOUT sequence_expr
//   Alt 12: sequence_expr WITHIN sequence_expr
//   Alt 13: clocking_event sequence_expr

module sequence_expr_coverage;

  logic        clk, clk2;
  logic        a, b, c, d, e;
  logic [3:0]  data;
  int          cnt;

  // Named sequences used in Alt 5 (sequence_instance)
  sequence seq_a;              a;           endsequence
  sequence seq_ab;             a ##1 b;     endsequence
  sequence seq_abc;            a ##1 b ##1 c; endsequence
  sequence seq_with_arg(logic x); x;        endsequence

  // ==========================================================================
  // Alt 1 - leading cycle_delay_range  (##N  before  sequence_expr)
  // ==========================================================================

  // 1a. single fixed leading delay
  sequence alt1_fixed;
    ##2 a;
  endsequence

  // 1b. single range leading delay  ##[N:M]
  sequence alt1_range;
    ##[1:3] a;
  endsequence

  // 1c. leading ##[*]  (0 to $)
  sequence alt1_star;
    ##[*] a;
  endsequence

  // 1d. leading ##[+]  (1 to $)
  sequence alt1_plus;
    ##[+] a;
  endsequence

  // 1e. chained leading delays: ##1 (##2 a)
  sequence alt1_chained;
    ##1 ##2 a;
  endsequence

  // 1f. two leading-delay groups: ##1 a ##2 b  (inner group is Alt 2)
  sequence alt1_then_delay;
    ##1 a ##[1:2] b;
  endsequence

  // ==========================================================================
  // Alt 2 - cycle_delay_range between operands
  // ==========================================================================

  // 2a. fixed delay  ##N
  sequence alt2_fixed;
    a ##1 b;
  endsequence

  // 2b. range delay  ##[N:M]
  sequence alt2_range;
    a ##[1:4] b;
  endsequence

  // 2c. unbounded  ##[*]  (##[0:$])
  sequence alt2_star;
    a ##[*] b;
  endsequence

  // 2d. one-or-more  ##[+]  (##[1:$])
  sequence alt2_plus;
    a ##[+] b;
  endsequence

  // 2e. three-term chain, homogeneous delays
  sequence alt2_chain3;
    a ##1 b ##1 c;
  endsequence

  // 2f. four-term chain, mixed delay types
  sequence alt2_chain4_mixed;
    a ##1 b ##[1:2] c ##[+] d;
  endsequence

  // 2g. zero-delay  ##0  (concurrent start)
  sequence alt2_zero;
    a ##0 b;
  endsequence

  // ==========================================================================
  // Alt 3 - expression_or_dist  boolean_abbrev?
  // ==========================================================================

  // 3a. bare expression, no repetition
  sequence alt3_bare;
    a;
  endsequence

  // 3b. expression_or_dist with dist clause, no repetition
  sequence alt3_dist;
    (data dist { 0 := 1, [1:3] :/ 3 });
  endsequence

  // - consecutive_repetition  [* …] ----------------------------------------

  // 3c. [* N]  exact count
  sequence alt3_consec_exact;
    a[*3];
  endsequence

  // 3d. [* N:M]  bounded range
  sequence alt3_consec_range;
    a[*2:5];
  endsequence

  // 3e. [*]  zero-or-more
  sequence alt3_consec_star;
    a[*];
  endsequence

  // 3f. [+]  one-or-more
  sequence alt3_consec_plus;
    a[+];
  endsequence

  // 3g. [* N:$]  unbounded upper
  sequence alt3_consec_unbounded;
    a[*1:$];
  endsequence

  // - non_consecutive_repetition  [= …] -------------------------------------

  // 3h. [= N]  exact non-consecutive
  sequence alt3_nonconsec_exact;
    a[=2];
  endsequence

  // 3i. [= N:M]  bounded range non-consecutive
  sequence alt3_nonconsec_range;
    a[=1:4];
  endsequence

  // 3j. [= N:$]  unbounded non-consecutive
  sequence alt3_nonconsec_unbounded;
    a[=1:$];
  endsequence

  // - goto_repetition  [-> …] ------------------------------------------------

  // 3k. [-> N]  exact goto
  sequence alt3_goto_exact;
    a[->1];
  endsequence

  // 3l. [-> N:M]  bounded range goto
  sequence alt3_goto_range;
    a[->2:4];
  endsequence

  // 3m. [-> N:$]  unbounded goto
  sequence alt3_goto_unbounded;
    a[->1:$];
  endsequence

  // ==========================================================================
  // Alt 4 - ( expression_or_dist (, sequence_match_item)* )  boolean_abbrev?
  //   Parenthesised expression with optional side-effect match items.
  //   sequence_match_item = operator_assignment | inc_or_dec_expression | func_call
  // ==========================================================================

  // 4a. parenthesised expression, no match items, no abbrev
  sequence alt4_paren_only;
    (a);
  endsequence

  // 4b. single operator_assignment match item
  sequence alt4_assign;
    (a, cnt = 0);
  endsequence

  // 4c. compound assignment match item
  sequence alt4_compound_assign;
    (a, cnt += 1);
  endsequence

  // 4d. inc_or_dec_expression match item (post-increment)
  sequence alt4_post_inc;
    (a, cnt++);
  endsequence

  // 4e. inc_or_dec_expression match item (pre-decrement)
  sequence alt4_pre_dec;
    (a, --cnt);
  endsequence

  // 4f. multiple match items
  sequence alt4_multi_match;
    (a, cnt += 1, b = 0);
  endsequence

  // 4g. with consecutive_repetition boolean_abbrev  [* N]
  sequence alt4_consec_abbrev;
    (a, cnt += 1)[*3];
  endsequence

  // 4h. with consecutive range boolean_abbrev  [* N:M]
  sequence alt4_consec_range_abbrev;
    (a, cnt += 1)[*1:4];
  endsequence

  // 4i. with non_consecutive boolean_abbrev  [= N]
  sequence alt4_nonconsec_abbrev;
    (a, b = 1)[=2];
  endsequence

  // 4j. with goto boolean_abbrev  [-> N]
  sequence alt4_goto_abbrev;
    (a, b = 1)[->1];
  endsequence

  // 4k. dist expression with match item and boolean_abbrev
  sequence alt4_dist_match_abbrev;
    (data dist { 0 := 1, 1 := 1 }, cnt += 1)[*2:4];
  endsequence

  // ==========================================================================
  // Alt 5 - sequence_instance  consecutive_repetition?
  //   Refers to a named sequence declaration.
  // ==========================================================================

  // 5a. no repetition
  sequence alt5_no_rep;
    seq_a;
  endsequence

  // 5b. named sequence with formal argument
  sequence alt5_param;
    seq_with_arg(a);
  endsequence

  // 5c. [* N]  exact consecutive repetition
  sequence alt5_consec_exact;
    seq_ab[*3];
  endsequence

  // 5d. [* N:M]  range consecutive repetition
  sequence alt5_consec_range;
    seq_ab[*1:4];
  endsequence

  // 5e. [*]  zero-or-more consecutive
  sequence alt5_star;
    seq_ab[*];
  endsequence

  // 5f. [+]  one-or-more consecutive
  sequence alt5_plus;
    seq_ab[+];
  endsequence

  // ==========================================================================
  // Alt 6 - ( sequence_expr (, sequence_match_item)* )  consecutive_repetition?
  //   The inner expression is a full sequence_expr (may contain ##).
  // ==========================================================================

  // 6a. parenthesised sequence, no match items, no repetition
  sequence alt6_paren_seq;
    (a ##1 b);
  endsequence

  // 6b. with operator_assignment match item, no repetition
  sequence alt6_with_match;
    (a ##1 b, cnt = 0);
  endsequence

  // 6c. with match item and [* N] consecutive repetition
  sequence alt6_match_consec_exact;
    (a ##1 b, cnt += 1)[*2];
  endsequence

  // 6d. with match item and [* N:M] range
  sequence alt6_match_consec_range;
    (a ##[1:2] b, cnt += 1)[*1:3];
  endsequence

  // 6e. [*]  zero-or-more
  sequence alt6_star;
    (a ##1 b)[*];
  endsequence

  // 6f. [+]  one-or-more
  sequence alt6_plus;
    (a ##1 b)[+];
  endsequence

  // 6g. complex inner sequence with multiple match items
  sequence alt6_complex;
    (a ##[1:2] b ##1 c, cnt = 0, d = 1)[*2];
  endsequence

  // ==========================================================================
  // Alt 7 - sequence_expr AND sequence_expr
  // ==========================================================================

  // 7a. simple signals
  sequence alt7_simple;
    a and b;
  endsequence

  // 7b. both operands are sequenced expressions
  sequence alt7_sequenced;
    (a ##1 b) and (c ##1 d);
  endsequence

  // 7c. left-associative chain  (a and b) and c
  sequence alt7_chained;
    a and b and c;
  endsequence

  // 7d. AND of named sequence instances
  sequence alt7_instances;
    seq_a and seq_ab;
  endsequence

  // ==========================================================================
  // Alt 8 - sequence_expr INTERSECT sequence_expr
  // ==========================================================================

  // 8a. simple signals
  sequence alt8_simple;
    a intersect b;
  endsequence

  // 8b. both operands are sequenced with range delays
  sequence alt8_sequenced;
    (a ##[1:2] b) intersect (c ##[1:3] d);
  endsequence

  // 8c. intersection with repetition on one side
  sequence alt8_with_rep;
    a[*3] intersect (b ##1 c ##1 d);
  endsequence

  // ==========================================================================
  // Alt 9 - sequence_expr OR sequence_expr
  // ==========================================================================

  // 9a. simple signals
  sequence alt9_simple;
    a or b;
  endsequence

  // 9b. both operands are sequenced
  sequence alt9_sequenced;
    (a ##1 b) or (c ##2 d);
  endsequence

  // 9c. left-associative chain
  sequence alt9_chained;
    a or b or c;
  endsequence

  // 9d. OR of named sequence instances
  sequence alt9_instances;
    seq_a or seq_ab;
  endsequence

  // ==========================================================================
  // Alt 10 - first_match ( sequence_expr (, sequence_match_item)* )
  // ==========================================================================

  // 10a. no match items
  sequence alt10_simple;
    first_match(a ##1 b);
  endsequence

  // 10b. one operator_assignment match item
  sequence alt10_one_match;
    first_match(a ##[1:3] b, cnt = 0);
  endsequence

  // 10c. multiple match items
  sequence alt10_multi_match;
    first_match(a ##1 b ##1 c, cnt = 0, d = 1);
  endsequence

  // 10d. inner sequence uses range delay and repetition
  sequence alt10_inner_complex;
    first_match(a ##[0:5] b[*2]);
  endsequence

  // ==========================================================================
  // Alt 11 - expression_or_dist THROUGHOUT sequence_expr
  // ==========================================================================

  // 11a. plain expression throughout a sequenced pair
  sequence alt11_simple;
    a throughout (b ##1 c);
  endsequence

  // 11b. expression throughout a delay-chained sequence
  sequence alt11_chain;
    a throughout (b ##1 c ##1 d);
  endsequence

  // 11c. expression throughout a range-delay sequence
  sequence alt11_range;
    (data == 4'hF) throughout (b ##[1:5] c);
  endsequence

  // 11e. expression throughout a named sequence instance
  sequence alt11_instance;
    a throughout seq_abc;
  endsequence

  // ==========================================================================
  // Alt 12 - sequence_expr WITHIN sequence_expr
  // ==========================================================================

  // 12a. simple expression within a range-delay window
  sequence alt12_simple;
    a within (b ##[0:5] c);
  endsequence

  // 12b. both operands are sequenced
  sequence alt12_sequenced;
    (a ##1 b) within (c ##[1:10] d);
  endsequence

  // 12c. single cycle within a wider window
  sequence alt12_window;
    b within (a ##[0:3] c);
  endsequence

  // 12d. named sequence within a range-delay sequence
  sequence alt12_instance_within;
    seq_ab within (a ##[0:8] c);
  endsequence

  // ==========================================================================
  // Alt 13 - clocking_event sequence_expr
  // ==========================================================================

  // 13a. posedge clocking event
  sequence alt13_posedge;
    @(posedge clk) a;
  endsequence

  // 13b. negedge clocking event
  sequence alt13_negedge;
    @(negedge clk) a;
  endsequence

  // 13c. edge (any-transition) clocking event
  sequence alt13_edge;
    @(edge clk) a;
  endsequence

  // 13d. clocking event applied to a sequenced expression
  sequence alt13_sequenced;
    @(posedge clk) (a ##1 b);
  endsequence

  // 13e. clocking event within a delay chain
  sequence alt13_in_chain;
    @(posedge clk) a ##1 b ##2 c;
  endsequence

  // 13f. domain switch mid-sequence (different clocks on each side)
  sequence alt13_domain_switch;
    @(posedge clk) a ##1 @(posedge clk2) b;
  endsequence

  // 13g. clocking event applied to an AND expression
  sequence alt13_with_and;
    @(posedge clk) (a and b);
  endsequence

  // ==========================================================================
  // Combined / nested permutations
  // ==========================================================================

  // AND + OR  (parentheses control precedence)
  sequence combo_and_or;
    (a and b) or (c and d);
  endsequence

  // THROUGHOUT + WITHIN  (nested)
  sequence combo_throughout_within;
    a throughout (b within (c ##[0:5] d));
  endsequence

  // first_match + AND
  sequence combo_first_match_and;
    first_match(a ##[1:3] b) and (c ##1 d);
  endsequence

  // clocking event + OR
  sequence combo_clk_or;
    @(posedge clk) (a or b);
  endsequence

  // leading delay + named instance with repetition
  sequence combo_leading_inst_rep;
    ##1 seq_a[*2] ##1 b;
  endsequence

  // sequence instance used inside throughout
  sequence combo_inst_throughout;
    (data == 4'h0) throughout seq_ab[*2:4];
  endsequence

  // first_match inside AND
  sequence combo_fm_intersect;
    first_match(a ##[0:3] b) intersect (c ##[0:3] d);
  endsequence

  // parenthesised group with match items then WITHIN outer
  sequence combo_group_within;
    (a ##1 b, cnt++)[*2] within (c ##[0:10] e);
  endsequence

  // THROUGHOUT with repeated named instance on the right
  sequence combo_throughout_rep_inst;
    a throughout seq_a[*3];
  endsequence

  // Clocked THROUGHOUT
  sequence combo_clk_throughout;
    @(posedge clk) (a throughout (b ##1 c));
  endsequence

  // ==========================================================================
  // Concurrent assertions that exercise each alternative directly
  // ==========================================================================

  // Alt 1
  ap_alt1_fixed:   assert property (@(posedge clk) ##2 a);
  ap_alt1_range:   assert property (@(posedge clk) ##[1:3] a);
  ap_alt1_star:    assert property (@(posedge clk) ##[*] a);
  ap_alt1_plus:    assert property (@(posedge clk) ##[+] a);

  // Alt 2
  ap_alt2_fixed:   assert property (@(posedge clk) a ##1 b);
  ap_alt2_range:   assert property (@(posedge clk) a ##[1:4] b);
  ap_alt2_chain:   assert property (@(posedge clk) a ##1 b ##[1:2] c ##[+] d);

  // Alt 3 - consecutive repetition
  ap_alt3_cx:      assert property (@(posedge clk) a[*3] |-> b);
  ap_alt3_cr:      assert property (@(posedge clk) a[*1:5] |-> b);
  ap_alt3_cstar:   assert property (@(posedge clk) a[*] |-> b);
  ap_alt3_cplus:   assert property (@(posedge clk) a[+] |-> b);
  // Alt 3 - non-consecutive repetition
  ap_alt3_ncx:     assert property (@(posedge clk) a ##1 b[=2] ##1 c);
  ap_alt3_ncr:     assert property (@(posedge clk) a ##1 b[=1:3] ##1 c);
  // Alt 3 - goto repetition
  ap_alt3_gx:      assert property (@(posedge clk) a ##1 b[->1] ##0 c);
  ap_alt3_gr:      assert property (@(posedge clk) a ##1 b[->1:3] ##0 c);

  // Alt 4
  ap_alt4_assign:  assert property (@(posedge clk) (a, cnt = 0)[*2] |-> b);
  ap_alt4_inc:     assert property (@(posedge clk) (a, cnt++)[*3] |-> b);
  ap_alt4_nc:      assert property (@(posedge clk) (a, b = 1)[=2] ##1 c);
  ap_alt4_goto:    assert property (@(posedge clk) (a, b = 1)[->1] ##0 c);

  // Alt 5
  ap_alt5_inst:    assert property (@(posedge clk) seq_ab |-> c);
  ap_alt5_rep:     assert property (@(posedge clk) seq_a[*2] |-> b);
  ap_alt5_star:    assert property (@(posedge clk) seq_a[*] |-> b);

  // Alt 6
  ap_alt6_match:   assert property (@(posedge clk) (a ##1 b, cnt = 0)[*2] |-> c);
  ap_alt6_plus:    assert property (@(posedge clk) (a ##1 b)[+] |-> c);

  // Alt 7
  ap_alt7:         assert property (@(posedge clk) a |-> (b and c));
  ap_alt7_seq:     assert property (@(posedge clk) a |-> ((a ##1 b) and (c ##1 d)));

  // Alt 8
  ap_alt8:         assert property (@(posedge clk) a |-> (b intersect c));
  ap_alt8_seq:     assert property (@(posedge clk) a |-> ((a ##[1:2] b) intersect (c ##[1:2] d)));

  // Alt 9
  ap_alt9:         assert property (@(posedge clk) (a or b) |-> c);
  ap_alt9_chain:   assert property (@(posedge clk) (a or b or c) |-> d);

  // Alt 10
  ap_alt10:        assert property (@(posedge clk) first_match(a ##[1:3] b) |-> c);
  ap_alt10_match:  assert property (@(posedge clk) first_match(a ##1 b, cnt = 0) |-> c);

  // Alt 11
  ap_alt11:        assert property (@(posedge clk) a throughout (b ##1 c));
  ap_alt11_dist:   assert property (@(posedge clk) (data == 4'hF) throughout (b ##[1:5] c));

  // Alt 12
  ap_alt12:        assert property (@(posedge clk) b within (a ##[0:5] c));
  ap_alt12_seq:    assert property (@(posedge clk) (a ##1 b) within (c ##[1:10] d));

  // Alt 13
  ap_alt13_pos:    assert property (@(posedge clk) @(posedge clk) a ##1 b);
  ap_alt13_neg:    assert property (@(posedge clk) @(negedge clk) a ##1 b);
  ap_alt13_switch: assert property (@(posedge clk) a ##1 @(posedge clk2) b);

  // Combined
  ap_combo_and_or:        assert property (@(posedge clk) (a and b) or (c and d));
  ap_combo_throughout_within: assert property (@(posedge clk)
                                 a throughout (b within (c ##[0:5] d)));
  ap_combo_fm_and:        assert property (@(posedge clk)
                                 first_match(a ##[1:3] b) and (c ##1 d));

endmodule
