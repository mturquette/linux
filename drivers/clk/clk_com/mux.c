#include "provider.h"

static u8 clk_com_mux_get_parent(struct clk_hw *hw)
{
	struct clk_com *com = to_clk_com(hw);
	struct clk_com_mux_data *data =
		(struct clk_com_mux_data*)com->data;
	int num_parents = clk_hw_get_num_parents(hw);
	int ret;
	u32 val;

	ret = clk_com_read(com, data->offset, &val);
	if (ret)
		return ret;

	val &= data->mask;

	/*
	 * Should we use transfer function for this kind
	 * of stuff ??
	 */
	if (data->table) {
		int i;

		for (i = 0; i < num_parents; i++)
			if (data->table[i] == val)
				return i;
		return -EINVAL;
	}

	if (val && (data->flags & CLK_MUX_INDEX_BIT))
		val = ffs(val) - 1;

	if (val && (data->flags & CLK_MUX_INDEX_ONE))
		val--;

	if (val >= num_parents)
		return -EINVAL;

	return val;
}

static int clk_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_com *com = to_clk_com(hw);
	struct clk_com_mux_data *data =
		(struct clk_com_mux_data*)com->data;
	unsigned int mask = data->mask;
	unsigned int val;

	if (data->table) {
		index = data->table[index];
	} else {
		if (data->flags & CLK_MUX_INDEX_BIT)
			index = 1 << index;

		if (data->flags & CLK_MUX_INDEX_ONE)
			index++;
	}

	val = index << mux->shift;

	if (mux->flags & CLK_MUX_HIWORD_MASK) {
		val |= mux->mask << (mux->shift + 16);
		return clk_com_write(com, data->offset, val);
	}

	return clk_com_update(com, data->offset,
			      mux->mask << mux->shift, val);
}

const struct clk_ops clk_com_mux_ops = {
	.get_parent = clk_com_mux_get_parent,
	.set_parent = clk_com_mux_set_parent,
	.determine_rate = __clk_mux_determine_rate,
};
EXPORT_SYMBOL_GPL(clk_com_mux_ops);

const struct clk_ops clk_com_mux_ro_ops = {
	.get_parent = clk_com_mux_get_parent,
};
EXPORT_SYMBOL_GPL(clk_com_mux_ro_ops);
