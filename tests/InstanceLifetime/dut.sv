// IEEE 1800-2023 Sec 37.10 vpiAutomatic on an instance: the lifetime given in the
// interface/program header (Sec 25.3, 24.3, 6.21), static when omitted.

interface automatic ifc_auto(); endinterface
interface static ifc_static(); endinterface
interface ifc_default(); endinterface

program automatic prg_auto(); endprogram
program static prg_static(); endprogram
program prg_default(); endprogram
