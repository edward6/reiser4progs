/* Copyright (C) 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   gauge.c -- gauges for fsck.reiser4. */

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include "gauge.h"
#include <stdio.h>
#include <unistd.h>

/* Current progs gauge. Used for correct pausing when exception */

void gauge_tree(aal_gauge_t *gauge) {
	repair_progress_tree_t *tree;
	gauge_hint_t *hint;
	aal_list_t *elem;
	time_t t;

	aal_assert("vpf-873", gauge != NULL);
	aal_assert("vpf-874", gauge->data != NULL);

	if (!isatty(2))
		return;

	hint = gauge->data;

	switch (gauge->state) {
	case GAUGE_STARTED:
	case GAUGE_RUNNING:
		current_gauge = gauge;

		time(&t);

		if (gauge->state == GAUGE_STARTED) {
			if (hint->start_time == 0)
				hint->start_time = hint->displayed_time = t;
		} else {
			/* Update the gauge not more ofter then once per second or if 
			 * another percent passed. */

			if ((t - hint->displayed_time) < 1)
				return;	    

			hint->displayed_time = t;
		}

		misc_wipe_line(stderr);
		if (aal_strlen(gauge->name) != 0)
			fprintf(stderr, "%s ", gauge->name);

		fprintf(stderr, "Item (unit) of total items: ");
		for(elem = hint->u.tree_hint.tree; elem; 
		    elem = elem->next) 
		{
			tree = (repair_progress_tree_t *)elem->data;
			fprintf(stderr, "%s%u(%u) of %u", 
				elem == hint->u.tree_hint.tree ? "" : " / ",
				tree->item, tree->unit, tree->i_total);
		}
		break;
	case GAUGE_DONE:
		current_gauge = NULL;
	case GAUGE_PAUSED:
		misc_wipe_line(stderr);
		break;
	}

	fflush(stderr);
}

void gauge_rate(aal_gauge_t *gauge) {
	repair_progress_rate_t *rate;
	gauge_hint_t *hint;    
	time_t t;

	aal_assert("vpf-871", gauge != NULL);
	aal_assert("vpf-872", gauge->data != NULL);

	if (!isatty(2))
		return;

	hint = gauge->data;
	rate = hint->u.rate_hint.rate;

	switch (gauge->state) {
	case GAUGE_STARTED:
	case GAUGE_RUNNING:
		current_gauge = gauge;

		time(&t);

		if (gauge->state == GAUGE_STARTED) {
			if (hint->start_time == 0)
				hint->start_time = hint->displayed_time = t;
		} else {
			/* Update the gauge not more ofter then once per second or if 
			 * another percent passed. */
			if (t - hint->displayed_time < 1)
				return;	    

			hint->u.rate_hint.speed = rate->done / (t - hint->start_time);
			hint->displayed_time = t;
		}

		misc_wipe_line(stderr);
		if (aal_strlen(gauge->name))
			fprintf(stderr, "%s ", gauge->name);

		fprintf(stderr, "%llu of %llu, speed %llu/sec", rate->done, 
			rate->total, hint->u.rate_hint.speed);

		break;
	case GAUGE_DONE:
		current_gauge = NULL;
	case GAUGE_PAUSED:
		misc_wipe_line(stderr);
		break;
	}

	fflush(stderr);
}

static errno_t progress_start(repair_progress_t *progress) {
	aal_gauge_t *gauge;
	gauge_hint_t *hint;

	aal_assert("vpf-876", progress != NULL);

	gauge = progress->data;

	if (progress->data == NULL) {
		if (progress->text)
			fprintf(stderr, "%s\n", progress->text);

		if (!(gauge = aal_gauge_create(progress->type, NULL)))
			return -ENOMEM;

		hint = gauge->data = aal_calloc(sizeof(gauge_hint_t), 0);

		if (!hint) {
			aal_gauge_free(gauge);
			return -ENOMEM;
		}
	}

	hint = gauge->data;

	switch (gauge->type) {
	case GAUGE_PERCENTAGE:
		hint->u.rate_hint.rate = &progress->u.rate;
		break;
	case GAUGE_TREE: {
		repair_progress_tree_t *tree;

		tree = aal_calloc(sizeof(repair_progress_tree_t), 0);

		if (!tree) {
			if (progress->data == NULL) {
				aal_free(gauge->data);
				aal_gauge_free(gauge);
			}
			return -ENOMEM;
		}

		*tree = progress->u.tree;

		hint->u.tree_hint.tree = 
			aal_list_insert(hint->u.tree_hint.tree, tree, 
					aal_list_len(hint->u.tree_hint.tree));

		break;
	}
	default:
		return -EINVAL;
	}


	if (progress->data == NULL) {
		progress->data = gauge;
		aal_gauge_start(gauge);
	} else
		aal_gauge_update(gauge, hint->percent);

	return 0;
}

static void progress_end(repair_progress_t *progress) {
	aal_gauge_t *gauge;

	aal_assert("vpf-875", progress != NULL);
	aal_assert("vpf-878", progress->data != NULL);

	gauge = (aal_gauge_t *)progress->data;

	aal_assert("vpf-877", gauge->data != NULL);

	if (gauge->type == GAUGE_TREE) {
		repair_progress_tree_t *tree;	    
		gauge_hint_t *hint = gauge->data;
		aal_list_t *elem;

		elem = aal_list_last(hint->u.tree_hint.tree);
		tree = elem->data;

		progress->u.tree = *tree;

		hint->u.tree_hint.tree = aal_list_remove(hint->u.tree_hint.tree, tree);
		aal_free(tree);

		/* If some elements are left, if was not the upper level of the 
		 * tree, continue. Otherwise, gauge should be freed. */
		if (hint->u.tree_hint.tree) {
			aal_gauge_update(progress->data, hint->percent);
			return;
		}
	} else if (gauge->type == GAUGE_PERCENTAGE) {
		aal_assert("vpf-894", progress->u.rate.total == 
			   progress->u.rate.done);
	}


	aal_gauge_done(progress->data);

	aal_free(gauge->data);
	aal_gauge_free(gauge);
	progress->data = NULL;
}

static void progress_update(repair_progress_t *progress) {
	aal_gauge_t *gauge;
	gauge_hint_t *hint;

	aal_assert("vpf-879", progress != NULL);
	aal_assert("vpf-880", progress->data != NULL);

	gauge = (aal_gauge_t *)progress->data;

	aal_assert("vpf-888", gauge->data != NULL);

	hint = gauge->data;

	switch (gauge->type) {
	case GAUGE_PERCENTAGE:
		progress->u.rate.done++;
		hint->percent = progress->u.rate.done * 100 / 
			progress->u.rate.total;

		break;
	case GAUGE_TREE:
		{
			repair_progress_tree_t *tree;
			aal_list_t *elem;

			elem = aal_list_last(hint->u.tree_hint.tree);

			aal_assert("vpf-881", elem != NULL);

			tree = (repair_progress_tree_t *)elem->data;

			progress->u.tree.item++;
			progress->u.tree.unit++;
			*tree = progress->u.tree;

			break;
		}
	}

	aal_gauge_update(gauge, hint->percent);
}

errno_t gauge_handler(repair_progress_t *progress) {
	errno_t res;

	aal_assert("vpf-868", progress != NULL);

	switch(progress->state) {
	case PROGRESS_START:
		if ((res = progress_start(progress)))
			return res;
		break;
	case PROGRESS_END:
		progress_end(progress);
		break;
	case PROGRESS_UPDATE:
		progress_update(progress);
		break;
	case PROGRESS_STAT:
		misc_wipe_line(stderr);
		if (progress->text)
			fprintf(stderr, "%s\n", progress->text);

		break;
	}

	return 0;
}

