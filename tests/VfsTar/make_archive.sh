#!/usr/bin/env bash
# Generates design.tar - the input for the VfsTar test - entirely from this
# script. The SystemVerilog sources are embedded below (heredocs); they are
# staged into a temporary directory, packed into an ustar archive, and the temp
# files are then removed. Only this script and design.tar are checked in.
#
# Archive member names (top.sv at the root, rtl/leaf.sv in a subdir) and the
# ustar format match what TarFileStore's reader expects (name <= 100 chars,
# typeflag '0'/'5', no PAX records). --mtime fixes the timestamp so the bytes
# are deterministic and don't churn git on regeneration.
set -euo pipefail

outdir="$(cd "$(dirname "$0")" && pwd)"
staging="$(mktemp -d)"
trap 'rm -rf "$staging"' EXIT   # always clean up the temp files

mkdir -p "$staging/rtl"

# --- top.sv -----------------------------------------------------------------
cat > "$staging/top.sv" <<'EOF'
// Top module for the VFS Tar integration test. Compiled from inside a .tar
// archive mounted via the `-tar` CLI option (TarFileStore backend) rather than
// from the local filesystem (LocalStore).
module top;
  logic x, y;
  leaf u_leaf(.a(x), .b(y));
endmodule
EOF

# --- rtl/leaf.sv ------------------------------------------------------------
cat > "$staging/rtl/leaf.sv" <<'EOF'
// Leaf module, served from a subdirectory entry (rtl/leaf.sv) inside the tar
// archive to exercise TarFileStore's nested-path handling.
module leaf(input logic a, output logic b);
  assign b = ~a;
endmodule
EOF

# --- echo the staged contents ----------------------------------------------
for f in top.sv rtl/leaf.sv; do
  echo "===== $f ====="
  cat "$staging/$f"
  echo
done

# --- pack into design.tar (member names relative to the staging dir) --------
tar -C "$staging" --format=ustar --mtime='2026-01-01 00:00:00' \
    -cf "$outdir/design.tar" top.sv rtl/leaf.sv

echo "Wrote $outdir/design.tar:"
tar -tvf "$outdir/design.tar"
