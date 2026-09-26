/*
:name: builtin_system_tasks_functions
:description: Every system task and system function declared at file scope in
:description: Builtin::getBuiltInClasses(), called once so its vpiTaskFunc binds
:tags: 18.13 19.9 20 21 40.3.2
*/

// The declarations carry no formals -- most of these are variadic, which SV cannot
// express -- so a call passes only what the grammar demands. Only the name is bound.
// A function's result is assigned to a variable of the return type the standard states.
//
// Not called here:
//   - the 12 timing checks of Sec 31.3/31.4 ($fullskew $hold $nochange $period $recovery $recrem $removal $setup $setuphold $skew $timeskew $width)
//     are specify_item productions with their own lexer tokens, legal only inside a
//     specify block, so they cannot appear as an ordinary statement.
//
// This file is EXPECTED TO REPORT ERRORS. $timeunit and $timeprecision (Sec
// 20.4.1) cannot be lexed today, so the golden carries their syntax errors --
// see the KNOWN GAP comment at the call site. That is deliberate: a gap only
// recorded in a comment is a gap nobody sees. When the lexer gains the missing
// tokens these errors disappear and the golden is regenerated.
//
// Never declared, so nothing to call: $system, $cast and $stacktrace (either a task
// or a function), and $signed/$unsigned/$sampled/$past/$past_gclk/$future_gclk, whose
// return type is their argument's.

module builtin_system_tf;
  integer ig; int i; int unsigned iu; real r; shortreal sr;
  string s; bit b; time t; bit [63:0] b64; bit [31:0] b32;
  bit clk;

  // The _gclk sampled-value functions (Sec 16.9.4) require a global clocking
  // block in scope (Sec 14.14); without one they are an error, not a binding.
  global clocking @(posedge clk); endclocking

  initial begin
    // ---- system tasks ----
    $assertcontrol();
    $assertfailoff();
    $assertfailon();
    $assertkill();
    $assertnonvacuouson();
    $assertoff();
    $asserton();
    $assertpassoff();
    $assertpasson();
    $assertvacuousoff();
    $async$and$array();
    $async$and$plane();
    $async$nand$array();
    $async$nand$plane();
    $async$nor$array();
    $async$nor$plane();
    $async$or$array();
    $async$or$plane();
    $display();
    $displayb();
    $displayh();
    $displayo();
    $dumpall();
    $dumpfile();
    $dumpflush();
    $dumplimit();
    $dumpoff();
    $dumpon();
    $dumpports();
    $dumpportsall();
    $dumpportsflush();
    $dumpportslimit();
    $dumpportsoff();
    $dumpportson();
    $dumpvars();
    $error();
    $exit();
    $fatal();
    $fclose();
    $fdisplay();
    $fdisplayb();
    $fdisplayh();
    $fdisplayo();
    $fflush();
    $finish();
    $fmonitor();
    $fmonitorb();
    $fmonitorh();
    $fmonitoro();
    $fstrobe();
    $fstrobeb();
    $fstrobeh();
    $fstrobeo();
    $fwrite();
    $fwriteb();
    $fwriteh();
    $fwriteo();
    $info();
    $load_coverage_db();
    $monitor();
    $monitorb();
    $monitorh();
    $monitoro();
    $monitoroff();
    $monitoron();
    $printtimescale();
    $q_add();
    $q_exam();
    $q_initialize();
    $q_remove();
    $readmemb();
    $readmemh();
    $sdf_annotate();
    $set_coverage_db_name();
    $sformat();
    $stop();
    $strobe();
    $strobeb();
    $strobeh();
    $strobeo();
    $swrite();
    $swriteb();
    $swriteh();
    $swriteo();
    $sync$and$array();
    $sync$and$plane();
    $sync$nand$array();
    $sync$nand$plane();
    $sync$nor$array();
    $sync$nor$plane();
    $sync$or$array();
    $sync$or$plane();
    $timeformat();
    $warning();
    $write();
    $writeb();
    $writeh();
    $writememb();
    $writememh();
    $writeo();

    // ---- system functions ----
    r = $acos();
    r = $acosh();
    r = $asin();
    r = $asinh();
    r = $atan();
    r = $atan2();
    r = $atanh();
    ig = $bits();
    r = $bitstoreal();
    sr = $bitstoshortreal();
    r = $ceil();
    b = $changed();
    b = $changed_gclk(b);
    b = $changing_gclk(b);
    ig = $clog2();
    r = $cos();
    r = $cosh();
    i = $countbits();
    i = $countones();
    ig = $coverage_control();
    ig = $coverage_get();
    ig = $coverage_get_max();
    ig = $coverage_merge();
    ig = $coverage_save();
    ig = $dimensions();
    ig = $dist_chi_square();
    ig = $dist_erlang();
    ig = $dist_exponential();
    ig = $dist_normal();
    ig = $dist_poisson();
    ig = $dist_t();
    ig = $dist_uniform();
    r = $exp();
    b = $falling_gclk(b);
    b = $fell();
    b = $fell_gclk(b);
    ig = $feof();
    ig = $ferror();
    ig = $fgetc();
    ig = $fgets();
    r = $floor();
    ig = $fopen();
    ig = $fread();
    ig = $fscanf();
    ig = $fseek();
    ig = $ftell();
    r = $get_coverage();
    ig = $high();
    r = $hypot();
    ig = $increment();
    b = $isunbounded();
    b = $isunknown();
    r = $itor();
    ig = $left();
    r = $ln();
    r = $log10();
    ig = $low();
    b = $onehot();
    b = $onehot0();
    r = $pow();
    ig = $q_full();
    ig = $random();
    r = $realtime();
    b64 = $realtobits();
    ig = $rewind();
    ig = $right();
    b = $rising_gclk(b);
    b = $rose();
    b = $rose_gclk(b);
    ig = $rtoi();
    s = $sformatf();
    b32 = $shortrealtobits();
    r = $sin();
    r = $sinh();
    ig = $size();
    r = $sqrt();
    ig = $sscanf();
    b = $stable();
    b = $stable_gclk(b);
    b = $steady_gclk(b);
    i = $stime();
    r = $tan();
    r = $tanh();
    ig = $test$plusargs();
    t = $time();
    s = $typename(i);
    ig = $ungetc();
    ig = $unpacked_dimensions();
    iu = $urandom();
    iu = $urandom_range();
    ig = $value$plusargs();
  end
endmodule

// ---------------------------------------------------------------------------
// KNOWN GAP, kept in its own module on purpose.
//
// $timeunit and $timeprecision (IEEE 1800-2023 Sec 20.4.1) do not parse.
// timeunit and timeprecision are SV keywords (SV3_1aLexer.g4:311,313), so
// "$timeunit" lexes as DOLLAR + TIMEUNIT -- a pair no rule accepts. They need
// combined tokens the way DOLLAR_ROOT and DOLLAR_SETUP have them. Both are
// declared in Builtin::getBuiltInClasses() and would bind once callable.
//
// The syntax errors are EXPECTED and the golden carries them, so the gap is
// visible in CI rather than only in a comment. They live in a separate module
// because the parser's error recovery runs on past the failing statement: in
// the main module above it swallowed the next three calls and cost their
// bindings. Isolated here, the damage cannot reach a working call.
//
// When the lexer gains the tokens, these errors disappear and the golden is
// regenerated.
// ---------------------------------------------------------------------------
module builtin_system_tf_known_gap;
  integer ig;
  initial begin
    ig = $timeprecision();
    ig = $timeunit();
  end
endmodule
