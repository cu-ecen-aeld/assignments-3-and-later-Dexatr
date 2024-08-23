#!/bin/sh

# Check if the correct number of arguments is provided
if [ $# -lt 2 ]; then
    echo "Error: Missing arguments. Usage: $0 <file_path> <string_to_write>"
    exit 1
fi

writefile=$1
writestr=$2

# Create the directory if it does not exist
mkdir -p "$(dirname "$writefile")"

# Write the string to the file (overwriting if it exists)
echo "$writestr" > "$writefile"

# Check if the write operation was successful
if [ $? -ne 0 ]; then
    echo "Error: Could not write to file $writefile"
    exit 1
fi
