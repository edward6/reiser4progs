/*
  list.c -- double-linked list implementation.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <aal/aal.h>

/* Allocates new aal_list_t instance and assigns passed @data to it */
aal_list_t *aal_list_alloc(void *data) {
	aal_list_t *list;
   
	if (!(list  = (aal_list_t *)aal_calloc(sizeof(*list), 0)))
		return NULL;
    
	list->data = data;
	list->next = NULL;
	list->prev = NULL;
    
	return list;
}

/* Returns last item from the passed @list */
aal_list_t *aal_list_last(aal_list_t *list) {
	if (!list) return NULL;
    
	while (list->next)
		list = list->next;

	return list;
}

/* Returns first item from the passed @list */
aal_list_t *aal_list_first(aal_list_t *list) {
	if (!list) return NULL;
    
	while (list->prev)
		list = list->prev;

	return list;
}

/* Returns next item */
aal_list_t *aal_list_next(aal_list_t *list) {
	if (!list) return NULL;
	return list->next;
}

/* Returns prev item */
aal_list_t *aal_list_prev(aal_list_t *list) {
	if (!list) return NULL;
	return list->prev;
}

/* Returns list length */
uint32_t aal_list_length(aal_list_t *list) {
	uint32_t length = 0;

	while (list) {
		length++;
		list = list->next;
	}
	
	return length;
}

/*
  This functions makes walk though the @list and calls passed @func for each
  list item. This may be used for searching something, or performing some
  per-item actions.
*/
int aal_list_foreach(aal_list_t *list, foreach_func_t func, 
		     void *data) 
{

	if (!func)
		return -EINVAL;

	while (list) {
		int res;
	
		if ((res = func(list->data, data)))
			return res;
	
		list = list->next;
	}
	return 0;
}

/* Perform lookup inside @list for @data and returns its position */
int32_t aal_list_pos(aal_list_t *list, void *data) {
	int32_t pos = 0;

	while (list) {
		if (list->data == data)
			return pos;
	
		pos++;
		list = list->next;
	}
	return pos;
}

/* Gets list item at @n position */
aal_list_t *aal_list_at(aal_list_t *list, uint32_t n) {
	while ((n-- > 0) && list)
		list = list->next;

	return list;
}

/* Inserts new item at @n position of @list */
aal_list_t *aal_list_insert(aal_list_t *list, 
			    void *data, uint32_t n) 
{
	aal_list_t *temp;
	aal_list_t *new;
    
	if (n == 0)
		return aal_list_prepend(list, data);
    
	if (!(temp = aal_list_at(list, n)))
		return aal_list_append(list, data);

	if (!(new = aal_list_alloc(data)))
		return NULL;
    
	if (temp->prev) {
		temp->prev->next = new;
		new->prev = temp->prev;
	}
    
	new->next = temp;
	temp->prev = new;

	return temp == list ? new : list;
}

/* Inserts new item in sorted maner */
aal_list_t *aal_list_insert_sorted(aal_list_t *list, void *data,
				   comp_func_t comp_func, void *user)
{
	int cmp;
	aal_list_t *new_list;
	aal_list_t *tmp_list = list;

	if (!comp_func)
		return NULL;
    
	if (!list) {
		new_list = aal_list_alloc(data);
		return new_list;
	}
  
	cmp = comp_func((const void *)tmp_list->data,
			(const void *)data, user);
  
	while ((tmp_list->next) && (cmp < 0)) {
		tmp_list = tmp_list->next;
		cmp = comp_func((const void *)tmp_list->data,
				(const void *)data, user);
	}

	new_list = aal_list_alloc(data);

	if ((!tmp_list->next) && (cmp < 0)) {
		tmp_list->next = new_list;
		new_list->prev = tmp_list;
		return new_list;
	}
   
	if (tmp_list->prev) {
		tmp_list->prev->next = new_list;
		new_list->prev = tmp_list->prev;
	}
	
	new_list->next = tmp_list;
	tmp_list->prev = new_list;
 
	return new_list;
}

/* Inserts new item just before passed @list */
aal_list_t *aal_list_prepend(aal_list_t *list, void *data) {
	aal_list_t *new;
	aal_list_t *last;
    
	if (!(new = aal_list_alloc(data)))
		return 0;
    
	if (list) {
		if (list->prev) {
			list->prev->next = new;
			new->prev = list->prev;
		}
		list->prev = new;
		new->next = list;
	}
    
	return new;
}

/* Inserts new item just after passed @list */
aal_list_t *aal_list_append(aal_list_t *list, void *data) {
	aal_list_t *new;
    
	if (!(new = aal_list_alloc(data)))
		return 0;
    
	if (list) {
		if (list->next)
			list->next->prev = new;

		new->next = list->next;
		new->prev = list;

		list->next = new;

		return list;
	} else
		return new;
}

/* 
  Removes item from the passed @list and return reffernce to the next or prev
  list item.
*/
aal_list_t *aal_list_remove(aal_list_t *list, void *data) {
	aal_list_t *temp;
	aal_list_t *result = list;

	if (list && (temp = aal_list_find(list, data))) {
		if (temp->prev)
			temp->prev->next = temp->next;
	    
		if (temp->next)
			temp->next->prev = temp->prev;

		if (temp == list) {
			if (temp->next)
				result = temp->next;
			else if (temp->prev)
				result = temp->prev;
			else
				result = NULL;
		}
		
		aal_free(temp);
	}

	return result;
}

/* Returns list item by its data */
aal_list_t *aal_list_find(aal_list_t *list, void *data) {
	while (list) {
		if (list->data == data)
			return list;
	
		list = list->next;
	}
	return NULL;
}

aal_list_t *aal_list_find_custom(aal_list_t *list, void *needle, 
				 comp_func_t comp_func, void *user) 
{

	if (!comp_func)
		return NULL;
    
	while (list) {
		if (comp_func((const void *)list->data,
			      (const void *)needle, user) == 0)
			return list;

		list = list->next;
	}
	
	return NULL;
}

/* Releases all list items */
void aal_list_free(aal_list_t *list) {
	aal_list_t *last = list;
    
	if (!list) return;
    
	while (last->next) {
		aal_list_t *temp = last->next;
		aal_free(last);
		last = temp;
	}
}
