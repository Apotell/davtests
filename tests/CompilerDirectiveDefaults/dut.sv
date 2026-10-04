// Compiler directives that set a default on the design elements that follow them,
// and `resetall putting every one of them back to its default.
//   IEEE 1800-2023 Sec 22.3  `resetall
//   IEEE 1800-2023 Sec 22.8  `default_nettype
//   IEEE 1800-2023 Sec 22.9  `unconnected_drive / `nounconnected_drive
//   IEEE 1800-2023 Sec 22.10 `celldefine / `endcelldefine
//   IEEE 1800-2023 Sec 22.14 `begin_keywords / `end_keywords
//   IEEE 1800-2023 Annex E   `default_decay_time, `default_trireg_strength, `delay_mode_*

module plain(); endmodule

`celldefine
`unconnected_drive pull1
`delay_mode_zero
`default_nettype none
module cell_a(input a); endmodule
interface ifc(); endinterface
program prg(); endprogram

`endcelldefine
`unconnected_drive pull0
`delay_mode_path
module b(); endmodule

`resetall
module after_reset(); endmodule

`celldefine
`unconnected_drive pull1
`nounconnected_drive
`delay_mode_distributed
`delay_mode_unit
`default_trireg_strength 30
`accelerate
`noaccelerate
`autoexpand_vectornets
`expand_vectornets
`noexpand_vectornets
`remove_gatename
`noremove_gatenames
`remove_netname
`noremove_netnames
`suppress_faults
`nosuppress_faults
`disable_portfaults
`enable_portfaults
`signed
`unsigned
`default_decay_time 100
`protect
`endprotect
`uselib
`begin_keywords "1800-2017"
module c(); endmodule
`end_keywords
