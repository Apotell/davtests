module tb;
  bit clk; always #5 clk = ~clk;
  int a;

  sequence seq15;
    (a dist {1:=3, 2:=1});
  endsequence

  assert property(@(posedge clk) seq15);

  initial begin
    a=1;
    #20 $finish;
  end
endmodule