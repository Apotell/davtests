// IEEE 1800-2023 Sec 16.9.1 Table 16-1: sequence operator precedence and associativity,
// highest first:
//   [* ] [= ] [-> ]    --
//   ##                 left
//   throughout         right
//   within             left
//   intersect          left
//   and                left
//   or                 left

module sequence_precedence;

  logic a, b, c, d;

  // ## binds tighter than and / or
  sequence delay_and;     a ##1 b and c;            endsequence  // (a ##1 b) and c
  sequence delay_or;      a ##1 b or c ##1 d;       endsequence  // (a ##1 b) or (c ##1 d)
  sequence and_delay;     a and b ##1 c;            endsequence  // a and (b ##1 c)

  // ## is left associative
  sequence delay_chain;   a ##1 b ##2 c;            endsequence  // (a ##1 b) ##2 c
  sequence unary_chain;   ##1 a ##2 b;              endsequence  // (##1 a) ##2 b

  // explicit parentheses keep their grouping
  sequence delay_paren;   a ##1 (b ##2 c);          endsequence  // a ##1 (b ##2 c)

  // intersect > and > or
  sequence and_intersect; a and b intersect c;      endsequence  // a and (b intersect c)
  sequence or_and;        a or b and c;             endsequence  // a or (b and c)
  sequence and_chain;     a and b and c;            endsequence  // (a and b) and c

  // within > intersect
  sequence intersect_within; a intersect b within c; endsequence  // a intersect (b within c)

  // throughout > within, ## > throughout, throughout is right associative
  sequence throughout_within; a throughout b within c;  endsequence  // (a throughout b) within c
  sequence throughout_delay;  a throughout b ##1 c;     endsequence  // a throughout (b ##1 c)
  sequence throughout_chain;  a throughout b throughout c; endsequence  // a throughout (b throughout c)

  // repetition binds tighter than ##
  sequence rep_delay;     a[*2] ##1 b;              endsequence  // (a[*2]) ##1 b

endmodule
