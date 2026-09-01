/*
:name: chapter6_error_rules_inv3
:description: IEEE 1800-2023 6.18 -- hierarchical reference to a type identifier
:tags: 6.18
:should_fail_because: type identifiers cannot be named through a hierarchical (instance) prefix
*/

// catalog row 18 | 6.18 | COMP
// Hierarchical references to type identifiers shall not be allowed
// (interface-port-based typedefs are not hierarchical references and are
// permitted).
//
// Kept in its own fixture: HLC's grammar does not accept an instance-prefixed
// name ("u.data_t") in a data_type position at all -- the IEEE 1800-2023
// Annex A.2.2.1 data_type grammar only allows a ps_type_identifier
// ([package_scope] type_identifier), not an arbitrary hierarchical prefix, so
// this is a PARSE-level rejection rather than a semantic one. See Row18.
module r18_sub; typedef int r18_data_t; endmodule
module r18_top;
  r18_sub u();
  u.r18_data_t v; // illegal - hierarchical reference to a type identifier
endmodule
