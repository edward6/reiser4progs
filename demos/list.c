/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   list.c -- small list using example. */

#include <stdio.h>
#include <aal/aal.h>

int main(int argc, char *argv[]) {
	aal_list_t *list = NULL;
	aal_list_t *walk = NULL;

	list = aal_list_append(list, "First test line");
	aal_list_append(list, "Second test line");
	aal_list_append(list, "Third test line");
    
	aal_list_foreach_forward(list, walk)
		fprintf(stderr, "%s\n", (char *)walk->data);

	aal_list_free(list);
	return 0;
}

