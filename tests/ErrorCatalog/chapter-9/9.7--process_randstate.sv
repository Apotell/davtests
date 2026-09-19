/*
:name: process_randstate
:description: IEEE 1800-2023 Sec 9.7 process methods the chapter-9 Google tests do not reach
:tags: 9.7
*/

// Google/chapter-9's four 9.7 tests call self, status, kill, await, suspend and resume, but
// none of the three random-state methods the 9.7 process prototype also declares. Those are
// what this file exercises.

module top();
  process p;
  string  st;

  initial begin
    p = process::self();

    p.srandom(1);          // function void srandom(int seed)
    st = p.get_randstate(); // function string get_randstate()
    p.set_randstate(st);    // function void set_randstate(string state)
  end
endmodule
