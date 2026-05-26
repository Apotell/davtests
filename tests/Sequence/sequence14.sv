module tb;
  bit clk; always #5 clk = ~clk;
  int x;

  function void inc(); x++; endfunction

  sequence seq14;
    (1, x=0, x++, inc());
  endsequence

  assert property(@(posedge clk) seq14);

  initial begin
    #20 $finish;
  end
endmodule