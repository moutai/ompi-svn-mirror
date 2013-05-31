#!/bin/sh 

module load cisco/autotools/ac269-am1131-lt242

set -x

mtt=/cm/shared/apps/mtt/client/mtt
scratch=$HOME/scratches/`date '+%Y-%m-%d'`-$$
ini=/home/feni/ompi-tests/cisco/mtt/savbu-usnic/cisco-ompi-v1.6-usnic-qa.ini

mtt=//home/jsquyres/svn/mtt/client/mtt
scratch=$HOME/mtt-scratches/`date '+%Y-%m-%d'`-$$
ini=/home/jsquyres/svn/ompi-tests/cisco/mtt/savbu-usnic/cisco-ompi-v1.6-usnic-qa.ini

rm -rf $scratch
mkdir -p $scratch

doit() {
    echo "=== Running command: $*"
    $*
    if test "$?" != "0"; then
        echo "=== Command failed: $*"
        exit 1
    fi
}

mtt_base="$mtt --scratch $scratch --file $ini --verbose"
doit $mtt_base --mpi-get
doit $mtt_base --mpi-install
doit $mtt_base --test-get --section ibm
doit $mtt_base --test-build --section ibm
doit $mtt_base --test-run --section ibm
