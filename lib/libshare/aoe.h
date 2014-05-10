/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2014 Turbo Fredriksson <turbo@bayour.com>.
 */

#define	VBLADE_CMD_PATH		"/usr/bin/vbladed"
#define AOE_DEFAULT_SHELF	9
#define AOE_DEFAULT_SLOT	0
#define AOE_DEFAULT_IFACE	"eth0"

typedef struct aoe_shareopts_s {
	int  shelf;		/* shelf (major AoE) address */
	int  slot;		/* slot  (minor AoE) address */
	char netif[10];		/* name of the ethernet network interface */
	char status[10];	/* status of share - 'up' and ... ? */
	int  size;		/* Size of device in sectors */
	char path[PATH_MAX];	/* Device Path */

	struct aoe_shareopts_s *next;
} aoe_shareopts_t;

aoe_shareopts_t *aoe_shares;

void libshare_aoe_init(void);
