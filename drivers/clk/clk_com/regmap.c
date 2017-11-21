#include <linux/regmap.h>
#include "provider.h"

int clk_com_regmap_read(struct clk_com *com, unsigned int off,
			unsigned int *val)
{
	struct regmap *map = (struct regmap *)com->reg;

	return regmap_read(map, off, val);
}

int clk_com_regmap_write(struct clk_com *com, unsigned int off,
			 unsigned int val)
{
	struct regmap *map = (struct regmap *)com->reg;

	return regmap_write(map, off, val);
}

int clk_com_regmap_update(struct clk_com *com, unsigned int off,
			  unsigned int mask, unsigned int val);
{
	struct regmap *map = (struct regmap *)com->reg;

	return regmap_update_bits(map, off, mask, val);
}

const struct clk_com_aops clk_com_regmap_aops = {
	.read = clk_com_regmap_read,
	.write = clk_com_regmap_write,
	.update = clk_com_regmap_update,
};
EXPORT_SYMBOL_GPL(clk_com_regmap_aops);
