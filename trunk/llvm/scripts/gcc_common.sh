#!/bin/bash
#
# Copyright 2010 Google Inc. All Rights Reserved.
# Author: glider@google.com (Alexander Potapenko)

ALL_ARGS=
ARGS=
LD_MODE=
OX=-O0
OPT_OX=
DEBUG=
OPT_PASSES=-adce
OPT_PASSES="-reg2mem -mem2reg -adce"
OPT_PASSES=-verify
OPT_PASSES=

until [ -z "$1" ]
do
  ALL_ARGS="$ALL_ARGS $1"
  if [ `expr match "$1" ".*\.cc"` -gt 0 ]
  then
    SRC="$1"
  elif [ `expr match "$1" ".*\.c"` -gt 0 ]
  then
    SRC="$1"
  elif [ `expr match "$1" "-o"` -gt 0 ]
  then
    if [ "$1" == "-o" ]
    then
      shift
      ALL_ARGS="$ALL_ARGS $1"
      SRC_OBJ="$1"
    else
      SRC_OBJ=${1:2}
    fi
  elif [ `expr match "$1" "-m64"` -gt 0 ]
  then
    PLATFORM="x86-64"
    ARGS="$ARGS $1"
  elif [ `expr match "$1" "-m32"` -gt 0 ]
  then
    PLATFORM="x86"
    ARGS="$ARGS $1"
  elif [ `expr match "$1" ".*\.[ao]\$"` -gt 0 ]
  then
    LD_MODE=1
  elif [ `expr match "$1" "-O0"` -gt 0 ]
  then
    OX=$1
    OPT_OX=
  elif [ `expr match "$1" "-O"` -gt 0 ]
  then
    OX=$1
    OPT_OX=$1
  elif [ `expr match "$1" "-g"` -gt 0 ]
  then
    DEBUG=-g
  elif [ `expr match "$1" "-c\|-std=\|-Werror"` -gt 0 ]
  then
    # pass
    echo "Dropped arg: $1"
  else
    ARGS="$ARGS $1"
  fi
  shift
done

if [ "$LD_MODE" == "1" ]
then
  $SCRIPT_ROOT/ld.sh $ALL_ARGS
  exit
fi

DEBUG=
#SRC=$1
#echo $ARGS
FNAME=`echo $SRC | sed 's/\.[^.]*$//'`
SRC_BIT="$FNAME.ll"
SRC_TMP="$FNAME-tmp.ll"
SRC_INSTR="$FNAME-instr.ll"
SRC_ASM="$FNAME.S"
if [ -z $SRC_OBJ ]
then
  SRC_OBJ=`basename $FNAME.o`
fi
SRC_EXE="$FNAME"
SRC_DBG="$SRC_OBJ.dbg"

INST_MODE=-offline
INST_MODE=-online

LOG=instrumentation.log

set_platform_dependent_vars

# Translate C code to LLVM bitcode.
$COMPILER -emit-llvm $MARCH $SRC $OX $DEBUG -S $DA_FLAGS $ARGS -o "$SRC_BIT" || exit 1
# Instrument the bitcode.
$OPT $OPT_PASSES  "$SRC_BIT" -S  > "$SRC_TMP" 2>$LOG || exit 1
$OPT -load "$PASS_SO" $INST_MODE -arch=$XARCH "$SRC_TMP" -S  > "$SRC_INSTR" 2>$LOG || exit 1


# Obsolete. TODO(glider): remove.
#cat $LOG | grep "^->" | sed "s/^->//" > "$SRC_DBG"
#cat $LOG | grep -v "^->"

# Translate LLVM bitcode to native assembly code.
$LLC -march=$XARCH $OX $SRC_INSTR  -o $SRC_ASM || exit 1
# Compile the object file.
$COMPILER $MARCH -c $SRC_ASM $OX $DEBUG -o $SRC_OBJ || exit 1

