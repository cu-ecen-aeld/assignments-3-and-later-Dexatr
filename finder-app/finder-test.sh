#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Siddhant Jajoo

set -e
set -u

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data

# Ensure the conf directory and its files exist
if [ ! -f /etc/finder-app/conf/username.txt ] || [ ! -f /etc/finder-app/conf/assignment.txt ]; then
    echo "Missing /etc/finder-app/conf/username.txt or /etc/finder-app/conf/assignment.txt"
    exit 1
fi

username=$(cat /etc/finder-app/conf/username.txt)

if [ $# -lt 3 ]
then
    echo "Using default value ${WRITESTR} for string to write"
    if [ $# -lt 1 ]
    then
        echo "Using default value ${NUMFILES} for number of files to write"
    else
        NUMFILES=$1
    fi    
else
    NUMFILES=$1
    WRITESTR=$2
    WRITEDIR=/tmp/aeld-data/$3
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

echo "Writing ${NUMFILES} files containing string ${WRITESTR} to ${WRITEDIR}"

rm -rf "${WRITEDIR}"

# Check the assignment type
assignment=$(cat /etc/finder-app/conf/assignment.txt)

if [ $assignment != 'assignment1' ]
then
    mkdir -p "$WRITEDIR"

    if [ -d "$WRITEDIR" ]
    then
        echo "$WRITEDIR created"
    else
        exit 1
    fi
fi

# Clean and compile the writer application if necessary
# make clean
# make

for i in $(seq 1 $NUMFILES)
do
    /usr/bin/writer "$WRITEDIR/${username}$i.txt" "$WRITESTR"
done

OUTPUTSTRING=$(/usr/bin/finder.sh "$WRITEDIR" "$WRITESTR")

# Write output to /tmp/assignment4-result.txt
echo "${OUTPUTSTRING}" > /tmp/assignment4-result.txt

# Remove temporary directories
rm -rf /tmp/aeld-data

set +e
echo ${OUTPUTSTRING} | grep "${MATCHSTR}"
if [ $? -eq 0 ]; then
    echo "success"
    exit 0
else
    echo "failed: expected ${MATCHSTR} in ${OUTPUTSTRING} but instead found"
    exit 1
fi
