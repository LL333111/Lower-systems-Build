/** @file convert.c
 * @brief Converts .mref Files to A More Compact Binary Format
 * 
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * @author Louis Ryan Tan
 *
 */

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdnoreturn.h>

#include "parse_trace.h" // struct trace_line

void noreturn help_usage(char **argv)
{
	fprintf(stdout,
		"Converts a multiprocess trace file into a more compact binary "
		"format.\n"
		"REQUIRES the input trace to be CORRECT.\n"
	);
	fprintf(stdout, "usage: %s [-i tracein] [-o traceout]\n", argv[0]);
	exit(EXIT_FAILURE);
}

int main(int argc, char ** argv)
{
	int opt;
	char * inpath = NULL;
	char * outpath = NULL;
	while ((opt = getopt(argc, argv, "h:i:o:")) != -1) {
		switch (opt) {
			case 'i':
			inpath = optarg;
			break;
			case 'o':
			outpath = optarg;
			break;
			case 'h':
			default:
			help_usage(argv);
		}
	}

	if (inpath == NULL || outpath == NULL) {
		help_usage(argv);
	}
	
	FILE * fin = fopen(inpath, "r");
	FILE * fout = fopen(outpath, "w");

	char line[32];
	while (fgets(line, sizeof(line), fin)) {
		struct trace_line tl;
		if (sscanf(line, "%u %c %zx %hhu",
			 &tl.vpid, &tl.reftype, &tl.vaddr, &tl.value) != 4) {
			fprintf(stderr, "invalid line: %s\n", line);
		}
		fwrite(&tl, sizeof(tl), 1, fout);
	}
	fclose(fin);
	fclose(fout);
	return 0;
}
