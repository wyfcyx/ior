#!/bin/bash

# Test script for basic IOR functionality testing various patterns
# It is kept as simple as possible and outputs the parameters used such that any test can be rerun easily.

# You can override the defaults by setting the variables before invoking the script, or simply set them here...
# Example: export IOR_EXTRA="-v -v -v"

ROOT="$(dirname ${BASH_SOURCE[0]})"
TYPE="basic"

source $ROOT/test-lib.sh

MDTEST 1 -n 5000 -i 8 -V 0
MDTEST 2 -n 5000 -i 8 -V 0
MDTEST 4 -n 5000 -i 8 -V 0
MDTEST 8 -n 5000 -i 8 -V 0
MDTEST 12 -n 5000 -i 8 -V 0
#MDTEST 2 -a MDMS -n 5000 -i 3
#MDTEST 4 -a MDMS -n 5000 -i 3

END
