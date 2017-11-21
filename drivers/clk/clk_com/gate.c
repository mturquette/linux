#include "provider.h"

static int clk_com_gate_endisable(struct clk_hw *hw, int enable)
{
	struct clk_com *com = to_clk_com(hw);
	struct clk_com_gate_data *data =
		(struct clk_com_gate_data*)com->data;
	int set = data->flags & CLK_GATE_SET_TO_DISABLE ? 1 : 0;
	unsigned int val = 0;

	if (set ^ enable)
		val = BIT(data->bit_idx);
	else
		val = 0;

	if (gate->flags & CLK_GATE_HIWORD_MASK) {
		val |= BIT(data->bit_idx + 16);
		return clk_com_write(com, data->offset, val);
	}

	return clk_com_update(com, data->offset, BIT(data->bit_idx), val);
}

static int clk_com_gate_enable(struct clk_hw *hw)
{
	return clk_gate_endisable(hw, 1);
}

static void clk_com_gate_disable(struct clk_hw *hw)
{
	if(clk_gate_endisable(hw, 0))
		pr_err("failed to disable gate\n");
}

int clk_com_gate_is_enabled(struct clk_hw *hw)
{
	struct clk_com *com = to_clk_com(hw);
	struct clk_com_gate_data *data =
		(struct clk_com_gate_data*)com->data;
	unsigned int val;
	int ret;

	ret = clk_com_read(com, data->offset, &val);
	if (ret) {
		pr_err("read failed, assume clk disabled\n");
		return CLK_GATE_SET_TO_DISABLE ? 1 : 0;
	}

	/* if a set bit disables this clk, flip it before masking */
	if (data->flags & CLK_GATE_SET_TO_DISABLE)
		val ^= BIT(data->bit_idx);

	val &= BIT(data->bit_idx);

	return val ? 1 : 0;
}


const struct clk_ops clk_com_gate_ops = {
	.enable = clk_com_gate_enable,
	.disable = clk_com_gate_disable,
	.is_enabled = clk_com_gate_is_enabled,
};
EXPORT_SYMBOL_GPL(clk_com_slow_gate_ops);

const struct clk_ops clk_com_slow_gate_ops = {
	.prepare = clk_com_gate_enable,
	.unprepare = clk_com_gate_disable,
	.is_prepared = clk_com_gate_is_enabled,
};
EXPORT_SYMBOL_GPL(clk_com_gate_ops);

const struct clk_ops clk_com_gate_ro_ops = {
	.is_enabled = clk_com_gate_is_enabled,
};
EXPORT_SYMBOL_GPL(clk_com_slow_gate_ops);

const struct clk_ops clk_com_slow_gate_ro_ops = {
	.is_prepared = clk_com_gate_is_enabled,
};
EXPORT_SYMBOL_GPL(clk_com_gate_ops);

/* That's a lot of ops .... scaling is going to be an issue
 * with regmap being possibly slow. It would be easier if we could 
 * query regmap sleepiness */
