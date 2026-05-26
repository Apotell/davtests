module tb;
  bit clk; always #5 clk = ~clk;
  bit a,b;

  sequence seq_open;
    a ##[2:$] b;  // delay >= 2
  endsequence

  assert property(@(posedge clk) seq_open);

  initial begin
    #10 a=1;
    #40 b=1;
    #60 $finish;
  end
endmodule