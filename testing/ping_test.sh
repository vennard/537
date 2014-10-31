#!/bin/bash

# Ping test to check availability of all 7 nodes
# To be run on client (Node 8)

ip=("10.10.6.1", "10.10.5.1", "10.10.4.1", "10.10.3.1", "10.10.2.1", "10.10.1.1", "10.10.0.1")
up=0
ind=0
errors=""
haserror=0

# check nodes layer by layer
while [ $up -eq 0 ]; do
  ping -c 1 ${ip[$ind]}  > /dev/null 2>&1
  if [ $? -ne 0 ]; then
	 errors="$errors ${ip[$ind]}"
	 haserror=1
  fi
  if [ $ind -eq 6 ]; then
	 up=1
	 if [ $haserror -eq 0 ]; then
		echo all ip addresses available. Success!
	 else
	 	echo The following ip addresses are unreachable:
	 	echo $errors
	 	echo
  	 fi
  fi
  let ind++
done

