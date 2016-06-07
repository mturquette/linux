/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2016 AmLogic, Inc.
 * Author: Michael Turquette <mturquette@baylibre.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING
 *
 * BSD LICENSE
 *
 * Copyright (c) 2016 AmLogic, Inc.
 * Author: Michael Turquette <mturquette@baylibre.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * MultiPhase Locked Loops are outputs from a PLL with additional frequency
 * scaling capabilities. MPLL rates are calculated as:
 *
 * f(N2_integer, SDM_IN ) = 2.0G/(N2_integer + SDM_IN/16384)
 */

#include <linux/clk-provider.h>
//#include <linux/module.h>
//#include <linux/kernel.h>
//#include <linux/slab.h>
//#include <linux/io.h>
//#include <linux/err.h>
//#include <linux/string.h>
//#include <linux/log2.h>

//#include "clk-mpll.h"
//#include "clk.h"

#include "clkc.h"

#define SDM_MAX 16384

#if 0
#define MAX_RATE	500000000
#define MIN_RATE	5000000
#define N2_MAX		127
#define N2_MIN		4
#define ERROR		10000000
#endif

#if 0
#define SDM_IN_SHIFT	0;
#define SDM_IN_WIDTH	14;
#define N_IN_SHIFT	16;
#define N_IN_WIDTH	9;
#define SSEN_SHIFT	25;
#endif

static unsigned long mpll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct meson_clk_mpll *mpll = to_meson_clk_mpll(hw);
	struct parm *p;
	unsigned long rate = 0;
	unsigned long reg, sdm, n2;

	p = &mpll->sdm;
	reg = readl(mpll->base + p->reg_off);
	sdm = PARM_GET(p->width, p->shift, reg);

	p = &mpll->n2;
	reg = readl(mpll->base + p->reg_off);
	n2 = PARM_GET(p->width, p->shift, reg);

	rate = (parent_rate * SDM_MAX) / ((SDM_MAX * n2) + sdm);

	return rate;
}

/* FIXME check if this can be simplified. Because math. */
static long mpll_round_rate(struct clk_hw *hw,
	    unsigned long rate, unsigned long *parent_rate)
{
	//struct meson_clk_mpll *mpll = to_meson_clk_mpll(hw);
	unsigned long remainder, sdm, n2 = *parent_rate;

	remainder = do_div(n2, rate);
	sdm = DIV_ROUND_UP(remainder * SDM_MAX, rate);

	return (*parent_rate * SDM_MAX) / ((SDM_MAX * n2) + sdm);
}

static int mpll_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct meson_clk_mpll *mpll = to_meson_clk_mpll(hw);
	struct parm *p;
	unsigned long remainder, sdm, n2 = parent_rate;
	u32 reg;

	remainder = do_div(n2, rate);
	sdm = DIV_ROUND_UP(remainder * SDM_MAX, rate);

	p = &mpll->n2;
	reg = readl(mpll->base + p->reg_off);
	reg = PARM_SET(p->width, p->shift, reg, n2);
	writel(reg, mpll->base + p->reg_off);

	p = &mpll->sdm;
	reg = readl(mpll->base + p->reg_off);
	reg = PARM_SET(p->width, p->shift, reg, sdm);
	writel(reg, mpll->base + p->reg_off);

	return 0;
}

const struct clk_ops meson_clk_mpll_ops = {
	.recalc_rate = mpll_recalc_rate,
	.round_rate = mpll_round_rate,
	.set_rate = mpll_set_rate,
};
