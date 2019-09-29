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


#define MIN_ADVISED_RATIO_BITS  (14)
#define MAX_ADVISED_STRIPE_SIZE (1 << 22)
#define MIN_NR_SGS (1U << 10)
#define MAX_NR_SGS (1U << 31)
#define CONFIRM_NR_SGS (1U << 20)

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

	advised =
		calibrate((block_count * block_size) >> MIN_ADVISED_RATIO_BITS,
			  block_size,
			  MAX_ADVISED_STRIPE_SIZE);
	if (is_default) {
		*result = advised;
		return 0;
	}
	if (*result < block_size) {
		/*
		 * bad stripe size
		 */
		aal_error("Invalid stripe size (%llu). It must not be smaller "
			  "than block size %u.",
			  *result, block_size);
		return -1;
	}
	if ((*result > advised) && !forced) {
		/*
		 * stripe is too large, use force flag
		 */
		aal_warn("Stripe of size %llu will be used.", *result);

		aal_error("It is too large and will lead to bad quality "
			  "of distribution. Use -f to force over");
		return -1;
	}
	return 0;
}

static int advise_nr_segments_simple(uint64_t *result, int forced)
{
	if (*result != 0) {
		aal_error("Option nr-segments is undefined for simple volumes");
		return -1;
	}
	return 0;
}

static int advise_nr_segments_asym(uint64_t *result, int forced)
{
	if (*result == 0) {
		*result = MIN_NR_SGS;
		return 0;
	}
	if (*result > MAX_NR_SGS) {
		aal_error("Invalid nr segments (%llu). It must not be larger "
			  "than %u.",
			  *result, MAX_NR_SGS);
		goto error;
	}
	if ((*result > CONFIRM_NR_SGS) &&
	    !forced) {
		aal_error("Support of %llu segments takes a lot of memory "
			  "resources. Use -f to force over.", *result);
		goto error;
	}
	if (*result < MIN_NR_SGS) {
		*result = MIN_NR_SGS;
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

static int check_data_capacity_simple(uint64_t result, uint64_t block_count, int forced)
{
	if (result != 0) {
		aal_error("Option data-capacity is undefined for simple volumes");
		return -1;
	}
	return 0;
}

static int check_data_capacity_asym(uint64_t result, uint64_t block_count, int forced)
{
	if ((result > block_count) && !forced) {
		aal_error("Data capacity (%llu) is larger than block count "
			  "(%llu). Use -f to force over.",
			  result, block_count);
		return -1;
	}
	return 0;
}

static uint64_t default_data_capacity_simple(uint64_t free_blocks)
{
	return 0;
}

static uint64_t default_data_capacity_asym(uint64_t free_blocks, int data_brick)
{
	if (data_brick)
		return free_blocks;
	/*
	 * for meta-data brick we assign 70% of free blocks
	 */
	return (90 * free_blocks) >> 7;
}

reiser4_vol_plug_t simple_vol_plug = {
	.p = {
		.id    = {VOL_SIMPLE_ID, 0, VOL_PLUG_TYPE},
		.label = "simple",
		.desc  = "Simple Logical Volume.",
	},
	.advise_stripe_size = advise_stripe_size_simple,
	.advise_nr_segments = advise_nr_segments_simple,
	.check_data_capacity = check_data_capacity_simple,
	.default_data_capacity = default_data_capacity_simple
};

reiser4_vol_plug_t asym_vol_plug = {
	.p = {
		.id    = {VOL_ASYM_ID, 0, VOL_PLUG_TYPE},
		.label = "asym",
		.desc  = "Asymmetric Heterogeneous Logical Volume.",
	},
	.advise_stripe_size = advise_stripe_size_asym,
	.advise_nr_segments = advise_nr_segments_asym,
	.check_data_capacity = check_data_capacity_asym,
	.default_data_capacity = default_data_capacity_asym
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
