#include <fstream>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

// Copy a string and explicitly add '\0' at the end.
#define STRING_COPY_WITH_NULL(dest, source) { \
	strncpy(dest, source, strlen(source));    \
    dest[strlen(source)] = '\0';              \
}

extern "C" char *bistream_file_name_with_absolute_path() {
    // Get the name of the bitstream file.
    char *bitstream;
    char *env = getenv("BITSTREAM");
    if (env == NULL) {
        // By default, the bitstream is $HOME/tmp/a.aocx
        char *home = getenv("HOME");
        bitstream = (char *)malloc(strlen(home) + 12);
        STRING_COPY_WITH_NULL(bitstream, home);
        strncat(bitstream, "/tmp/a.aocx", 11);
    } else {
        bitstream = (char *)malloc(strlen(env) + 1);
        STRING_COPY_WITH_NULL(bitstream, env);

        // Check that the name ends with .aocx
        if (strlen(bitstream) <= 5) {
            printf("Error! Invalid bitstream name: '%s'\n", bitstream);
            free(bitstream);
            return NULL;
        }
        char suffix[] = ".aocx";
        for (int i = 0; i < 5; i++) {
            if (bitstream[strlen(bitstream) - 5 + i] != suffix[i]) {
                printf("Error! Invalid bitstream name: '%s'. Expected to end with '.aocx'\n", bitstream);
                free(bitstream);
                return NULL;
            }
        }
    }

    // Get the file name with the absolute path
    char *full_name = realpath(bitstream, NULL);
    // We cannot free the space returned by realpath(). Copy to our user space that caller can free
    char *full_name1 = (char *)malloc(strlen(full_name) + 1);
    STRING_COPY_WITH_NULL(full_name1, full_name);
    free(bitstream);
    return full_name1;
}

extern "C" char *bitstream_directory() {
    char *bitstream = bistream_file_name_with_absolute_path();
    char *parent_dir = dirname(bitstream);
    char *dir = (char *)malloc(strlen(parent_dir) + 1);
    STRING_COPY_WITH_NULL(dir, parent_dir);
    free(bitstream);
    return dir;
}

extern "C" char *quartus_output_directory() {
    char *bitstream = bistream_file_name_with_absolute_path();
    char *dir = (char *)malloc(strlen(bitstream) - 5 + 1);
    strncpy(dir, bitstream, strlen(bitstream) - 5);
    dir[strlen(bitstream) - 5] = '\0';
    free(bitstream);
    return dir;
}

extern "C" char *concat_directory_and_file(const char *dir, const char *file) {
    if (!(dir && strlen(dir) >= 1)) {
        printf("Error! Invalid directory\n");
        return NULL;
    }
    if (!(file && strlen(file) >= 1)) {
        printf("Error! Invalid file name\n");
        return NULL;
    }
    char *name = (char *)malloc(strlen(dir) + strlen(file) + 2);
    STRING_COPY_WITH_NULL(name, dir);
    strncat(name, "/", 1);
    strncat(name, file, strlen(file));
    return name;
}
