module tb;
  bit clk; always #5 clk = ~clk;
  bit a;

  sequence seq_plus;
    a[+];  // one or more consecutive
  endsequence

  assert property(@(posedge clk) seq_plus);

  initial begin
    #10 a=1;
    #10 a=1;
    #10 a=1;
    #40 $finish;
  end
endmodule