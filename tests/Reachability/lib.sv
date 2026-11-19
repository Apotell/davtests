// A library: packages and classes, no module or program. It has no top-level module and needs
// none -- nothing is reported for it.

package lib_pkg;
  class LibItem;
    int value;
  endclass

  function automatic int lib_fn();
    return 0;
  endfunction
endpackage

class LibStandalone;
endclass
