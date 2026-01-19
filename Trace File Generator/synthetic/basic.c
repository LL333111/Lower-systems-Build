/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Author: Louis Ryan Tan
 */

#include <stdio.h>

int main()
{
    FILE * f = fopen("basic.mref", "w");
    if (f == NULL) {
        fprintf(stderr, "Failed to open basic.mref\n");
        return 1;
    }

    // start 2 processes
    fprintf(f, "0 B 0 0\n");
    fprintf(f, "1 B 0 0\n");

    // fork process 0
    fprintf(f, "0 F 2 0\n");

    // do some memory read and writes
    fprintf(f, "0 M 0x10 71\n");
    fprintf(f, "0 S 0x11 76\n");
    fprintf(f, "0 M 0x12 72\n");
    fprintf(f, "1 S 0x13 70\n");

    fprintf(f, "1 S 0x12 58\n");
    fprintf(f, "1 S 0x12 41\n");

    fprintf(f, "0 S 0x10 15\n");
    fprintf(f, "0 M 0x11 0\n");
    fprintf(f, "0 S 0x12 0\n");
    fprintf(f, "0 M 0x13 13\n");

    // fork process 0
    fprintf(f, "0 F 2 0\n");

    // expect the previously loaded data is still there
    fprintf(f, "0 L 0x10 15\n");
    fprintf(f, "0 L 0x11 0\n");
    fprintf(f, "0 L 0x12 0\n");
    fprintf(f, "0 L 0x13 13\n");
    fprintf(f, "1 L 0x12 41\n");

    // expect the previously loaded data is copied in child
    fprintf(f, "2 L 0x10 15\n");
    fprintf(f, "2 L 0x11 0\n");
    fprintf(f, "2 L 0x12 0\n");
    fprintf(f, "2 L 0x13 13\n");

    // child writes some values
    fprintf(f, "2 S 0x10 65\n");
    fprintf(f, "2 M 0x11 51\n");
    fprintf(f, "2 S 0x12 101\n");
    fprintf(f, "2 S 0xcoffee 122\n");

    // child verifies the written values
    fprintf(f, "2 L 0x10 65\n");
    fprintf(f, "2 L 0x11 51\n");
    fprintf(f, "2 L 0x12 101\n");
    fprintf(f, "2 L 0x13 13\n"); // shouldn't change
    fprintf(f, "2 L 0xcoffee 122\n");

    // end all processes
    fprintf(f, "0 E 0 0\n");
    fprintf(f, "1 E 0 0\n");
    fprintf(f, "2 E 0 0\n");

    fclose(f);

    return 0;
}
