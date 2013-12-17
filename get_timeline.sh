#!/bin/bash
DIR=$1
max=0
scale=0
echo $(ls -l $DIR/assembly.ll | awk '{print $8}'),0,0
for i in $(ls -tv $DIR/*ktest)
do
  scale=$(basename $(ls ${i%.ktest}.looptotal*) | awk -F. '{print $4}')
  if [ $max -lt $scale ]
  then
    max=$scale
  fi
  echo $(ls -l $i | awk '{print $8}'),$scale,$max
done
