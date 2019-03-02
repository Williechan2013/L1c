#!/bin/bash

source "${srcdir}/test_runner_utils.sh"

cur_dir=$(pwd)

function cleanup(){
    cd $cur_dir
}
trap cleanup EXIT

host_os=$(get_os)

if test $host_os = windows || test x$WITH_MEX = xno;
then
    exit 77
fi


if test -z $verbose; then
    verbose=0
fi

LIB_DIR="${ABS_TOP_BUILDDIR}/interfaces"
TEST_SRCDIR="${ABS_TOP_SRCDIR}/test"
ml_script_dir="${TEST_SRCDIR}"
data_path="${ABS_TOP_SRCDIR}/test/test_data/example_img_data.json"

# Check the matlab dlopen interface
cmd=$(ml_cmd "${LIB_DIR}" "${ml_script_dir}" "test_l1qc_dct_dlopen" \
             "'${data_path}'" "${verbose}")

matlab -nojvm -r "${cmd}"

exit $?
