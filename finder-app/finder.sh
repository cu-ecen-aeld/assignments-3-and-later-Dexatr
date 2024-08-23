#!/bin/sh

# Check if the correct number of arguments is provided
if [ $# -lt 2 ]; then
    echo "Error: Missing arguments. Usage: $0 <directory_path> <search_string>"
    exit 1
fi

filesdir=$1
searchstr=$2

# Check if the provided path is a directory
if [ ! -d "$filesdir" ]; then
    echo "Error: $filesdir is not a directory"
    exit 1
fi

# Count the number of files in the directory
num_files=$(find "$filesdir" -type f | wc -l)

# Count the number of matching lines
num_matches=$(grep -r "$searchstr" "$filesdir" 2>/dev/null | wc -l)

# Display the results
echo "The number of files are $num_files and the number of matching lines are $num_matches"
