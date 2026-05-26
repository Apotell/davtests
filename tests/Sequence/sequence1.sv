module tb;
  bit clk; always #5 clk = ~clk;
  bit a, b;

  sequence seq1;
    ##1 a ##2 b; // delay before sequence elements
  endsequence

  property p; @(posedge clk) seq1; endproperty
  assert property(p);

  initial begin
    a=0; b=0;
    #10 a=1;
    #20 b=1;
    #50 $finish;
  end
endmodule