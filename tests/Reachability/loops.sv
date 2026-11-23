// Instantiation loops (IEEE 1800-2023 Sec 23.3.1, 27.2). A module that instantiates itself,
// directly or through others, with no generate construct to end the recursion, can never be
// instantiated: each is an error. Recursion through a generate conditional (Sec 27.2: generate
// constructs "make recursive module instantiation possible") is legal and is not reported.

module self_loop;
  self_loop u_self ();
endmodule

module loop_a;
  loop_b u_b ();
endmodule

module loop_b;
  loop_a u_a ();
endmodule

// The same through a bind directive: bind_src is instantiated into bind_host, which bind_src
// itself instantiates.
module bind_host;
endmodule

module bind_src;
  bind_host u_host ();
endmodule

bind bind_host bind_src u_src ();

// Recursion ended by a generate condition: legal.
module rec #(
    parameter int N = 3
);
  if (N > 0) begin : g_rec
    rec #(N - 1) u_rec ();
  end
endmodule

module loop_top;
  rec u_rec ();
endmodule
