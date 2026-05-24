import os
import platform

# If you are adding a new entry, please include a short comment
# explaining why the specific test is blacklisted.

_unix_black_list = set([name.lower() for name in [
  'ariane/Ariane',                                    # Uses shell script with make command
  'black-parrot/bp_top/syn/BlackParrot',
  'black-parrot/bp_top/syn/BlackUcode',
  'black-parrot/bp_top/syn/BlackUnicore',
  'Earlgrey_nexysvideo/synth/Earlgrey_nexysvideo',    # ram size in ci machines
  'wav-lpddr-hw/verif/run/Lpddr',
  'Rsd/Processor/Src/Rsd',                            # Out of memory on CI machines
  'SimpleParserTest/SimpleParserTestCache',           # race condition

  # Temporarily disabled while we stabilize new compiler implementation
  'YosysOpenSparc/YosysOpenSparc',
  'LowMemLib/LowMemLib',                              # Need multi-processing which isn't supported on Windows
  'LowMemPkg/LowMemPkg'                               # Need multi-processing which isn't supported on Windows

  # Following tests got some issues with new generator logic. Temporarily disabled!
  'SimpleCmdLineTest/TestBasic',
  'TestFileSplit/TestFileSplit',
  'TestMacros/TestMacros',
  'TestNoHash/TestNoHash',
  'TestSepComp/TestSepComp',
  'TestSepComp/badpath/TestSepCompBadPath',
  'TestSepCompNoHash/TestSepCompNoHash',
]])

_windows_black_list = _unix_black_list.union(set([name.lower() for name in [
  'Earlgrey_Verilator_01_05_21/sim-icarus/Earlgrey_Verilator_01_05_21',   # lowmem is unsupported
  'UnitPython/UnitPython',                                                # Python is unsupported
]]))

_msys2_black_list = _unix_black_list.union(set([name.lower() for name in [
  'Earlgrey_Verilator_01_05_21/sim-icarus/Earlgrey_Verilator_01_05_21', # lowmem is unsupported
]]))


def is_blacklisted(name):
  if platform.system() == 'Windows':
    blacklist = _msys2_black_list if 'MSYSTEM' in os.environ else _windows_black_list
  else:
    blacklist = _unix_black_list
  return name.lower() in blacklist
