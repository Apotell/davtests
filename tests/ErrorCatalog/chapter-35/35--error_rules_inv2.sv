/*
:name: chapter35_error_rules_inv2
:description: IEEE 1800-2023 35.4 -- an escaped-identifier DPI linkage name that is not a valid C identifier
:tags: 35.4
:should_fail_because: the linkage name's stripped form still contains characters illegal in a C identifier
*/

// catalog row 1108 | 35.4 | COMP
// A global/linkage name shall follow C naming conventions: it shall start
// with a letter or underscore followed by alphanumeric characters or
// underscores. If given as an escaped identifier, the leading backslash and
// trailing whitespace are stripped and the result shall still comply with C
// identifier rules.
//
// Kept alone in its own fixture: HLC's grammar does not accept an escaped
// identifier as the c_identifier of an "= function" linkage-name form, so
// the whole import declaration desynchronizes the parser ("mismatched input
// 'function'" then "mismatched input 'endmodule'"), leaving the enclosing
// module's own AST malformed. Nothing may be appended below.

module r1108_m;
  import "DPI-C" \init[1] = function void r1108_f(); // ERROR: linkage name 'init[1]' is not a valid C identifier
endmodule
