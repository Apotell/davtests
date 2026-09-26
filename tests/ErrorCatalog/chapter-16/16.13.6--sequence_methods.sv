/*
:name: sequence_methods
:description: IEEE 1800-2023 Sec 16.13.6 sequence methods triggered and matched
:tags: 16.13.6
*/

// Google has no coverage of either sequence method -- nothing under tests/Google calls
// .triggered or .matched on a sequence at all -- so this file covers both.
//
// The two are not interchangeable, and the file exercises each only where the standard
// allows it. 16.13.6: triggered "may be used in wait statements or Boolean expressions
// outside a sequence context", while matched "can only be used in sequence expressions".
// Using matched in a plain Boolean context is illegal and HLC rejects it (CP5929), so it
// appears here only inside a multiclocked sequence, which is the case 16.13.6 describes.

module top();
  bit clk1, clk2, a, b, c, x;

  sequence s1;
    @(posedge clk1) a ##1 b;
  endsequence

  // matched: the source sequence's end point referenced from a destination sequence on a
  // different clock.
  sequence s2;
    @(posedge clk2) s1.matched ##1 c;
  endsequence

  // triggered: a Boolean expression outside any sequence context.
  always @(posedge clk1)
    if (s1.triggered) x <= 1;

  // triggered again, this time in the wait form 16.13.6 calls out.
  initial begin
    wait (s1.triggered);
    x <= 0;
  end

  assert property (@(posedge clk2) s2);
endmodule
