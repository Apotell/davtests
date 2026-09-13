/*
:name: chapter35_error_rules_inv3
:description: IEEE 1800-2023 35.7 -- a class member function cannot be exported via DPI
:tags: 35.7
:should_fail_because: export declarations are not permitted as class items
*/

// catalog row 1136 | 35.7 | COMP
// Class member functions cannot be exported; all other SystemVerilog
// functions can be.
//
// Kept alone in its own fixture: HLC's grammar does not accept 'export' as
// a class_item at all ("extraneous input 'export'"), which then desyncs the
// parser badly enough that the following 'endclass' is read as extraneous
// input expecting <EOF> -- the whole class is lost. Nothing may be appended
// below.

class r1136_C;
  function int r1136_f(); return 1; endfunction
  export "DPI-C" function r1136_f; // ERROR: class member function cannot be exported
endclass
