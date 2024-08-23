#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        syslog(LOG_ERR, "Usage: %s <file_path> <string>", argv[0]);
        return 1;
    }

    const char *file_path = argv[1];
    const char *string = argv[2];

    FILE *file = fopen(file_path, "w");
    if (!file) {
        syslog(LOG_ERR, "Could not open file: %s", file_path);
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", string, file_path);
    fprintf(file, "%s", string);
    fclose(file);

    return 0;
}
