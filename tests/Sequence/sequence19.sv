module tb;
  bit clk; always #5 clk = ~clk;
  bit a;

  sequence seq_star;
    a[*];  // zero or more repetitions
  endsequence

  assert property(@(posedge clk) seq_star);

  initial begin
    #10 a=1;
    #10 a=0;
    #40 $finish;
  end
endmodule