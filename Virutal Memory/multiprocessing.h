/** @file multiprocessing.h
 * @brief Multiprocessing Header File
 * 
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * @author Louis Ryan Tan
 * @author Nagata Parama Aptana
 */

#ifndef __MULTIPROCESSING_H__
#define __MULTIPROCESSING_H__

#include <assert.h>
#include "sim.h"
#include "pagetable.h"

/* memory manager */
typedef struct mm_s mm_t;

/* process abstraction */
struct task_s {
	struct mm_s * mm;
};

/* Multiprocessing startup and breakdown */
void init_multiprocessing(struct mp_config *cfg);
void free_multiprocessing();

/* task utilities */
i32 get_max_nr_tasks();
struct task_s * get_task_by_id(u32 id);
struct task_s * current_task();
i32 current_task_id();
struct task_s * create_task(int pid);
void free_task(struct task_s * tsk);

/* mm utilities */
asid_t get_asid(mm_t * mm);
pagetable_t * get_pagetable(mm_t * mm);

/* fork utilities */
i64 fork369(int parent_id, int child_id);

#endif /* __MULTIPROCESSING_H__ */
