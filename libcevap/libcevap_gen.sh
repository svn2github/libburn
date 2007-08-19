#!/bin/sh

test_dir=.
model_file=../doc/libdax_model.txt
xtr_dir=.
cgen_dir=.
# cgen_dir=~/projekte/cdrskin_dir/libburn-develop/libcevap

cd "$test_dir" || exit 1
test -e smem.h || exit 1

cat "$model_file" | \
  "$xtr_dir"/extract_cgen_input.sh | \
  "$cgen_dir"/cgen -smem_local -ansi -global_include cevap_global.h \
                   -overwrite "$@"

