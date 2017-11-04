/*
  volume.c -- reiser4 volume plugins.
  Plugins are needed for the fsck and for all  utilities when
  specifying them by the name with --override option.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_MINIMAL
#include "reiser4/plugin.h"
#include <misc/misc.h>


#define MAX_DEFAULT_STRIPE_SIZE (1 << 22)
#define REISER4_MAX_BRICKS_LOWER_LIMIT (1U << 10)
#define REISER4_MAX_BRICKS_UPPER_LIMIT (1U << 31)
#define REISER4_MAX_BRICKS_CONFIRM_LIMIT (1U << 20)

static inline uint64_t max(uint64_t a, uint64_t b)
{
	return a > b ? a : b;
}

static inline uint64_t min(uint64_t a, uint64_t b)
{
	return a < b ? a : b;
}

static inline uint64_t calibrate(uint64_t val, uint64_t low, uint64_t high)
{
	return max(min(val, high),low);
}

static int advise_stripe_size_simple(uint64_t *result, uint32_t block_size,
				     uint64_t block_count, int is_default,
				     int forced)
{
	/*
	 * For simple volumes default stripe has infinite size
	 */
	if (*result != 0 && *result < block_size) {
		/*
		 * bad stripe size
		 */
		aal_error("Invalid stripe size (%llu). It must not be smaller "
			  "than block size %u.",
			  *result, block_size);
		return -1;
	}
	return 0;
}

static int advise_stripe_size_asym(uint64_t *result, uint32_t block_size,
				   uint64_t block_count, int is_default,
				   int forced)
{
	uint64_t advised;

	advised = calibrate((block_count * block_size)/1024,
			    block_size,
			    MAX_DEFAULT_STRIPE_SIZE);
	if (is_default) {
		*result = advised;
		return 0;
	}
	if (*result != 0 && *result < block_size) {
		/*
		 * bad stripe size
		 */
		aal_error("Invalid stripe size (%llu). It must not be smaller "
			  "than block size %u.",
			  *result, block_size);
		return -1;
	}
	if ((*result == 0 || *result > advised) && !forced) {
		/*
		 * stripe is too large, use force flag
		 */
		aal_error("Stripe size %llu is too large. Use -f to force over",
			  *result);
		return -1;
	}
	return 0;
}

static int advise_max_bricks_simple(uint64_t *result, int forced)
{
	if (*result != 0) {
		aal_error("Option max-bricks is undefined for simple volumes");
		return -1;
	}
	return 0;
}

static int advise_max_bricks_asym(uint64_t *result, int forced)
{
	if (*result == 0) {
		*result = REISER4_MAX_BRICKS_LOWER_LIMIT;
		return 0;
	}
	if (*result > REISER4_MAX_BRICKS_UPPER_LIMIT) {
		aal_error("Invalid max bricks (%llu). It must not be larger "
			  "than %u.",
			  *result, REISER4_MAX_BRICKS_UPPER_LIMIT);
		goto error;
	}
	if ((*result > REISER4_MAX_BRICKS_CONFIRM_LIMIT) &&
	    !forced) {
		aal_error("Support of %llu bricks requires a lot of memory "
			  "resources. Use -f to force over.", *result);
		goto error;
	}
	if (*result < REISER4_MAX_BRICKS_LOWER_LIMIT) {
		*result = REISER4_MAX_BRICKS_LOWER_LIMIT;
		return 0;
	}
	if (*result != 1U << misc_log2(*result))
		/*
		 * round up to the power of 2
		 */
		*result = 1 << (1 + misc_log2(*result));
	return 0;
 error:
	return -1;
}

reiser4_vol_plug_t simple_vol_plug = {
	.p = {
		.id    = {VOL_SIMPLE_ID, 0, VOL_PLUG_TYPE},
		.label = "simple",
		.desc  = "Simple Logical Volume.",
	},
	.advise_stripe_size = advise_stripe_size_simple,
	.advise_max_bricks = advise_max_bricks_simple,
};

reiser4_vol_plug_t asym_vol_plug = {
	.p = {
		.id    = {VOL_ASYM_ID, 0, VOL_PLUG_TYPE},
		.label = "asym",
		.desc  = "Asymmetric Heterogeneous Logical Volume.",
	},
	.advise_stripe_size = advise_stripe_size_asym,
	.advise_max_bricks = advise_max_bricks_asym,
};

#endif

/*
  Local variables:
  c-indentation-style: "K&R"
  mode-name: "LC"
  c-basic-offset: 8
  tab-width: 8
  fill-column: 80
  scroll-step: 1
  End:
*/
