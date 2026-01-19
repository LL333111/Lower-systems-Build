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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <threads.h>

#include "khash.h"

// Constants
#define MAX_SIMUL_NPROCS    255
#define MAX_SIMUL_VALUE     255
#define MAX_STRLEN          256
#define PAGE_SHIFT           12

#if !defined(SEED)
#define SEED 369
#endif


#if !defined(NDEBUG)

/** DEBUGBREAK
 * Intended to be used when debugging with a debugger such as GDB.
 * Halts the program.
 */
#if defined(__has_builtin) && __has_builtin(__builtin_trap)
// generates an illegal instruction so the program halts exactly at the point
// where DEBUGBREAK is called. Removes the need to go up stack traces from glibc
// functions when calling abort()
#define DEBUGBREAK() __builtin_trap()
#endif

#if !defined(DEBUGBREAK)
// default to abort if not available
#define DEBUGBREAK() abort()
#endif

#define TG_ASSERT(x) do {                   \
    if (!(x)) {                             \
        fprintf(stderr,                     \
            "%s:%d: Assertion failure %s",  \
            __FILE__, __LINE__, #x);        \
        fflush(stderr);                     \
        DEBUGBREAK();                       \
    }                                       \
} while(0)

#endif

// do nothing on release build
#if !defined(TG_ASSERT)
#define TG_ASSERT(x) (void)(x);
#endif

/* ============== tg_queue_t ============== */
/**
 * A queue to keep track of the ordering of forked child processes by the
 * main process.
 *
 * NOTE: since we push everything once and then pop,
 * the tg_queue doesn't need to support all utilities of a queue.
 * Particularly, we follow a (almost) circular buffer implementation, but we
 * ensure that end < capacity by simply making the queue larger. This is a 
 * necessity either way since everything is pushed in the beginning.
 * This allows us to ignore the case where end wraps around (i.e. begin >= end).
 */
typedef struct tg_queue_s {
    int begin;
    int end;
    int * buffer;
    int capacity;
} tg_queue_t;

#define __TG_QUEUE_INIT_CAP 8

void tg_queue_init(tg_queue_t * q)
{
    q->begin = 0;
    q->end = 0;
    q->capacity = __TG_QUEUE_INIT_CAP;
    q->buffer = malloc(q->capacity * sizeof(*q->buffer));
    TG_ASSERT(q->buffer != NULL && "tg_queue buffer not initialized");
}

void tg_queue_push(tg_queue_t * q, int value)
{
    if (q->end == q->capacity) {
        q->capacity *= 2;
        q->buffer = realloc(q->buffer, q->capacity * sizeof(*q->buffer));
        TG_ASSERT(q->buffer != NULL && "tg_queue buffer not initialized");
    }
    q->buffer[q->end++] = value;
}

int tg_queue_pop(tg_queue_t * q)
{
    TG_ASSERT(q->begin != q->end);
    return q->buffer[q->begin++];
}

void tg_queue_destroy(tg_queue_t * q)
{
    TG_ASSERT(q->buffer != NULL && "tg_queue buffer not initialized");
    free(q->buffer);
}

/* ============== as_map_t ============== */
/**
 * Defines an address space, mapping virtual address to its value. Acts as an
 * 'address space' for a process. Provides the utility of a map data structure.
 *
 * Implementation uses khash's map under the hood.
 *
 * Provided functions:
 * as_map_t * as_map_create();
 * void as_map_destroy(as_map_t * map);
 * as_map_t *as_map_copy(as_map_t *src);
 *
 * int as_map_put(as_map_t * map, uint64_t key, value_t value);
 * int as_map_get(as_map_t * map, uint64_t key, value_t *res);
 * void as_map_del(as_map_t * map, uint64_t key);
 */

typedef uint8_t value_t;
KHASH_MAP_INIT_INT64(as_map, value_t);

typedef khash_t(as_map) as_map_t;

/**
 * @brief
 * Copies `src` and returns the copied as_map_t structure. Returns NULL on failure.
 * @param src The as_map to be copied.
 * @return a copy of src, allocated on the heap, or NULL if allocation fails.
 */
as_map_t *as_map_copy(as_map_t *src)
{
    /**
     * NOTE:
     * Use khash's kmalloc, kfree to be consistent with khash's semantics.
     * khash interacts with memory solely through kmalloc, kfree, etc. so
     * this should work.
    */
    as_map_t * res = kmalloc(sizeof(*res));
    if (res == NULL) {
        goto failure;
    }
    memcpy(res, src, sizeof(*res));

    // length of keys and values
    khint_t len = kh_n_buckets(src);
    // copy keys
    khint32_t * flags_cpy = kmalloc(__ac_fsize(len) * sizeof(khint32_t));
    if (flags_cpy == NULL) {
        goto cleanup_res;
    }
    memcpy(flags_cpy, src->flags, __ac_fsize(len) * sizeof(khint32_t));
    // copy keys
    khint64_t *keys_cpy = kmalloc(len * sizeof(*keys_cpy));
    if (keys_cpy == NULL) {
        goto cleanup_flags;
    }
    memcpy(keys_cpy, src->keys, len * sizeof(*keys_cpy));
    // copy values
    value_t *vals_cpy = kmalloc(len * sizeof(*vals_cpy));
    if (vals_cpy == NULL) {
        goto cleanup_keys;
    }
    memcpy(vals_cpy, src->vals, len * sizeof(*vals_cpy));

    res->flags = flags_cpy;
    res->keys = keys_cpy;
    res->vals = vals_cpy;

    return res;
cleanup_keys:
    kfree(keys_cpy);
cleanup_flags:
    kfree(flags_cpy);
cleanup_res:
    kfree(res);
failure:
    return NULL;
}

/**
 * @brief
 * Inserts the key-value pair (`key`, `value`) into `map` if `key` has been
 * inserted before. Otherwise, modifies the value associated with `key` to
 * `value`.
 * @param map
 * @param key
 * @param value
 * @return -1 on failure, 0 on success
 */
int as_map_put(as_map_t * map, uint64_t key, value_t value)
{
    TG_ASSERT(map != NULL);
    int ret;
    khiter_t k = kh_put(as_map, map, key, &ret);
    if (ret < 0) {
        return -1;
    }
    kh_val(map, k) = value;
    return 0;
}

/**
 * @brief
 * Gets the value associated with `key` and places it into `res`.
 * Returns 0 on success and -1 on failure.
 * @param map
 * @param key
 * @param res
 * @return 0 on success, -1 if key is not found in map.
 * */
int as_map_get(as_map_t * map, uint64_t key, value_t *res)
{
    khiter_t k = kh_get(as_map, map, key);
    if (k == kh_end(map)) {
        return -1;
    }
    if (res != NULL) {
        *res = kh_val(map, k);
    }
    return 0;
}

/**
 * @brief
 * Deletes the mapping associated with `key` in `map`.
 * @param map
 * @param key 
 */
void as_map_del(as_map_t * map, uint64_t key)
{
    int k = kh_get(as_map, map, key);
    if (k == kh_end(map)) {
        return;
    }
    kh_del(as_map, map, k);
}

/**
 * @brief
 * Destroys `map` and frees up the memory used by it.
 * @param map
 */
void as_map_destroy(as_map_t * map)
{
    kh_destroy(as_map, map);
}

/**
 * @brief
 * Creates a map allocated on the heap.
 * @return the pointer to map on success, or NULL on failure.
 */
as_map_t * as_map_create()
{
    return kh_init(as_map);
}

/* ============== Marker and Trace Data Structures ============== */

/**
 * This project uses a variation of the FastSlim Trace Reduction Algorithm to reduce the size of the
 * memory trace.
 *
 * This segment contains the data structure and utility functions used to read files, parse trace
 * lines, and write outputs.
 *
 * NOTE:
 * The strings in this project are mainly used for opening raw reference files (`.log` files).
 * The approach used is to allocate a fixed MAX_STRLEN bytes of memory and placing the string value into it.
 * This is done purely for simplicity.
 * This means that the reference file name cannot exceed MAX_STRLEN.
*/

/**
 * Metadata from the marker file.
 */
struct marker {
    pid_t pid;
    uint64_t start;
    uint64_t end;
    uint64_t fork369_start;
    uint64_t fork369_end;
    uint64_t is_parent;
    uint64_t fork_start;
    uint64_t fork_end;
}; 

/**
 * Configuration for an execution of trace simplification.
 */
struct trace_config {
    struct marker marker;
    const char * indir;
    const char * outdir;
    int simpagesize;
    int fastslim_bufsize;
    bool verbose;
};

typedef struct traceline {
    char reftype;
    uint64_t vaddr;
} traceline_t;

/**
 * Structures used by the FastSlim Algorithm.
 */
typedef struct traceitem {
    bool marked;
    uint32_t timestamp;
    traceline_t traceline;
} traceitem_t;

struct trace_config;

/**
 * the trace_writer struct is meant to hold any data structure required
 * to write into the output file.
*/
struct trace_writer {
    FILE * file;
    as_map_t * map;

    // fastslim specific
    uint32_t timestamp;
    uint32_t buffer_capacity; // capacity before flushing
    uint32_t buffer_size;
    struct {
        uint64_t * keys;
        traceitem_t * values;
    } buffer;
};

static inline
void tw_init(struct trace_writer * tw, FILE * file, as_map_t * as,
             struct trace_config * tc)
{
    tw->file = file;
    tw->map = as;
    tw->timestamp = 0;
    tw->buffer_size = 0;
    tw->buffer_capacity = tc->fastslim_bufsize;
    tw->buffer.keys = malloc(sizeof(*tw->buffer.keys) * tw->buffer_capacity);
    tw->buffer.values = malloc(sizeof(*tw->buffer.values) * tw->buffer_capacity);
}

static inline
void tw_destroy(struct trace_writer * tw)
{
    fclose(tw->file);
    free(tw->buffer.keys);
    free(tw->buffer.values);
}

static inline uint64_t get_vaddr_page(uint64_t vaddr)
{
    return vaddr >> PAGE_SHIFT;
}
static inline uint64_t get_vaddr_offset(uint64_t vaddr)
{
    return vaddr & ((1 << PAGE_SHIFT) - 1);
}

int compare_traceitem(const void *lhs, const void *rhs)
{
    return ((traceitem_t *)lhs)->timestamp >= ((traceitem_t *)rhs)->timestamp;
}

value_t get_simul_value(traceline_t * tl, as_map_t * as);

void flush_writer(struct trace_writer * tw)
{
    TG_ASSERT(tw->buffer.keys != NULL);
    TG_ASSERT(tw->buffer.values != NULL);
    traceitem_t to_write[tw->buffer_capacity];
    int bufsize = 0;
    bufsize = tw->buffer_size;
    memcpy(to_write, tw->buffer.values, bufsize * sizeof(*tw->buffer.values));
    // sort based on timestamp
    qsort(to_write, bufsize, sizeof(*to_write), compare_traceitem);

    for (int i = 0; i < bufsize; ++i) {
        traceline_t *tl = &(to_write[i].traceline);
        int value = get_simul_value(tl, tw->map);
        fprintf(tw->file, "%c %lx %hhu\n", tl->reftype, tl->vaddr, value);
    }
    tw->buffer_size = 0;

}

void write_traceline(struct trace_writer * tw, traceline_t * tl)
{
    if (tl->reftype == 'F') {
        flush_writer(tw);
        fprintf(tw->file, "F %lx 0\n", tl->vaddr);
        return;
    }
    const uint64_t key =
        (get_vaddr_page(tl->vaddr) << PAGE_SHIFT) | (uint64_t)tl->reftype;
    bool found = false;
    for (int i = 0; i < tw->buffer_size; ++i) {
        if (tw->buffer.keys[i] == key) {
            found = true;
            traceitem_t * ti = &tw->buffer.values[i];
            ti->marked = true;
            ti->timestamp = tw->timestamp;
            break;
        }
    }
    TG_ASSERT(tw->buffer_size < tw->buffer_capacity);
    if (!found) {
        tw->buffer.keys[tw->buffer_size] = key; 
        tw->buffer.values[tw->buffer_size] = (traceitem_t) {
            .marked = false,
            .timestamp = tw->timestamp,
            .traceline = *tl,
        };
        tw->buffer_size++;
        if (tw->buffer_size == tw->buffer_capacity) {
            flush_writer(tw);
        }
    }
    tw->timestamp++;
}

int get_traceline(FILE * f, traceline_t *tl)
{
    /**
     * NOTE:
     *  this works on the assumption that valgrind will not generate lines that
     *  are >256 is length.
     */
    char line[256];
    while (true) {
        if(fgets(line, sizeof(line), f) == NULL) {
            return -1;
        }
        if (line[0] != '=') {
            break;
        }
    }
    // get reftype
    // a bit hacky, but it's very efficient
    if (line[0] == 'I') {
        tl->reftype = 'I';
    } else {
        tl->reftype = line[1];
    }

    tl->vaddr = strtoul(line + 3, NULL, 16);
    return 0;
}


KHASH_MAP_INIT_INT(cqueue, tg_queue_t);
khash_t(cqueue) * children;
int parse_markerfile(const char * markerpath, struct marker *out, bool verbose)
{
    TG_ASSERT(children != NULL);
    FILE * f = fopen(markerpath, "r");
    TG_ASSERT(f != NULL);
    int res;
    res = fscanf(f, "%d %lx %lx %lx %lx %lx %lx %lx",
                 &out->pid,
                 &out->start, &out->end,
                 &out->fork369_start, &out->fork369_end,
                 &out->fork_start, &out->fork_end,
                 &out->is_parent
                 );
    if (verbose) {
        printf("start pid: %d\n"
               "Addresses: %lx %lx %lx %lx %lx %lx %lx\n",
            out->pid,
            out->start, out->end,
            out->fork369_start, out->fork369_end,
            out->fork_start, out->fork_end,
            out->is_parent
        );
    }
    if (res != 8) {
        fprintf(stderr, "first line of marker file should have 8 integers\n");
        return -1;
    }
    int par, chld;
    int count = 1;
    while ((res = fscanf(f, "%d=>%d", &par, &chld)) > 0) {
        if (verbose) {
            printf("%d %d\n", par, chld);
        }
        TG_ASSERT(res == 2);
        khiter_t k = kh_get(cqueue, children, par);
        tg_queue_t * q = NULL;
        if (k == kh_end(children)) {
            // not found before, put key and initialize queue
            int res;
            k = kh_put(cqueue, children, par, &res);
            TG_ASSERT(res >= 0);
            q = &kh_val(children, k);
            tg_queue_init(q);
        } else {
            q = &kh_val(children, k);
        }
        tg_queue_push(q, chld);
        count++;
        if (count >= MAX_SIMUL_NPROCS) {
            fprintf(stderr, "Too many processes forked, aborting...");
            TG_ASSERT(false);
        }
    }
    fclose(f);
    return count;
}

int get_next_childpid(int pid)
{
    khiter_t k = kh_get(cqueue, children, pid);
    TG_ASSERT(k != kh_end(children));
    tg_queue_t * q = &kh_val(children, k);
    return tg_queue_pop(q);
}

const char * get_input_filename(struct trace_config * tc, int pid)
{
    char * res = calloc(MAX_STRLEN, sizeof(*res));
    snprintf(res, MAX_STRLEN, "%s/%d.log", tc->indir, pid);
    res[MAX_STRLEN - 1] = '\0';
    return res;
}

const char * get_output_filename(struct trace_config * tc, int pid)
{
    char * res = calloc(MAX_STRLEN, sizeof(*res));
    snprintf(res, MAX_STRLEN, "%s/%d.ref", tc->outdir, pid);
    res[MAX_STRLEN - 1] = '\0';
    return res;
}

struct simplify_trace_params {
    struct trace_config * tc;
    pid_t pid;
    const char * inpath;
    const char * outpath;
    as_map_t *as;
    bool found_start;
};

void simplify_trace(struct trace_config * tc, pid_t pid, const char * inpath,
                    const char * outpath, as_map_t *as, bool found_start)
{
    if (tc->verbose) {
        printf(
            "Simplifying trace with pid: %d\n"
            "- in : %s\n"
            "- out: %s\n",
            pid, inpath, outpath);
    }
    FILE * fin = fopen(inpath, "r");
    FILE * fout = fopen(outpath, "w");
    if (fin == NULL) {
        printf("Cannot open input file %s, aborting simplify_trace on this file.\n", inpath);
        return;
    }
    if (fout == NULL) {
        printf("Cannot open output file %s, aborting simplify_trace on this file.\n", outpath);
        return;
    }
    struct trace_writer tw;
    tw_init(&tw, fout, as, tc);

    traceline_t tl;
    bool ignore_line = false;
    struct marker * m = &tc->marker;
    while (get_traceline(fin, &tl) >= 0) {
        if (tl.vaddr == m->start) {
            found_start = true;
        } else if (tl.vaddr == m->fork369_start) {
            ignore_line = true;
        } else if (tl.vaddr == m->fork_start) {
            ignore_line = false;
        } else if (tl.vaddr == m->fork_end) {
            ignore_line = true;
        } else if (tl.vaddr == m->is_parent) {
            TG_ASSERT(ignore_line);
            int cpid = get_next_childpid(pid);
            const char * chinpath = get_input_filename(tc, cpid);
            const char * choutpath = get_output_filename(tc, cpid);

            traceline_t tmp = {
                .reftype = 'F',
                .vaddr = cpid,
            };
            write_traceline(&tw, &tmp);

	    // Copy as after flushing and writing the F traceline
            as_map_t * chas = as_map_copy(as);
            simplify_trace(
                tc,
                cpid,
                chinpath,
                choutpath,
                chas,
                found_start
            );
        }

        if (found_start && !ignore_line) {
            int offset = get_vaddr_offset(tl.vaddr) % tc->simpagesize;
            tl.vaddr = (get_vaddr_page(tl.vaddr) << PAGE_SHIFT) + offset;
            write_traceline(&tw, &tl);
        }
    }

    flush_writer(&tw);

    as_map_destroy(as);
    tw_destroy(&tw);
    free((void*)inpath);
    free((void*)outpath);
    fclose(fin);
}

void help_usage(int argc, char **argv, FILE * fout)
{
    char * program_name = NULL;
    // apparently there might be a case when argv[0] is NULL
    if (argc == 0) {
        program_name = "[program name]";
    } else {
        program_name = argv[0];
    }
    fprintf(fout,
        "%s "
        "-i [input-dir] "
        "-o [output-dir] "
        "-s [simpagesize] "
        "-b [fastslim-bufsize] "
        "-m [marker-path]"
        "\n",
        program_name
    );
}

/**
 * @brief
 * Get the value associated with the address in the trace line `tl` in `as` based
 * on the reference type.
 * If the instruction is a store/modify or if it's the first load on that address,
 * generate a random number between 0-255, place it into the address space, and
 * return it.
 *
 * @param tl The traceline.
 * @param as The AS map.
 * @return The value associated with `tl` in `as` based on the reference type.
*/
value_t get_simul_value(traceline_t * tl, as_map_t * as)
{
    value_t val;
    int found = as_map_get(as, tl->vaddr, &val);

    if (tl->reftype == 'S' || tl->reftype == 'M')
    {
        val = (value_t)(rand() % 256);
        as_map_put(as, tl->vaddr, val);
        return val;
    }

    if (tl->reftype == 'I' || tl->reftype == 'L')
    {
        if (found == 0)
        {
            return val;
        }
        else
        {
            val = (value_t)(rand() % 256);
            as_map_put(as, tl->vaddr, val);
            return val;
        }
    }
    return 0;
};


int main(int argc, char **argv)
{
    srand(SEED);
    children = kh_init(cqueue);
    TG_ASSERT(children != NULL);

    const int default_simpagesize = 16;
    const int default_fastslim_bufsize = 8;
    struct trace_config tc = {
        .simpagesize = default_simpagesize,
        .fastslim_bufsize = default_fastslim_bufsize,
        .verbose = false,
    };

    int opt;
    char * markerpath = NULL;
    while ((opt = getopt(argc, argv, "hi:o:s:b:m:v")) != -1) {
        switch(opt) {
        case 'h':
            help_usage(argc, argv, stdout);
            exit(0);
        case 'i':
            tc.indir = optarg;
            break;
        case 'o':
            tc.outdir = optarg;
            break;
        case 's':
            tc.simpagesize = strtol(optarg, NULL, 10);
            break;
        case 'b':
            tc.fastslim_bufsize = strtol(optarg, NULL, 10);
            break;
        case 'm':
            markerpath = optarg;
            break;
        case 'v':
            tc.verbose = true;
            break;
        case '?':
        default:
            help_usage(argc, argv, stderr);
            exit(1);
        };
    }
    if (markerpath == NULL) {
        help_usage(argc, argv, stderr);
        exit(1);
    }

    if (tc.verbose) {
        printf(
            "Generating traces using:\n"
            "- Input directory: %s\n"
            "- Marker: %s\n"
            "- Output directory: %s\n"
            "- Simulation Page Size: %d\n"
            "- Fastslim Buffer Size: %d\n",
            tc.indir, markerpath, tc.outdir, tc.simpagesize, tc.fastslim_bufsize 
        );
    }

    int ntraces = parse_markerfile(markerpath, &tc.marker, tc.verbose);
    TG_ASSERT(ntraces > 0);

    as_map_t * as = as_map_create();

    const char * inpath = get_input_filename(&tc, tc.marker.pid);

    char * outpath = calloc(MAX_STRLEN, sizeof(*outpath));
    snprintf(outpath, MAX_STRLEN, "%s/start.ref", tc.outdir);
    outpath[MAX_STRLEN - 1] = '\0';

    simplify_trace(&tc, tc.marker.pid, inpath, outpath, as, false);

    tg_queue_t q;
    kh_foreach_value(children, q, tg_queue_destroy(&q));
    kh_destroy(cqueue, children);
}
