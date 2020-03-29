#!/usr/bin/env bash

# utility function(s) like ProgressBar
source scripts/util.sh

ECHO_MOD=kecho.ko

if [ "$EUID" -eq 0 ]
  then echo "Don't run this script as root"
  exit
fi

# load kecho
echo "Preparing..."
sudo rmmod -f kecho 2>/dev/null
sleep 1
sudo insmod $ECHO_MOD || exit 1
sleep 1

END=1000
# Quoted from film "I Am David" (2003)
MSG="You could not bribe honest people, but bad people would accept bribery."
 echo "Send message via telnet"
for i in $(seq 1 $END);
do
    echo "$MSG" | telnet 127.0.0.1 12345 2>/dev/null >/dev/null
    ProgressBar $i $END
done

# epilogue
sudo rmmod kecho
printf "\nComplete\n"
