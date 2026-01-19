#include <stdio.h>
#include <stdlib.h>

#include "marker.h"

/* This is set up to showcase scan-resistant 2Q algorithm with memory size of
 * five page frames, 2Q's threshold set at 10% of pages (==0, so only 1 page 
 * can go into A1 list).
 */

#define PAGE_SIZE    4096
#define REUSE_FREQ     50 /* Access all reused pages after this many scanned pages */
#define NREUSE_PAGES   40 /* Number of reused pages, should fit in Am for 2Q. */
#define NSCAN_PAGES 10000 /* Number of scanned pages. Must be multiple of REUSE_FREQ. */

int main()
{
	int ok;
	unsigned char *scan_array;
	unsigned char *reuse_array;
	
	ok = posix_memalign((void *)&scan_array, PAGE_SIZE, NSCAN_PAGES * PAGE_SIZE);
	if (ok != 0) {
		perror("scan_array posix_memalign failed");
		exit(1);
	}
	ok = posix_memalign((void *)&reuse_array, PAGE_SIZE, NREUSE_PAGES * PAGE_SIZE);
	if (ok != 0) {
		perror("reuse_array posix_memalign failed");
		exit(1);
	}

	marker_start("runs/reuse_scan/marker");
	register int i,j,k;
	//register int l;
	// Access reuse_array before starting scan
	for (k = 0; k < NREUSE_PAGES; k++) {
		reuse_array[k*PAGE_SIZE] = k;
	}

	// Scan large array (each page used only once)
	// Every REUSE_FREQ pages of large array, access reuse array

	for (i = 0; i < NSCAN_PAGES/REUSE_FREQ; i++) {
		for (k = 0; k < NREUSE_PAGES; k++) {
			reuse_array[k*PAGE_SIZE] = reuse_array[k*PAGE_SIZE]+i;
		}

		for (j = 0; j < REUSE_FREQ; j++) {			
			scan_array[(i*REUSE_FREQ+j)*PAGE_SIZE] = i*REUSE_FREQ + j;
		}
	}
	
	marker_end();

	// Use random entry in each array so compiler doesn't
	// optimize everything away. 
	int index = (random() % NSCAN_PAGES) * PAGE_SIZE;
	printf("random entry in scan array is %u\n", scan_array[index]);
	index = (random() % NREUSE_PAGES) * PAGE_SIZE;
	printf("random entry in reuse array is %u\n", reuse_array[index]);

	return 0;
}
