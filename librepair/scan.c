/*
    librepair/scan.c - methods are needed for the fsck pass2. 
    Copyright (C) 1996-2002 Hans Reiser.

    The second fsck pass - scan - fsck zeros extent pointers which point 
    to an already used block. Builds a map of used blocks.
*/

#include <repair/librepair.h>

errno_t repair_scan_node_check(reiser4_joint_t *joint, void *data) {
    return repair_node_handle_pointers(joint->node, (repair_check_t *)data);
}
