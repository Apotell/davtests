module tb;
  bit clk; always #5 clk = ~clk;
  int x;

  function void f(); x++; endfunction

  sequence seq5;
    (1, x=0, f()); // match items
  endsequence

  assert property(@(posedge clk) seq5);

  initial begin
    #20 $finish;
  end
endmodule