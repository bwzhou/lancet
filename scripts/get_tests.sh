#!/bin/bash
DIR=$1
for i in $(ls -tv $DIR/*ktest)
do
  name=${i%.ktest}
  if ls $name.looptotal* &> /dev/null
  then
    scale=$(ls $name.looptotal*|awk -F. '{print $NF}')
    echo -n $(basename $name)' '
    echo -n $scale' '
    cat $name.feature
  fi
done|awk -v ncol=$2 '{
if (ncol==0) {
  # output: TestName Scale NumOfConstants
  print $1,$2,NF-2
} else if (ncol==NF-2) {
  # output: Scale [Constant]+
  for (i=2;i<NF;i++) {
    printf "%s,",$i
  }
  print $NF
}
}'
