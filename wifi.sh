#!/bin/sh

wifistats() {
  /System/Library/PrivateFrameworks/Apple80211.framework/Versions/Current/Resources/airport -I |\
    awk 'BEGIN {top = ""; row = ""} {top = top $1 "\t"; row = row $2 "\t"} END { print top "\n" row}' |\
    column -t;
}
case $# in
  0)
    ;;
  1)
    re='^[0-9]+$'
    if ! [[ $1 =~ $re ]] ; then
       echo "error: wait time not a number" >&2; exit 1
    fi
    wait_time=$1
    ;;
  *)
    exit 1
    ;;
esac

if [[ -n $wait_time ]]; then
  while true; do wifistats; sleep $wait_time; done
else
  wifistats
fi
