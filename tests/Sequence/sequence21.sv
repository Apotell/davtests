module tb;
  bit clk; always #5 clk = ~clk;
  bit a,b;

  sequence my_seq(input bit x, input bit y);
    x ##1 y;
  endsequence

  sequence seq_inst;
    my_seq(a,b);  // positional args
  endsequence

  assert property(@(posedge clk) seq_inst);

  initial begin
    #10 a=1;
    #10 b=1;
    #50 $finish;
  end
endmodule