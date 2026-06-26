# davtests

Test suite for Apotell's Davenche suite, a SystemVerilog front-end compiler based on [Hlc](https://github.com/chipsalliance/Hlc).

## Repository layout

```
hlc/          Test command files (.hlc) and golden reference logs (.log)
              hlc/<group>/<test>/<test>.hlc   — compiler command line
              hlc/<group>/<test>/<test>.log   — expected output (golden)

tests/        SystemVerilog source files referenced by the .hlc files

scripts/      Regression and utility scripts
              regression.py   — run the full test suite
              summarize.py    — aggregate results across shards

UVM/          UVM library copies used by tests that require them
```

Each `.hlc` file contains a command line with a `-wd` option that points back to the corresponding source directory under `tests/`, making individual tests runnable standalone without going through the regression harness.

## Running regression locally

```bash
python3 scripts/regression.py \
  --build-dirpath <path-to-build> \
  --hlc-filepath hlc \
  --reducer-filepath uhdm-reduce \
  --output-dirpath regression \
  --jobs $(nproc)
```

## CI

Regression runs are triggered automatically via `repository_dispatch` from code repos after a successful build. Results are published as workflow artifacts.

## License

Copyright 2024 Apotell. Portions derived from work by Alain Dargelas (2019).
Licensed under the [Apache License, Version 2.0](LICENSE).
