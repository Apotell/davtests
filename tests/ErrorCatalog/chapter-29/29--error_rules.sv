/*
:name: chapter29_error_rules
:description: IEEE 1800-2023 Clause 29 (User-defined primitives) error scenarios
:tags: 29.3.1 29.3.4
*/

// catalog row 991 | 29.3.1 | COMP
// The output port shall be the first port in the UDP port list.
primitive r991_p (i, o);
  input i;
  output o;
  table
    0 : 1;
  endtable
endprimitive

// catalog row 995 | 29.3.4 | COMP
// Each sequential UDP table row may specify a transition on at most one input;
// a row with two edge specifications is illegal.
//
// Kept last in this file: a malformed table entry is the kind of construct the
// parser recovers from worst, so anything lost is lost from the end.
primitive r995_p (q, a, b);
  output q;
  reg q;
  input a, b;
  table
    (01) (10) : 0 : 1 ;
  endtable
endprimitive
