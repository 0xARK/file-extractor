#!/bin/bash
for i in {1..100}
do
  echo "creating file $i"
  echo "uwu_uwu_uwu" > "./monitored-folder-2/$i"
  sleep 0.5
done

rm ./monitored-folder-2/*