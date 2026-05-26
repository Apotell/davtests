module tb;
  bit clk; always #5 clk = ~clk;
  bit a;

  sequence seq_goto;
    a[->3];  // a must occur exactly 3 times (not necessarily consecutive)
  endsequence

  assert property(@(posedge clk) seq_goto);

  initial begin
    a=0;
    #10 a=1;
    #10 a=0;
    #10 a=1;
    #10 a=0;
    #10 a=1;
    #50 $finish;
  end
endmodule