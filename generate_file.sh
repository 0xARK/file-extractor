#!/bin/bash
for i in {1..20}
do
  echo "creating file $i"
  echo "test_test_test" > "./monitored-folder/$i.txt"
  sleep 0.1
done

sleep 1
rm ./monitored-folder/*.txt