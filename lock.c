/*
 *
 * Copyright 2000, 2001, 2002 Hans Reiser <reiser@namesys.com>
 *
 */

/*
 * We assume that reiserfs tree operations, such as modification (balancing),
 * lookup (search_by_key) and maybe super balancing (squeeze left) should
 * require locking of small or huge chunks of tree nodes to perform these
 * operations.  We need to introduce per node lock object (it will be part of
 * znode object which is associated with each node (buffer) in cache), and we
 * need special techniques to avoid deadlocks.
 *
 * V4 LOCKING ALGORITHM DESCRIPTION
 *
 * Suppose we have a set of processes which do lock (R/W) of tree nodes. Each
 * process have a set (m.b. empty) of already locked nodes ("process locked
 * set"). Each process may have a pending lock request to the node locked by
 * another process (this request cannot be satisfied without unlocking the
 * node because request mode and node lock mode are not compatible).

 * The deadlock situation appears when we have a loop constructed from
 * process locked sets and lock request vectors.

 *
 * NOTE: we can have loops there because initial strict tree structure is
 * extended in reiser4 tree by having of sibling pointers which connect
 * neighboring cached nodes.
 *
 * +-P1-+          +-P3-+
 * |+--+|   V1     |+--+|
 * ||N1|| -------> ||N3||
 * |+--+|          |+--+|
 * +----+          +----+
 *   ^               |
 *   |V2             |V3
 *   |               v
 * +---------P2---------+
 * |+--+            +--+|
 * ||N2|  --------  |N4||
 * |+--+            +--+|
 * +--------------------+
 *
 * The basic deadlock avoidance mechanism used by everyone is to acquire locks
 * only in one predetermined order -- it makes loop described above
 * impossible.  The problem we have with chosen balancing algorithm is that we
 * could not find predetermined order of locking there. Tree lookup goes from
 * top to bottom locking more than one node at time, tree change (AKA
 * balancing) goes from bottom, two (or more) tree change operations may lock
 * same set of nodes in different orders. So, basic deadlock avoidance scheme
 * does not work there.
 *
 * Instead of having one predetermined order of getting locks for all
 * processes we divide all processes into two classes, and each class
 * has its own order of locking.
 *
 * Processes of first class (we call it L-class) take locks in from top to
 * tree bottom and from right to left, processes from second class (H-class)
 * take locks from bottom to top and left to right. It is not a complete
 * definition for the order of locking. Exact definitions will be given later,
 * now we should know that some locking orders are predefined for both
 * classes, L and H.
 *
 * How does it help to avoid deadlocks ?
 *
 * Suppose we have a deadlock with n processes. Processes from one
 * class never deadlock because they take locks in one consistent
 * order.
 *
 * So, any possible deadlock loop must have L-class processes as well as H-class
 * ones. This lock.c contains code that prevents deadlocks between L- and
 * H-class processes by using process lock priorities.
 *
 * We assign low priority to all L-class processes and high priority to all
 * processes from class H. There are no other lock priority levels except low
 * and high. We know that any deadlock loop contains at least one node locked
 * by L-class process and requested by H-class process. It is enough to avoid
 * deadlocks if this situation is caught and resolved.
 *
 * V4 DEADLOCK PREVENTION ALGORITHM IMPLEMENTATION.
 *
 * The deadlock prevention algorithm is based on comparing of
 * priorities of node owners (processes which keep znode locked) and
 * requesters (processes which want to acquire a lock on znode).  We
 * implement a scheme where low-priority owners yield locks to
 * high-priority requesters. We created a signal passing system that
 * is used to ask low-priority processes to yield one or more locked
 * znodes.
 *
 * The condition when a znode needs to change its owners is described by the
 * following formula:

   #############################################
   #                                           #
   # (number of high-priority requesters) >  0 #
   #                AND                        #
   # (numbers of high-priority owners)    == 0 #
   #                                           #
   #############################################

   *
   * FIXME: It is a bit different from initial statement of resolving of all
   * situations where we have a node locked by low-priority process and
   * requested by high-priority one.
   *
   * The difference is that in current implementation a low-priority process
   * delays node releasing if another high-priority process owns this node.
   *
   * It is not proved yet that modified deadlock check prevents deadlocks. It
   * is possible we should return to strict deadlock check formula as
   * described above in the proof (i.e. any low pri owners release locks when
   * high-pri lock request arrives, unrelated to another high-pri owner
   * existence). -zam.

 * It is enough to avoid deadlocks if we prevent any low-priority process from
 * falling asleep if its locked set contains a node which satisfies the
 * deadlock condition.
 *
 * That condition is implicitly or explicitly checked in all places where new
 * high-priority request may be added or removed from node request queue or
 * high-priority process takes or releases a lock on node. The main
 * goal of these checks is to never lose the moment when node becomes "has
 * wrong owners" and send "must-yield-this-lock" signals to its low-pri owners
 * at that time.
 *
 * The information about received signals is stored in the per-process
 * structure (lock stack) and analyzed before low-priority process is going to
 * sleep but after a "fast" attempt to lock a node fails. Any signal wakes
 * sleeping process up and forces him to re-check lock status and received
 * signal info. If "must-yield-this-lock" signals were received the locking
 * primitive (longterm_lock_znode()) fails with -EDEADLK error code.
 *
 * USING OF V4 LOCKING ALGORITHM IN V4 TREE TRAVERSAL CODE
 *
 * Let's complete definitions of lock orders used by L- and H-class
 * processes.
 *
 * 1. H-class:
 *    We count all tree nodes as shown at the picture:

  .
  .
  .
 (m+1)
  |  \
  |    \
 (n+1)  (n+2) ..                     (m)
  | \   | \                           | \
  1  2  3  4  5  6  7  8  9  10 .. (n-1) (n)

 * i.e. leaves first, then all nodes at one level above and so on until tree
 * root. H-class process acquires (and requests) locks in increasing order of
 * assigned numbers. Current implementation of v4 balancing follows these
 * rules.

 * 2. L-class:
 * Count all tree nodes in recursive order:

 .
 .
 13---
 |     \
 8      12
 | \      \
 3   7-    (11)
 |\  |\ \  | \
 1 2 4 5 6 9 (10) ...

 * and L-class process requests locks in decreasing order of assigned
 * numbers. You see that any movement left or down decreases the node number.
 * Recursive counting was used because one situation in
 * reiser4_get_left_neighbor() where process does low-pri lock request (to
 * left) after locking of a set of nodes as high-pri process in upward
 * direction.

 x <- c
      ^
      |
      b
      ^
      |
      a

 * Nodes a-b-c are locked in accordance with H-class rules, then the process
 * changes its class and tries to lock node x. Recursive order guarantees that
 * node x will have a number less than all already locked nodes (a-b-c)
 * numbers.
 *
 * V4 LOCKING DRAWBACKS
 * 
 * The deadlock prevention code may signal about possible deadlock in the
 * situation where is no deadlock actually. 
 *
 * Suppose we have a tree balancing operation in progress. One level is
 * completely balanced and we are going to propagate tree changes on the level
 * above. We locked leftmost node in the set of all immediate parents of all
 * modified nodes (by current balancing operation) on already balanced level
 * as it requires by locking order defined for H-class (high-priority)
 * processes.  At this moment we want to look on left neighbor for free space
 * on this level and thus avoid new node allocation. That locking is done in
 * the order not defined for H-class, but for L-class process. It means we
 * should change the process locking priority and make process locked set
 * available to high-priority requests from any balancing that goes upward. It
 * is not good.  First, for most cases it is not a deadlock situation; second,
 * locking of left neighbor node is impossible in that situation. -EDEADLK is
 * returned from longterm_lock_znode() until we release all nodes requested by
 * high-pri processes, we can't do it because nodes we need to unlock contain
 * modified data, we can't unroll changes due to reiser4 journal limits.
 *
 *  +-+     +-+
 *  |X| <-- | |
 *  +-+     +-+--LOW-PRI-----+
 *          |already balanced|
 *          +----------------+
 *              ^
 *              |
 *            +----HI-PRI-------+
 *            |another balancing|
 *            +-----------------+
 *
 * The solution is not to lock nodes to left in described situation. Current
 * implementation of reiser4 tree balancing algorithms uses non-blocking
 * try_lock call in that case.
 *
 * LOCK STRUCTURES DESCRIPTION
 *
 * The following data structures are used in reiser4 locking
 * implementation:
 *
 * Lock owner (lock_stack) is an lock objects accumulator. It
 * has a list of special link objects (lock_handle) each one
 * has a pointer to lock object this lock owner owns.  Reiser4 lock
 * object (reiser4_lock) has list of link objects of same type each
 * one points to lock owner object -- this lock object owner.  This
 * more complex structure (comparing with older design) is used
 * because we have to implement n<->m relationship between lock owners
 * and lock objects -- one lock owner may own several lock objects and
 * one lock object may belong to several lock owners (in case of
 * `read' or `shared' locks).  Yet another relation may exist between
 * one lock object and lock owners.  If lock procedure cannot
 * immediately take lock on an object it adds the lock owner on
 * special `requestors' list belongs to lock object.  That list
 * represents a queue of pending lock requests.  Because one lock
 * owner may request only only one lock object in a time, it is a 1>n
 * relation between lock objects and a lock owner implemented as it
 * written above. Full information (priority, pointers to lock and
 * link objects) about each lock request is stored in lock owner
 * structure in `request' field.
 *
 * SHORT_TERM LOCKING
 *
 * We use following scheme for protection of shared objects in smp
 * environment: locks (zlock objects) and lock owners
 * (lock_stack objects) have own spinlocks. The spinlock at
 * lock object protects all lock object fields and double-linked lists
 * of requesters and owner_links. The spinlock in lock_stack structure
 * protects only owner->nr_hipri_requests flag. There is simple rule to avoid
 * deadlocks: lock reiser4 lock object first, no more then one in a
 * time, then lock lock_stack object for owner->nr_hipri_requests flag
 * analysis/modification.
 *
 * Actually, lock object spinlocks do all we need. There are several cases of
 * concurrent access all protected by that spinlocks: putting/removing of
 * owner on/from requesters list at given lock object and linking/unlinking of
 * lock object and owner.
 *
 * The lock_handles lists don't need additional spinlocks for protection.
 * We can safely scan that lists when lock owner is not connected to
 * some `requesters' list because it means noone can pass new lock to us
 * and modify our lock_handles list. When lock owner is connected (and
 * while connecting/disconnecting) to requesters list, it is protected
 * by lock object (that owns req. list) spinlock.
 *
 * Only one case when lock object's spinlocks are not enough is protection of
 * owner object from concurrent access when we are passing signal to lock
 * owners.  The operations of sleep (with check for `nr_hipri_requests' flag) and
 * waking process up must serialized, which is currently done by taking
 * owner->guard spinlock.
 */

/* Josh's explanation to Zam on why the locking and capturing code are intertwined.
 *
 * FIXME: DEADLOCK STILL OBSERVED:!!!
 *
 * Point 1. The order in which a node is captured matters.  If a read-capture arrives
 * before a write-capture, the read-capture may cause no capturing "work" to be done at
 * all, whereas the write-capture may cause copy-on-capture to occur.  For this to be
 * correct the writer cannot beat the reader to the lock, if they are captured in the
 * opposite order.
 *
 * Point 2. Just as locking can block waiting for the request to be satisfied, capturing
 * can block waiting for expired atoms to commit.
 *
 * Point 3. It is not acceptable to first acquire the lock and then block waiting to
 * capture, especially when ignorant of deadlock.  There is no reason to lock until the
 * capture has succeeded.  To block in "capture" should be the same as to block waiting
 * for a lock, therefore a deadlock condition will cause the capture request to return
 * -EDEADLK.
 *
 * Point 4. It is acceptable to first capture and then wait to lock.  BUT, once the
 * capture request succeeds the lock request cannot be satisfied out-of-order.  For
 * example, once a read-capture is satisfied no writers may acquire the lock until the
 * reader has a chance to read the block.
 *
 * Point 5. Summary: capture requests must be partially-ordered with respect to lock
 * requests.
 *
 * What this means is for a regular lock_znode request (try_lock is slightly simpler).
 *
 *   1. acquire znode spinlock, check whether the lock request is compatible
 *      \_ and if not, make request and sleep on lock_stack semaphore
 *      |_ and when it wakes, go to step #1
 *   2. perform a try_capture request
 *      \_ and if it would block, sleep on lock_stack semaphore
 *      |_ and when it wakes, go to step #1
 *      |_ if the try_capture request is successful, it returns with
 *         the znode locked
 *   3. since try_capture occasionally releases the znode lock due to
 *      spinlock-ordering constraints (there's a pointer cycle atom->node->atom),
 *      recheck whether lock request is still compatible.
 *      \_ and if it is not, same as step #1
 *   4. before releasing znode spinlock, call lock_object() as before.  */


#include "reiser4.h"

/**
 * Returns a lock owner associated with current thread
 */
lock_stack* get_current_lock_stack ( void )
{
	return &get_current_context()->stack;
}

/**
 * Wakes up all low priority owners informing them about possible deadlock
 */
static void wake_up_all_lopri_owners (znode *node)
{
	lock_handle *handle = owners_list_front(&node->lock.owners);

	assert("nikita-1824", spin_znode_is_locked(node));
	while (!owners_list_end(&node->lock.owners, handle)) {
		spin_lock_stack(handle->owner);

		assert("nikita-1832", handle->node == node);
		/*
		 * count this signal in owner->nr_signaled */
		if (!handle->signaled) {
			handle->signaled = 1;
			atomic_inc(&handle->owner->nr_signaled);
		}
		/*
		 * Wake up a single process */
		__reiser4_wake_up(handle->owner);

		spin_unlock_stack(handle->owner);
		handle = owners_list_next(handle);
	}
}

/*
 * Adds a lock to a lock owner, which means creating a link to the lock and
 * putting the link into the two lists all links are on (the doubly linked list
 * that forms the lock_stack, and the doubly linked list of links attached
 * to a lock.
 */
static inline void link_object (
	lock_handle *handle, lock_stack *owner, znode *node
)
{
	assert ("jmacd-810", handle->owner == NULL);
	assert ("nikita-1828", owner == get_current_lock_stack());
	assert ("nikita-1830", spin_znode_is_locked (node));

	handle->owner = owner;
	handle->node = node;
	locks_list_clean(handle);
	owners_list_clean(handle);
	locks_list_push_back(&owner->locks, handle);
	owners_list_push_front(&node->lock.owners, handle);
	handle->signaled = 0;
}

/*
 * Breaks a relation between a lock and its owner
 */
static inline void unlink_object (lock_handle *handle)
{
	assert ("zam-354", handle->owner != NULL);
	assert ("nikita-1608", handle->node != NULL);
	assert ("nikita-1633", spin_znode_is_locked(handle->node));
	assert ("nikita-1829", handle->owner == get_current_lock_stack());

	if(REISER4_DEBUG) {
		locks_list_remove_clean(handle);
		owners_list_remove_clean(handle);
	} else {
		locks_list_remove(handle);
		owners_list_remove(handle);
	}
		
	/* indicates that lock handle is free now */
	handle->owner = NULL;
}

/*
 * Actually locks an object knowing that we are able to do this
 */
static void lock_object (lock_stack *owner, znode *node)
{
	assert("nikita-1834", spin_znode_is_locked(node));
	assert("nikita-1839", owner == get_current_lock_stack());

	if (owner->request.mode == ZNODE_READ_LOCK) {
		node->lock.nr_readers++;
	} else {
		/* check that we don't switched from read to write lock */
		assert("nikita-1840", node->lock.nr_readers <= 0);
		/* We allow recursive locking; a node can be locked several
		 * times for write by same process */
		node->lock.nr_readers--;
		ON_DEBUG_MODIFY(znode_pre_write(node));
	}

	link_object(owner->request.handle, owner, node);

	if (owner->curpri) {
		node->lock.nr_hipri_owners ++;
	}
	trace_on (TRACE_LOCKS, "%spri lock: %p node: %p: hipri_owners: %u: nr_readers: %d\n", owner->curpri ? "hi" : "lo", owner, node, node->lock.nr_hipri_owners, node->lock.nr_readers);
}

/**
 * Check for recursive write locking
 */
static int recursive (lock_stack *owner, znode *node)
{
	int ret;
	/*
	 * Owners list is not empty for a locked node */
	assert("zam-314", !owners_list_empty(&node->lock.owners));
	assert("nikita-1841", owner == get_current_lock_stack());
	assert("nikita-1848", spin_znode_is_locked(node));

	ret = (owners_list_front(&node->lock.owners)->owner == owner);

	/* Recursive read locking should be done usual way */
	assert("zam-315", !ret || owner->request.mode == ZNODE_WRITE_LOCK);
	/* mixing of read/write locks is not allowed */
	assert("zam-341", !ret || znode_is_wlocked(node));

	return ret;
}

#if REISER4_DEBUG
/* Returns true if the lock is held by the calling thread. */
int znode_is_any_locked( const znode *node )
{
	lock_handle *handle;
	lock_stack  *stack;
	int ret;
	
	if (! znode_is_locked( node )) {
		return 0;
	}

	stack = get_current_lock_stack ();

	spin_lock_stack (stack);

	ret = 0;
	
	for (handle = locks_list_front (& stack->locks);
	            ! locks_list_end   (& stack->locks, handle);
	     handle = locks_list_next  (handle)) {

		if (handle->node == node) {
			ret = 1;
			break;
		}
	}

	spin_unlock_stack (stack);

	return ret;
}

/* Returns true if a write lock is held by the calling thread. */
int znode_is_write_locked( const znode *node )
{
	lock_stack  *stack;
	lock_handle *handle;

	assert ("jmacd-8765", node != NULL);
	
	if( ! znode_is_wlocked( node ) ) {
		return 0;
	}

	stack = get_current_lock_stack();	

	/* If it is write locked, then all owner handles must equal the current stack. */
	handle = owners_list_front( &node -> lock.owners );

	return ( handle -> owner == stack );
}
#endif

/**
 * This "deadlock" condition is the essential part of reiser4 locking
 * implementation. This condition is checked explicitly by calling
 * check_deadlock_condition() or implicitly in all places where znode lock
 * state (set of owners and request queue) is changed. Locking code is
 * designed to use this condition to trigger procedure of passing object from
 * low priority owner(s) to high priority one(s).
 *
 * The procedure results in passing an event (setting lock_handle->signaled
 * flag) and count this event in nr_signaled field of owner's lock stack
 * object and wakeup owner's process.
 */
static inline int check_deadlock_condition (znode *node)
{
	assert ("nikita-1833", spin_znode_is_locked (node));
	return node->lock.nr_hipri_requests > 0
		&& node->lock.nr_hipri_owners == 0;
}

/**
 * checks lock/request compatibility
 */
static int can_lock_object (lock_stack *owner, znode *node)
{
	assert("nikita-1842", owner == get_current_lock_stack());
	assert("nikita-1843", spin_znode_is_locked(node));

	/* See if the node is disconnected. */
	if (ZF_ISSET (node, JNODE_IS_DYING)) {
		trace_on (TRACE_LOCKS, "attempt to lock dying znode: %p", node);
		return -EINVAL;
	}

	/* Do not ever try to take a lock if we are going in low priority
	 * direction and a node have a high priority request without high
	 * priority owners. */
	if (!owner->curpri && check_deadlock_condition(node)) {
		return -EAGAIN;
	}

	if (!is_lock_compatible(node, owner->request.mode) &&
	    !recursive(owner, node)) {
		return -EAGAIN;
	}

	return 0;
}

/**
 * Setting of a high priority to the process. It clears "signaled" flags
 * because znode locked by high-priority process can't satisfy our "deadlock
 * condition".
 */
static void set_high_priority(lock_stack *owner)
{
	assert("nikita-1846", owner == get_current_lock_stack());
	/* Do nothing if current priority is already high */
	if (!owner->curpri) {
		lock_handle *item = locks_list_front(&owner->locks);
		while (!locks_list_end(&owner->locks, item)) {
			znode *node = item->node;

			spin_lock_znode(node);

			node->lock.nr_hipri_owners++;

			trace_on (TRACE_LOCKS, "set_hipri lock: %p node: %p: hipri_owners after: %u nr_readers: %d\n", item, node, node->lock.nr_hipri_owners, node->lock.nr_readers);

			/* we can safely set signaled to zero, because
			 * previous statement (nr_hipri_owners ++) guarantees
			 * that signaled will be never set again. */
			item->signaled = 0;
			spin_unlock_znode(node);

			item = locks_list_next(item);
		}
		owner->curpri = 1;
		atomic_set(&owner->nr_signaled, 0);
	}
}

/**
 * Sets a low priority to the process.
 */
static void set_low_priority(lock_stack *owner)
{
	/* Do nothing if current priority is already low */
	if (owner->curpri) {
		/*
		 * scan all locks (lock handles) held by @owner, which is
		 * actually current thread, and check whether we are reaching
		 * deadlock possibility anywhere.
		 */
		lock_handle *handle = locks_list_front(&owner->locks);
		while (!locks_list_end(&owner->locks, handle)) {
			znode *node = handle->node;
			spin_lock_znode(node);
			/*
			 * this thread just was hipri owner of @node, so
			 * nr_hipri_owners has to be greater than zero.
			 */
			/* FIXME: JMACD->ZAM/NIKITA: This is failing for some reason. */
			trace_on (TRACE_LOCKS, "set_lopri lock: %p node: %p: hipri_owners before: %u nr_readers: %d\n", handle, node, node->lock.nr_hipri_owners, node->lock.nr_readers);
			assert("nikita-1835", node->lock.nr_hipri_owners > 0);
			node->lock.nr_hipri_owners--;
			/*
			 * If we have deadlock condition, adjust a nr_signaled
			 * field. It is enough to set "signaled" flag only for
			 * current process, other low-pri owners will be
			 * signaled and waken up after current process unlocks
			 * this object and any high-priority requestor takes
			 * control. */
			if (check_deadlock_condition(node)
			    && !handle->signaled)
			{
				handle->signaled = 1;
				atomic_inc(&owner->nr_signaled);
			}
			spin_unlock_znode(node);
			handle = locks_list_next(handle);
		}
		owner->curpri = 0;
	}
}

/**
 * unlock a znode long term lock
 */
void longterm_unlock_znode (lock_handle *handle)
{
	znode *node =  handle->node;
	lock_stack *oldowner = handle->owner;

	assert ("jmacd-1021", handle != NULL);
	assert ("jmacd-1022", handle->owner != NULL);
	ON_DEBUG_CONTEXT (assert ("nikita-1392", 
				  lock_counters()->long_term_locked_znode > 0));

	assert("zam-130", oldowner == get_current_lock_stack());

	ON_DEBUG_CONTEXT(-- lock_counters()->long_term_locked_znode);

	spin_lock_znode(node);

	assert("zam-101", znode_is_locked (node));

	/* Adjust a number of high priority owners of this lock*/
	if (oldowner->curpri) {
		assert("nikita-1836", node->lock.nr_hipri_owners > 0);
		node->lock.nr_hipri_owners--;
	}
	trace_on (TRACE_LOCKS, "%spri unlock: %p node: %p: hipri_owners: %u nr_readers %d\n", oldowner->curpri ? "hi" : "lo", handle, node, node->lock.nr_hipri_owners, node->lock.nr_readers);

	/* Last write-lock release. */
	if (znode_is_wlocked_once(node)) {

		ON_DEBUG_MODIFY (WITH_DATA (node, znode_post_write (node)));

		/* Handle znode deallocation */
		if (ZF_ISSET(node, JNODE_HEARD_BANSHEE)) {
			assert("nikita-1221", ergo (znode_is_loaded(node), 
						    node_is_empty(node)));

			/*
			 * invalidate lock. FIXME-NIKITA locking.  This doesn't
			 * actually deletes node, only removes it from
			 * sibling list and invalidates lock. Lock
			 * invalidation includes waking up all threads
			 * still waiting on this node and notifying them
			 * that node is dying.
			 */
			spin_unlock_znode(node);
			ON_DEBUG(check_lock_data());
			ON_DEBUG(check_lock_node_data(node));
			ON_DEBUG(node_check (node, REISER4_NODE_PANIC));
			forget_znode(handle);
			assert( "nikita-2191", znode_invariant( node ) );
			zput(node);
			return;
		}
	}

	if (handle->signaled) atomic_dec(&oldowner->nr_signaled);

	/* Unlocking means owner<->object link deletion */
	unlink_object(handle);

	/* This is enough to be sure whether an object is completely
	 * unlocked. */
	if (znode_is_rlocked (node))
		node->lock.nr_readers--;
	else
		node->lock.nr_readers++;

	/* If the node is locked it must have an owners list.  Likewise, if the node is
	 * unlocked it must have an empty owners list. */
	assert("zam-319", znode_is_locked(node) || owners_list_empty(&node->lock.owners));
	assert("zam-320", !znode_is_locked(node) || !owners_list_empty(&node->lock.owners));

	/* If there are pending lock requests we wake up a requestor */
	if (!requestors_list_empty(&node->lock.requestors)) {
		lock_stack *requestor;

		requestor = requestors_list_front(&node->lock.requestors);
		reiser4_wake_up(requestor);
	}

	spin_unlock_znode(node);

	/* minus one reference from handle->node */
	handle->node = NULL;
	assert( "nikita-2190", znode_invariant( node ) );
	zput(node);
	ON_DEBUG( check_lock_data() );
	ON_DEBUG( check_lock_node_data( node ) );
}

/**
 * locks given lock object
 */
int longterm_lock_znode (
	/* local link object (may be allocated on the process owner); */
	lock_handle *handle,
	/* znode we want to lock. */
	znode               *node ,
	/* {ZNODE_READ_LOCK, ZNODE_WRITE_LOCK}; */
	znode_lock_mode      mode ,
	/* {0, -EINVAL, -EDEADLK}, see return codes description. */ 
	znode_lock_request   request)
{
	int ret;
	int hipri = (request & ZNODE_LOCK_HIPRI) != 0;
	int wake_up_next = 0;
	txn_capture try_capture_flags = 0;
	int non_blocking = 0;

	/* Get current process context */
	lock_stack *owner = get_current_lock_stack();

	/* Check that the lock handle is initialized and isn't already being used. */
	assert ("jmacd-808", handle->owner == NULL);

	ON_DEBUG_CONTEXT (assert ("nikita-1391", 
				  lock_counters()->spin_locked == 0));
	if (request & ZNODE_LOCK_NONBLOCK)  {
		try_capture_flags |= TXN_CAPTURE_NONBLOCKING;
		non_blocking = 1;
	}

	if (request & ZNODE_LOCK_DONT_FUSE)
		try_capture_flags |= TXN_CAPTURE_DONT_FUSE;

	/* If we are changing our process priority we must adjust a number
	 * of high priority owners for each znode that we already lock */
	if (hipri) {
		set_high_priority(owner);
	} else {
		set_low_priority(owner);
	}

	reiser4_stat_znode_add(lock_znode);
	/* Synchronize on node's guard lock. */
	spin_lock_znode(node);

	/* Fill request structure with our values. */
	owner->request.mode   = mode;
	owner->request.handle = handle;
	owner->request.node   = node;

	for (;;) {

		reiser4_stat_znode_add(lock_znode_iteration);

		/* Check the lock's availability: if it is unavaiable we get EAGAIN, 0
		 * indicates "can_lock", otherwise the node is invalid.  */
		ret = can_lock_object(owner, node);

		if (ret == -EINVAL) {
			/* wakeup next requestor to support lock invalidating */
			wake_up_next = 1;
			break;
		}

		if ( ret == -EAGAIN && non_blocking) {
			break;
		}

		assert("nikita-1844", 
		       (ret == 0) || ((ret == -EAGAIN) && !non_blocking));
		/* If we can get the lock... Try to capture first before taking the
		 * lock.*/
		if ((ret = txn_try_capture (ZJNODE (node), mode, try_capture_flags)) != 0) {
			/* In the failure case, the txnmgr releases the znode's lock (or
			 * in some cases, it was released a while ago).  There's no need
			 * to reacquire it so we should return here, avoid releasing the
			 * lock. */
			owner->request.mode = 0;
			/* next requestor may not fail */
			wake_up_next = 1;
			if ((ret != -EAGAIN) && 
			    (ret != -EINTR) && (ret != -EDEADLK)) {
				warning("nikita-1845", 
					"Failed to capture node: %i",
					ret);
				print_znode("node", node);
			}
			break;
		}

		/* Check the lock's availability again -- this is because under some
		 * circumstances the capture code has to release and reacquire the znode
		 * spinlock. */
		ret = can_lock_object(owner, node);

		/* This time, a return of (ret == 0) means we can lock, so we should break
		 * out of the loop. */
		if (ret != -EAGAIN || non_blocking) {
			break;
		}

		/* Lock is unavailable, we have to wait. */

		/* By having semaphore initialization here we cannot lose
		 * wakeup signal even if it comes after `nr_signaled' field
		 * check. */
		if ((ret = prepare_to_sleep(owner))) {
			break;
		}

		assert("nikita-1837", spin_znode_is_locked(node));
		if (hipri) {
			/*
			 * If we are going in high priority direction then
			 * increase high priority requests counter for the
			 * node */
			node->lock.nr_hipri_requests++;
			/*
			 * If there no high priority owners for a node,
			 * then immediately wake up low priority owners,
			 * so they can detect possible deadlock */
			if (node->lock.nr_hipri_owners == 0)
				wake_up_all_lopri_owners(node);
			/*
			 * And prepare a lock request */
			requestors_list_push_front(
				&node->lock.requestors, owner);
		} else {
			/*
			 * If we are going in low priority direction then we
			 * set low priority to our process. This is the only
			 * case	 when a process may become low priority */
			/*
			 * And finally prepare a lock request */
			requestors_list_push_back(
				&node->lock.requestors, owner);
		}

		/*
		 * Ok, here we have prepared a lock request, so unlock a
		 * znode ...*/
		spin_unlock_znode(node);
		/*
		 * ... and sleep */
		ret = go_to_sleep(owner);

		spin_lock_znode(node);
		if (hipri) {
			assert("nikita-1838", node->lock.nr_hipri_requests > 0);
			node->lock.nr_hipri_requests--;
		}

		requestors_list_remove(owner);
		if (ret) {
			warning("nikita-2266", "Wait interrupted: %i", ret);
			break;
		}
	}

	assert ("jmacd-807", spin_znode_is_locked (node));

	/* If we broke with (ret == 0) it means we can_lock, now do it. */
	if (ret == 0) {
		lock_object(owner, node);
		owner->request.mode = 0;
		if (mode == ZNODE_READ_LOCK)
			wake_up_next = 1;
	}

	if (wake_up_next && !requestors_list_empty(&node->lock.requestors)) {
		lock_stack *next =
			requestors_list_front(&node->lock.requestors);
		reiser4_wake_up(next);
	}

	spin_unlock_znode(node);

	if (ret == 0) {
		/* 
		 * count a reference from lockhandle->node 
		 *
		 * znode was already referenced at the entry to this function,
		 * hence taking spin-lock here is not necessary (see comment
		 * in the zref()).
		 */
		zref (node);

		ON_DEBUG_CONTEXT(++ lock_counters()->long_term_locked_znode);
		if (REISER4_DEBUG && mode == ZNODE_WRITE_LOCK)
			node_check (node, REISER4_NODE_PANIC);
	}

	ON_DEBUG( check_lock_data() );
	ON_DEBUG( check_lock_node_data( node ) );
	return ret;
}

/**
 * lock object invalidation means changing of lock object state to `INVALID'
 * and waiting for all other processes to cancel theirs lock requests.
 */
void invalidate_lock (lock_handle *handle /* path to lock
							   * owner and lock
							   * object is being
							   * invalidated. */ )
{
	znode *node = handle->node;
	lock_stack *owner = handle->owner;
	lock_stack *rq;

	assert("zam-325", owner == get_current_lock_stack());

	spin_lock_znode(node);

	assert("zam-103", znode_is_write_locked(node));
	assert("nikita-1393", ! ZF_ISSET(node, JNODE_LEFT_CONNECTED));
	assert("nikita-1793", ! ZF_ISSET(node, JNODE_RIGHT_CONNECTED));
	assert("nikita-1394", ZF_ISSET(node, JNODE_HEARD_BANSHEE));

	if (handle->signaled) atomic_dec(&owner->nr_signaled);

	ZF_SET(node, JNODE_IS_DYING);
	unlink_object(handle);
	node->lock.nr_readers = 0;

	/* all requestors will be informed that lock is invalidated. */
	for (rq = requestors_list_front(&node->lock.requestors);
	        ! requestors_list_end(&node->lock.requestors, rq);
	     rq = requestors_list_next(rq)) {
		reiser4_wake_up(rq);
	}

	/* We use that each unlock() will wakeup first item from requestors
	 * list; our lock stack is the last one. */
	while (!requestors_list_empty(&node->lock.requestors)) {
		requestors_list_push_back(&node->lock.requestors, owner);

		prepare_to_sleep(owner);

		spin_unlock_znode(node);
		go_to_sleep(owner);
		spin_lock_znode(node);

		requestors_list_remove(owner);
	}

	spin_unlock_znode(node);
}

/**
 * Initializes lock_stack.
 */
void init_lock_stack (lock_stack *owner /* pointer to
							 * allocated
							 * structure. */)
{
	xmemset(owner, 0, sizeof(lock_stack));
	locks_list_init(&owner->locks);
	requestors_list_clean (owner);
	spin_lock_init(&owner->sguard);
	owner->curpri = 1;
	sema_init(&owner->sema, 0);
}

/**
 * Initializes lock object.
 */
void reiser4_init_lock (zlock *lock /* pointer on allocated
					     * uninitialized lock object
					     * structure. */ )
{
	xmemset(lock, 0, sizeof(zlock));
	requestors_list_init(&lock->requestors);
	owners_list_init(&lock->owners);
}

/* lock handle initialization */
void init_lh (lock_handle *handle)
{
	xmemset(handle, 0, sizeof *handle);
	locks_list_clean(handle);
	owners_list_clean(handle);
}

/* freeing of lock handle resources */
void done_lh (lock_handle *handle)
{
	assert ("zam-342", handle != NULL);
	if (handle->owner != NULL)
		longterm_unlock_znode(handle);
}

/* What kind of lock? */
znode_lock_mode lock_mode (lock_handle *handle)
{
	if (handle->owner == NULL) {
		return ZNODE_NO_LOCK;
	} else if (znode_is_rlocked (handle->node)) {
		return ZNODE_READ_LOCK;
	} else {
		return ZNODE_WRITE_LOCK;
	}
}

/**
 * Transfer a lock handle (presumably so that variables can be moved between stack and
 * heap locations).
 */
void move_lh_internal (lock_handle * new, lock_handle * old, int unlink_old)
{
	znode * node = old -> node;
	lock_stack * owner = old -> owner;
	int signaled;

	/*
	 * locks_list, modified by link_object() is not protected by
	 * anything. This is valid because only current thread ever modifies
	 * locks_list of its lock_stack.
	 */
	assert( "nikita-1827", owner == get_current_lock_stack() );
	assert( "nikita-1831", new -> owner == NULL );

	spin_lock_znode( node );

	new -> node  = node;

	signaled = old->signaled;
	if( unlink_old ) {
		unlink_object( old );
	} else {
		if (node->lock.nr_readers > 0) {
			node->lock.nr_readers += 1;
		} else {
			node->lock.nr_readers -= 1;
		}
		if (signaled) {
			atomic_inc (& owner->nr_signaled);
		}
		if (owner->curpri) {
			node->lock.nr_hipri_owners += 1;
		}
		ON_DEBUG_CONTEXT(++ lock_counters()->long_term_locked_znode);

		zref (node);
	}
	link_object( new, owner, node );
	new->signaled = signaled;

	spin_unlock_znode( node );
}

void move_lh (lock_handle * new, lock_handle * old) { move_lh_internal (new, old, /*unlink_old*/1); }
void copy_lh (lock_handle * new, lock_handle * old) { move_lh_internal (new, old, /*unlink_old*/0); }

/* after getting -EDEADLK we unlock znodes until this function returns false */
int check_deadlock ( void )
{
	lock_stack * owner = get_current_lock_stack();
	return atomic_read(&owner->nr_signaled) != 0;
}

/**
 * Reset the semaphore (under protection of lock_stack spinlock) to avoid lost
 * wake-up. */
int prepare_to_sleep (lock_stack *owner)
{
	assert("nikita-1847", owner == get_current_lock_stack());

	if(0) {
		/*
		 * FIXME-NIKITA: I commented call to sema_init() out hoping
		 * that it is the reason or thread sleeping in
		 * down(&owner->sema) without any other thread running.
		 *
		 * Anyway, it is just an optimization: is semaphore is not
		 * reinitialised at this point, in the worst case
		 * longterm_lock_znode() would have to iterate its loop once
		 * more.
		 */
		spin_lock_stack(owner);
		sema_init(&owner->sema, 0);
		spin_unlock_stack(owner);
	}

	/*
	 * FIXME-NIKITA: this check looks bogus, because it is done without
	 * any king of lock being held. So, it looks like ->nr_signaled can
	 * change just after this function returns. But, the only place where
	 * ->nr_signaled can be changed concurrently, that is, where
	 * owner->nr_signaled is changed and 
	 *
	 *               owner != get_current_lock_stack()
	 * 
	 * is wake_up_all_lopri_owners() and this function calls up() later.
	 * 
	 */
	if (atomic_read(&owner->nr_signaled) != 0 && !owner->curpri) {
		return -EDEADLK;
	}
	return 0;
}

/*
 * Wakes up a single thread
 */
void __reiser4_wake_up (lock_stack *owner)
{
	up(&owner->sema);
}

/*
 * Puts a thread to sleep
 */
int go_to_sleep (lock_stack *owner)
{
	return down_interruptible(&owner->sema);
}

int lock_stack_isclean (lock_stack *owner)
{
	if (locks_list_empty (& owner->locks)) {
		assert ("zam-353", atomic_read(&owner->nr_signaled) == 0);
		return 1;
	}

	return 0;
}

/*
 * Debugging help
 */
void show_lock_stack (reiser4_context *context)
{
	lock_handle *handle;
	lock_stack  *owner = & context->stack;

	spin_lock_stack (owner);

	info (".. lock stack:\n");
	info (".... nr_signaled %d\n", atomic_read (& owner->nr_signaled));
	info (".... curpri %s\n", owner->curpri ? "high" : "low");

	if (owner->request.mode != 0) {
		info (".... current request: %s", owner->request.mode == ZNODE_WRITE_LOCK ? "write" : "read");
		print_address( "", znode_get_block (owner->request.node));
	}

	info (".... current locks:\n");

	for (handle = locks_list_front (& owner->locks);
	            ! locks_list_end   (& owner->locks, handle);
	     handle = locks_list_next  (handle)) {

		if (handle->node != NULL)
			print_address( znode_is_rlocked (handle->node) ? "......  read" : "...... write", znode_get_block (handle->node));
	}

	spin_unlock_stack (owner);
}

#if REISER4_DEBUG

void check_lock_stack( lock_stack *stack )
{
	spin_lock_stack( stack );
	locks_list_check( &stack -> locks );
	spin_unlock_stack( stack );
}

extern spinlock_t active_contexts_lock;
extern context_list_head active_contexts;

void check_lock_data()
{
	if (0) {
		reiser4_context *context;

		spin_lock (& active_contexts_lock);
		for( context = context_list_front( &active_contexts ) ;
		     ! context_list_end( &active_contexts, context ) ;
		     context = context_list_next( context ) ) {
			check_lock_stack( &context -> stack );
		}
		spin_unlock (& active_contexts_lock);
	} else
		check_lock_stack( &get_current_context() -> stack );
}

void check_lock_node_data( znode *node )
{
	spin_lock_znode( node );
	owners_list_check( &node -> lock.owners );
	requestors_list_check( &node -> lock.requestors );
	spin_unlock_znode( node );
}

#endif

/*
 * Make Linus happy.
 * Local variables:
 * c-indentation-style: "K&R"
 * mode-name: "LC"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 120
 * End:
 */
