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
 * Copyright (c) 2002, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2014 Turbo Fredriksson <turbo@bayour.com>, based on nfs.c
 *                         by Gunnar Beutner
 *
 * This is an addition to the zfs device driver to add, modify and remove AoE
 * (ATA-over-Ethernet) shares using the 'vblade' command that comes from
 * http://aoetools.sf.net.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <sys/stat.h>

#include <libzfs.h>
#include <libshare.h>
#include "libshare_impl.h"
#include "aoe.h"

static boolean_t aoe_available(void);
static boolean_t aoe_is_share_active(sa_share_impl_t);

static sa_fstype_t *aoe_fstype;

/*
 * Used internally by aoe_enable_share to enable sharing for a single host.
 */
static int
aoe_enable_share_one(const char *sharename, const char *sharepath)
{
}

/*
 * Enables AoE sharing for the specified share.
 */
static int
aoe_enable_share(sa_share_impl_t impl_share)
{
}

/*
 * Used internally by aoe_disable_share to disable sharing for a single host.
 */
static int
aoe_disable_share_one(const char *sharename)
{
}

/*
 * Disables AoE sharing for the specified share.
 */
static int
aoe_disable_share(sa_share_impl_t impl_share)
{
}

/*
 * Checks whether the specified AoE share options are syntactically correct.
 */
static int
aoe_validate_shareopts(const char *shareopts)
{
	/* TODO: Accept 'name' and sec/acl (?) */
	if ((strcmp(shareopts, "off") == 0) || (strcmp(shareopts, "on") == 0))
		return (SA_OK);

	return (SA_SYNTAX_ERR);
}

/*
 * Checks whether a share is currently active.
 */
static boolean_t
aoe_is_share_active(sa_share_impl_t impl_share)
{
	if (!aoe_available())
		return (B_FALSE);

	return (B_FALSE);
}

/*
 * Called to update a share's options. A share's options might be out of
 * date if the share was loaded from disk and the "shareaoe" dataset
 * property has changed in the meantime. This function also takes care
 * of re-enabling the share if necessary.
 */
static int
aoe_update_shareopts(sa_share_impl_t impl_share, const char *resource,
    const char *shareopts)
{
	char *shareopts_dup;
	boolean_t needs_reshare = B_FALSE;
	char *old_shareopts;

	if (!impl_share)
		return (SA_SYSTEM_ERR);

	FSINFO(impl_share, aoe_fstype)->active =
	    aoe_is_share_active(impl_share);

	old_shareopts = FSINFO(impl_share, aoe_fstype)->shareopts;

	if (FSINFO(impl_share, aoe_fstype)->active && old_shareopts != NULL &&
		strcmp(old_shareopts, shareopts) != 0) {
		needs_reshare = B_TRUE;
		aoe_disable_share(impl_share);
	}

	shareopts_dup = strdup(shareopts);

	if (shareopts_dup == NULL)
		return (SA_NO_MEMORY);

	if (old_shareopts != NULL)
		free(old_shareopts);

	FSINFO(impl_share, aoe_fstype)->shareopts = shareopts_dup;

	if (needs_reshare)
		aoe_enable_share(impl_share);

	return (SA_OK);
}

/*
 * Clears a share's AoE options. Used by libshare to
 * clean up shares that are about to be free()'d.
 */
static void
aoe_clear_shareopts(sa_share_impl_t impl_share)
{
	free(FSINFO(impl_share, aoe_fstype)->shareopts);
	FSINFO(impl_share, aoe_fstype)->shareopts = NULL;
}

static const sa_share_ops_t aoe_shareops = {
	.enable_share = aoe_enable_share,
	.disable_share = aoe_disable_share,

	.validate_shareopts = aoe_validate_shareopts,
	.update_shareopts = aoe_update_shareopts,
	.clear_shareopts = aoe_clear_shareopts,
};

/*
 * Provides a convenient wrapper for determining AoE availability
 */
static boolean_t
aoe_available(void)
{
	if (access(VBLADE_CMD_PATH, F_OK) != 0)
		return (B_FALSE);

	return (B_TRUE);
}

/*
 * Initializes the AoE functionality of libshare.
 */
void
libshare_aoe_init(void)
{
	aoe_fstype = register_fstype("aoe", &aoe_shareops);
}
