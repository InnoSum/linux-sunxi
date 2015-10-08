#include "sun4i-ss.h"

static int sun4i_ss_hwrng_init(struct hwrng *hwrng)
{
	struct sun4i_ss_ctx *ss;

	ss = container_of(hwrng, struct sun4i_ss_ctx, hwrng);
	get_random_bytes(ss->seed, SS_SEED_LEN);

	return 0;
}

static int sun4i_ss_hwrng_read(struct hwrng *hwrng, void *buf,
		size_t max, bool wait)
{
	int i;
	u32 v;
	u32 *data = buf;
	u32 mode = SS_OP_PRNG | SS_PRNG_ONESHOT | SS_ENABLED;
	size_t len;
	struct sun4i_ss_ctx *ss;

	ss = container_of(hwrng, struct sun4i_ss_ctx, hwrng);
	len = min_t(size_t, SS_DATA_LEN, max);

	spin_lock_bh(&ss->slock);

	writel(mode, ss->base + SS_CTL);
	for (i = 0; i < SS_SEED_LEN / 4; i++)
		writel(ss->seed[i], ss->base + SS_KEY0 + i * 4);
	writel(mode | SS_PRNG_START, ss->base + SS_CTL);

#define SS_HWRNG_TIMEOUT 30
	/* if we are in SS_PRNG_ONESHOT mode, wait for completion */
	if ((mode & SS_PRNG_CONTINUE) == 0) {
		i = 0;
		do {
			v = readl(ss->base + SS_CTL);
			i++;
		} while (v != mode && i < SS_HWRNG_TIMEOUT);
		if (v != mode) {
			dev_err(ss->dev,
				"ERROR: hwrng end timeout %d>%d ctl=%x\n",
				i, SS_HWRNG_TIMEOUT, v);
			len = -EFAULT;
			goto release_ss;
		}
	}

	for (i = 0; i < len; i += 4) {
		v = readl(ss->base + SS_MD0 + i);
		*(data + i / 4) = v;
	}
	for (i = 0; i < SS_SEED_LEN / 4; i++) {
		v = readl(ss->base + SS_KEY0 + i * 4);
		ss->seed[i] = v;
	}
release_ss:
	writel(0, ss->base + SS_CTL);
	spin_unlock_bh(&ss->slock);
	return len;
}

int sun4i_ss_hwrng_register(struct hwrng *hwrng)
{
	hwrng->name = "sunxi Security System";
	hwrng->init = sun4i_ss_hwrng_init;
	hwrng->read = sun4i_ss_hwrng_read;

	/*sun4i_ss_hwrng_init(hwrng);*/
	return hwrng_register(hwrng);
}

void sun4i_ss_hwrng_remove(struct hwrng *hwrng)
{
	hwrng_unregister(hwrng);
}
