#ifndef __COM_PROVIDER_H
#define __COM_PROVIDER_H

#include <linux/clk-provider.h>

struct clk_com;

struct clk_com_aops {
	int (*read)(struct clk_com *com, unsigned int off, unsigned int *val);
	int (*write)(struct clk_com *com, unsigned int off, unsigned int val);
	int (*update)(struct clk_com *com, unsigned int off,
		      unsigned int mask, unsigned int val);
};

struct clk_com {
	struct clk_hw hw;
	struct clk_com_aops *aops;
	void *reg;
	void *data;
	spinlock_t *lock;
};

#define to_clk_com(_hw) container_of(_hw, struct clk_com, hw)

int clk_com_read(struct clk_com *com, unsigned int off, unsigned int *val);
int clk_com_write(struct clk_com *com, unsigned int off, unsigned int val);
int clk_com_update(struct clk_com *com, unsigned int off,
		   unsigned int mask, unsigned int val);

#ifdef CONFIG_REGMAP
extern const struct clk_com_access_ops clk_com_regmap_aops;
#endif

struct clk_com_gate_data {
	u32 offset;
	u8 flags;
	u8 bit_idx;
};

extern const struct clk_ops clk_com_gate_ops;
extern const struct clk_ops clk_com_slow_gate_ops;
extern const struct clk_ops clk_com_gate_ro_ops;
extern const struct clk_ops clk_com_slow_ro_gate_ops;

struct clk_com_mux_data {
	u32 *table;
	u32 offset;
	u32 mask;
	u8 shift;
	u8 flags;
};

extern const struct clk_ops clk_com_mux_ops;
extern const struct clk_ops clk_com_mux_ro_ops;

#endif /* __COM_PROVIDER_H */
