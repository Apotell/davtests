

module tb9;
  logic clk = 0;
  logic rst_n = 0;
  logic req, gnt, busy;
  logic clk1 = 0;
  logic clk2 = 0;

  logic a, b;

  // Two clocks
  always #5 clk1 = ~clk1;
  always #7 clk2 = ~clk2;
  always #5 clk = ~clk;

  initial begin
    req = 0; gnt = 0; busy = 0;

    #8 rst_n = 1;

    // Valid transaction
    #10 req = 1;
    #10 gnt = 1;
    #10 req = 0; gnt = 0;

    // Busy violation case
    #10 busy = 1;
    #10 req = 1;   // should fail
    #10 busy = 0;
  end

  initial begin
    req = 0; gnt = 0; a = 0; b = 0;

    #10 rst_n = 1;

    // Valid behavior on clk1
    #10 req = 1;
    #10 gnt = 1;
    #10 req = 0; gnt = 0;

    // clk2 behavior
    #14 a = 1;
    #14 b = 1;

    #1000 $finish;
  end

  // ================= CHECKER =================
  checker advanced_chk(input logic clk, rst_n, req, gnt, busy);

    default clocking cb @(posedge clk);
    endclocking

    default disable iff (!rst_n);

    // -------------------------------------------------
    // 1. Sequence with argument
    // -------------------------------------------------
    sequence req_to_gnt(delay);
      req ##[1:delay] gnt;
    endsequence


    // -------------------------------------------------
    // 2. Sequence with local variable + first_match
    // -------------------------------------------------
    sequence capture_req_time;
      int count;
      (req, count = 0)
      ##1 (1, count = count + 1)[*1:$]
      ##0 first_match(gnt);
    endsequence


    // -------------------------------------------------
    // 3. Sequence using throughout
    // -------------------------------------------------
    sequence no_req_during_busy;
      !req throughout busy;
    endsequence


    // -------------------------------------------------
    // 4. Sequence using intersect
    // -------------------------------------------------
    sequence overlap_seq;
      (req ##1 gnt) intersect (req ##[1:2] gnt);
    endsequence


    // -------------------------------------------------
    // 5. Property using sequence (overlapping implication)
    // -------------------------------------------------
    property p1;
      req |-> req_to_gnt(3);
    endproperty


    // -------------------------------------------------
    // 6. Property using non-overlapping implication
    // -------------------------------------------------
    property p2;
      req |=> gnt;
    endproperty


    // -------------------------------------------------
    // 7. Property with argument
    // -------------------------------------------------
    property p_with_arg(int max_delay);
      req |-> ##[1:max_delay] gnt;
    endproperty


    // -------------------------------------------------
    // 8. Property using throughout
    // -------------------------------------------------
    property p_busy;
      busy |-> no_req_during_busy;
    endproperty


    // -------------------------------------------------
    // 9. Property using intersect sequence
    // -------------------------------------------------
    property p_overlap;
      overlap_seq;
    endproperty
    
    // -------------------------------------------------
	// 10. Property using AND (both must pass)
	// -------------------------------------------------
	property p_and;
  		req |-> ( req_to_gnt(3) and ##1 (gnt && !req) );
	endproperty

    // -------------------------------------------------
	// 8. Property using OR (either can pass)
	// -------------------------------------------------
	property p_or;
  		req |-> ( req_to_gnt(3) or ##1 gnt );
	endproperty

    // ================= ASSERTIONS =================
    assert property (p1)
      else $error("p1 failed");

    assert property (p2)
      else $error("p2 failed");

    assert property (p_with_arg(2))
      else $error("p_with_arg failed");

    assert property (p_busy)
      else $error("Request came during busy!");

    assert property (p_overlap)
      else $error("Overlap condition failed");
      
    assert property (p_and)
      else $error("p1 failed");
      
    assert property (p_or)
      else $error("p1 failed");

  endchecker
      
  // ================= CHECKER =================
  checker clock_chk(
    input logic clk1, clk2, rst_n,
    input logic req, gnt, a, b
  );

    // -------------------------------------------------
    // 1. Property with explicit clock
    // -------------------------------------------------
    property p_clk1;
      @(posedge clk1)
      disable iff (!rst_n)
      req |-> ##1 gnt;
    endproperty
    
    assert property (p_clk1)
      else $error("clk1 property failed");

    // -------------------------------------------------
    // 2. Property with different clock (clk2)
    // -------------------------------------------------
    property p_clk2;
      @(posedge clk2)
      a |-> ##1 b;
    endproperty
      
    assert property (p_clk2)
      else $error("clk2 property failed");      

    // -------------------------------------------------
    // 3. Sequence with its own clock
    // -------------------------------------------------
    sequence seq_clk1;
      @(posedge clk1) req ##1 gnt;
    endsequence

    property p_seq_usage;
      seq_clk1;
    endproperty

    assert property (p_seq_usage)
      else $error("sequence clock failed");

    // -------------------------------------------------
    // 4. Clock with condition (gated clocking)
    // -------------------------------------------------
    property p_cond_clock;
      @(posedge clk1 iff rst_n)
      req |-> ##1 gnt;
    endproperty

    assert property (p_cond_clock)
      else $error("conditional clock failed");

    // -------------------------------------------------
    // 5. Multi-clock property 
    //    Start on clk1, check on clk2
    // -------------------------------------------------
    property p_multiclock;
      @(posedge clk1)
      req |-> @(posedge clk2) gnt;
    endproperty

    assert property (p_multiclock)
      else $error("multiclock failed");
      
    // -------------------------------------------------
    // 6. Using different clocks in sequence + property
    // -------------------------------------------------
    sequence cross_seq;
      @(posedge clk1) req ##1
      @(posedge clk2) gnt;
    endsequence

    property p_cross;
      cross_seq;
    endproperty

    assert property (p_cross)
      else $error("cross clock sequence failed");
                  
    // -----------------------------------------
    // Property with event argument
    // -----------------------------------------
    property p_with_clk(event ev);
      @(ev) req |-> ##1 gnt;
    endproperty

  endchecker
      
        checker seq_multi_chk(input logic clk, input logic [3:0] a, b);

    // -----------------------------------------
    // Sequence with multiple arguments
    // -----------------------------------------
    sequence seq_multi(event ev, logic [3:0] x, y, int d);
      @(ev) (x == 5) ##[1:d] (y == x);
    endsequence

    // Using the sequence
    assert property (seq_multi(posedge clk, a, b, 3))
      else $error("Sequence multi-arg failed");

  endchecker
      
  checker mix_chk(input logic clk, req, gnt);

  // Sequence
  sequence handshake_seq(event ev, logic r, g, int d);
    @(ev) r ##[1:d] g;
  endsequence

  // Property using sequence
  property handshake_prop(event ev, logic r, g, int d);
    handshake_seq(ev, r, g, d);
  endproperty

  assert property (handshake_prop(posedge clk, req, gnt, 3))
    else $error("Handshake failed");

endchecker

  seq_multi_chk chk1(clk, a, b);     
  clock_chk chk2(clk1, clk2, rst_n, req, gnt, a, b);     
  advanced_chk chk3(clk, rst_n, req, gnt, busy);
  mix_chk chk4(clk,req,gnt);

endmodule