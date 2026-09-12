#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

// sim6502 doesn't support it???
#include <dirent.h>

int main(void) {
    DIR *dir;
    struct dirent *entry;

    // Open the current directory (".")
    dir = opendir(".");
    if (dir == NULL) {
        perror("Error opening directory");
        return EXIT_FAILURE;
    }

    // Read and print each entry until the end of the directory
    while ((entry = readdir(dir)) != NULL) {
        printf("%s\n", entry->d_name);
    }

    // Close the directory stream
    closedir(dir);
    return EXIT_SUCCESS;
}
