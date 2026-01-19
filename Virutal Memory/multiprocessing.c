/** @file multiprocessing.c
 * @brief Basic Multiprocessing Implementation
 * 
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * @author Nagata Parama Aptana
 * @author Louis Ryan Tan
 */

#include <assert.h>
#include <string.h>

#include "multiprocessing.h"
#include "malloc369.h"
#include "pagetable.h"
#include "sim.h"
#include "types.h"

#define DEFAULT_MAX_NR_TASKS 128

struct mm_s {
	asid_t asid;
	struct pagetable * pgtable;
};

i32 max_nr_tasks;

struct task_s * tasks;
struct task_s * curtask;

i32 get_max_nr_tasks()
{
	return max_nr_tasks;
}

struct task_s * get_task_by_id(u32 id)
{
	return &tasks[id];
}

struct task_s * current_task()
{
	return curtask;
}

int current_task_id()
{
	assert (curtask != NULL);
	return (int) (curtask - tasks);
}

mm_t *create_mm(asid_t asid, struct pagetable *pt)
{
	mm_t *res = malloc369(sizeof(mm_t));
	assert(res != NULL);
	*res = (mm_t) { .asid = asid, .pgtable = pt };
	return res;
}

void free_mm(struct mm_s * mm)
{
	free_pagetable(mm->pgtable);
	free369(mm);
}

void init_multiprocessing(struct mp_config *cfg)
{
	max_nr_tasks = cfg->max_nr_tasks > 0
		? cfg->max_nr_tasks
		: DEFAULT_MAX_NR_TASKS;
	tasks = malloc369(max_nr_tasks * sizeof(*tasks));
	assert(tasks != NULL);
	memset(tasks, 0, max_nr_tasks * sizeof(*tasks));
};

void free_multiprocessing()
{
	// tasks have to empty at the end
	free369(tasks);
}

i64 task_switch(struct task_s * newtask)
{
	assert(newtask->mm != NULL);

	curtask = newtask;
	return 0;
}

asid_t get_asid(struct mm_s * mm)
{
	return mm->asid;
}

struct pagetable * get_pagetable(struct mm_s * mm)
{
	return mm->pgtable;
}

struct task_s * create_task(int pid)
{
	struct task_s *tsk = &tasks[pid];
	assert(tsk->mm == NULL);
	tsk->mm = create_mm(pid, create_pagetable());
	return tsk;
}

void free_task(struct task_s * tsk)
{
	free_mm(tsk->mm);
	tsk->mm = NULL;
}

i64 fork369(int parent_id, int child_id)
{
	tasks[child_id].mm = create_mm(
		child_id,
		duplicate_pagetable(tasks[parent_id].mm->pgtable, parent_id)
	);
	return 0;
}

