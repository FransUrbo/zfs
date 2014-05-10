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
#include <dirent.h>
#include <sys/stat.h>
#include <sys/list.h>

#include <libzfs.h>
#include <libshare.h>
#include "libshare_impl.h"
#include "aoe.h"

#if !defined(offsetof)
#define offsetof(s, m)  ((size_t)(&(((s *)0)->m)))
#endif

static boolean_t aoe_available(void);
static boolean_t aoe_is_share_active(sa_share_impl_t);
static int aoe_get_shareopts(sa_share_impl_t, const char *, aoe_shareopts_t **);

static sa_fstype_t *aoe_fstype;
static list_t all_aoe_shares_list;

typedef struct aoe_shares_list_s {
	char name[10];
	list_node_t next;
} aoe_shares_list_t;

/*
 * Simplified version of 'aoe-stat':

  for dir in /sys/block/*e[0-9]*\.[0-9]*; do
    test -r "$d/payload" && payload=yes	# ??
    dev="${dir/*\!/}";
    nif=`cat "$dir/netif"`;
    sec=`cat "$dir/size"`
    stt=`cat "$dir/state"`;
    psize=$(((512000 * $sec) / (1000 * 1000 * 1000)))
    psize=`printf "%04d\n" $psize | sed 's!\(...\)$!.\1!'`
    printf '%10s %15sGB %6s %-5s %-14s\n' $dev $psize $nif $payload $stt
  done
*/

/* TODO: This is an exact copy of iscsi.c:iscsi_read_sysfs_value() from #1099 */
static int
aoe_read_sysfs_value(char *path, char **value)
{
	int rc = SA_SYSTEM_ERR, buffer_len;
	char buffer[255];
	FILE *sysfs_file_fp = NULL;

	/* Make sure that path and value is set */
	assert(path != NULL);
	if (!value)
		return (rc);

	/*
	 * TODO:
	 * If *value is not NULL we might be dropping allocated memory, assert?
	 */
	*value = NULL;

#if DEBUG >= 2
	fprintf(stderr, "aoe_read_sysfs_value: path=%s", path);
#endif

	sysfs_file_fp = fopen(path, "r");
	if (sysfs_file_fp != NULL) {
		if (fgets(buffer, sizeof (buffer), sysfs_file_fp)
		    != NULL) {
			/* Trim trailing new-line character(s). */
			buffer_len = strlen(buffer);
			while (buffer_len > 0) {
			    buffer_len--;
			    if (buffer[buffer_len] == '\r' ||
				buffer[buffer_len] == '\n') {
				buffer[buffer_len] = 0;
			    } else
				break;
			}

			*value = strdup(buffer);

#if DEBUG >= 2
			fprintf(stderr, ", value=%s", *value);
#endif

			/* Check that strdup() was successful */
			if (*value)
				rc = SA_OK;
		}

		fclose(sysfs_file_fp);
	}

#if DEBUG >= 2
	fprintf(stderr, "\n");
#endif
	return (rc);
}

/*
 * Retrieve the list of AoE shares.
 * Do this only if we haven't already.
 * TODO: That doesn't work exactly as intended. Threading?
 */
static int
aoe_retrieve_shares(void)
{
	int ret;
	char *shelf = NULL, *slot = NULL, *netif = NULL, *status = NULL,
		*ssize = NULL, *buffer = NULL, tmp_path[255];
	DIR *dir;
	struct dirent *directory;
	struct stat eStat;
	aoe_shareopts_t *entry;

	/* Create the global share list  */
	list_create(&all_aoe_shares_list, sizeof (aoe_shareopts_t),
		    offsetof(aoe_shareopts_t, next));

	/* First retrieve a list of all shares, without info.
	 * DIR: /sys/block/*e[0-9]*\.[0-9]* 
	 * This is a link to /sys/devices/virtual/block/..., so
	 * start there.
	 */
	if ((dir = opendir("/sys/devices/virtual/block"))) {
		while ((directory = readdir(dir))) {
			if (directory->d_name[0] == '.' &&
			    !S_ISDIR(eStat.st_mode))
				continue;

			if (strcmp(directory->d_name, "etherd!") == 0) {
#ifdef DEBUG
				fprintf(stderr, "  aoe_retrieve_shares: %s\n",
					directory->d_name);
#endif
				/* TODO: From the path, retrieve shelf and slot */
				shelf = "9";
				slot  = "0";

				/* Get ethernet interface - netif */
				ret = snprintf(tmp_path, sizeof (tmp_path),
					       "%s/netif", directory->d_name);
				if (ret < 0 || ret >= sizeof (tmp_path))
					goto look_out;
				if (aoe_read_sysfs_value(tmp_path, &buffer)
				    != SA_OK)
					goto look_out;
				netif = buffer;
				buffer = NULL;

				/* Get state - status */
				ret = snprintf(tmp_path, sizeof (tmp_path),
					       "%s/state", directory->d_name);
				if (ret < 0 || ret >= sizeof (tmp_path))
					goto look_out;
				if (aoe_read_sysfs_value(tmp_path, &buffer)
				    != SA_OK)
					goto look_out;
				status = buffer;
				buffer = NULL;

				/* Get sector size - sector_size */
				ret = snprintf(tmp_path, sizeof (tmp_path),
					       "%s/size", directory->d_name);
				if (ret < 0 || ret >= sizeof (tmp_path))
					goto look_out;
				if (aoe_read_sysfs_value(tmp_path, &buffer)
				    != SA_OK)
					goto look_out;
				ssize = buffer;
				buffer = NULL;

				/* Put the linked list together */
				entry = (aoe_shareopts_t *) malloc(sizeof (aoe_shareopts_t));
				if (entry == NULL)
					goto look_out;

				/* TODO: How to get the physical ZVOL device? */
				strncpy(entry->path, NULL,
					sizeof (entry->path));

				strncpy(entry->netif, netif,
					sizeof (entry->netif));
				strncpy(entry->status, status,
					sizeof (entry->status));
				entry->shelf = atoi(shelf);
				entry->slot  = atoi(slot);
				entry->size  = atoi(ssize);

				list_insert_tail(&all_aoe_shares_list, entry);
			}
		}

look_out:
		closedir(dir);
	}

	return (SA_OK);
}

/*
 * Used internally by aoe_enable_share to enable sharing for a single host.
 */
static int
aoe_enable_share_one(sa_share_impl_t impl_share, const char *sharepath)
{
	char *argv[6], *shareopts, params_shelf[10], params_slot[10];
	aoe_shareopts_t *opts;
	int rc, ret;

	opts = (aoe_shareopts_t *) malloc(sizeof (aoe_shareopts_t));
	if (opts == NULL)
		return (SA_NO_MEMORY);

	/* Get any share options */
	shareopts = FSINFO(impl_share, aoe_fstype)->shareopts;
	rc = aoe_get_shareopts(impl_share, shareopts, &opts);
	if (rc < 0) {
		free(opts);
		return (SA_SYSTEM_ERR);
	}

#ifdef DEBUG
	fprintf(stderr, "aoe_enable_share_one_iet: shelf=%d, slot=%d, "
		"netif=%s, sharepath=%s\n", opts->shelf, opts->slot,
		opts->netif, impl_share->sharepath);
#endif

	ret = snprintf(params_shelf, sizeof (params_shelf), "%d", opts->shelf);
	if (ret < 0 || ret >= sizeof (params_shelf)) {
		free(opts);
		return (SA_SYSTEM_ERR);
	}

	ret = snprintf(params_slot, sizeof (params_slot), "%d", opts->slot);
	if (ret < 0 || ret >= sizeof (params_slot)) {
		free(opts);
		return (SA_SYSTEM_ERR);
	}

	/* vblade $shelf $slot $netif  /dev/zvol/$sharepath */
	argv[0] = VBLADE_CMD_PATH;
	argv[1] = (char *)params_shelf;
	argv[2] = (char *)params_slot;
	argv[3] = (char *)opts->netif;
	argv[4] = (char *)sharepath;
	argv[5] = NULL;

#ifdef DEBUG
	int i;
	fprintf(stderr, "CMD: ");
	for (i = 0; i < 7; i++)
		fprintf(stderr, "%s ", argv[i]);
	fprintf(stderr, "\n");
#endif

	rc = libzfs_run_process(argv[0], argv, STDERR_VERBOSE);
	if (rc != 0) {
		free(opts);
		return (SA_SYSTEM_ERR);
	}

	return (SA_OK);
}

/*
 * Enables AoE sharing for the specified share.
 */
static int
aoe_enable_share(sa_share_impl_t impl_share)
{
	char *shareopts;

	if (!aoe_available())
		return (SA_SYSTEM_ERR);

	shareopts = FSINFO(impl_share, aoe_fstype)->shareopts;
	if (shareopts == NULL) /* on/off */
		return (SA_SYSTEM_ERR);

	if (strcmp(shareopts, "off") == 0)
		return (SA_OK);

	/* Magic: Enable (i.e., 'create new') share */
	return (aoe_enable_share_one(impl_share, impl_share->sharepath));
}

/*
 * Used internally by aoe_disable_share to disable sharing for a single host.
 */
static int
aoe_disable_share_one(int shelf, int slot, const char *netif)
{
	/* TODO: How to disable a share
	   debian:~# ps | grep vblade
	   11982 ?        S      0:00 vblade 9 0 eth0 /dev/zvol/rpool/test
	*/
}

/*
 * Disables AoE sharing for the specified share.
 */
static int
aoe_disable_share(sa_share_impl_t impl_share)
{
	int ret;
	aoe_shareopts_t *share;

	if (!aoe_available()) {
		/*
		 * The share can't possibly be active, so nothing
		 * needs to be done to disable it.
		 */
		return (SA_OK);
	}

	/* Retrieve the list of (possible) active shares */
	aoe_retrieve_shares();
	for (share = list_head(&all_aoe_shares_list);
	     share != NULL;
	     share = list_next(&all_aoe_shares_list, share))
	{
#ifdef DEBUG
		fprintf(stderr, "aoe_disable_share: %s ?? %s (%d.%d/%s)\n",
			impl_share->sharepath, share->path, share->shelf,
			share->slot, share->netif);
#endif
		if (strcmp(impl_share->sharepath, share->path) == 0) {
#ifdef DEBUG
			fprintf(stderr, "=> disable %d.%d/%s (%s)\n",
				share->shelf, share->slot, share->netif,
				share->path);
#endif
			if((ret = aoe_disable_share_one(share->shelf,
				share->slot, share->netif))
			   == SA_OK)
				list_remove(&all_aoe_shares_list, share);
			return (ret);
		}
	}

	return (SA_OK);
}

/*
 * Validates share option(s).
 */
static int
aoe_get_shareopts_cb(const char *key, const char *value, void *cookie)
{
	char *dup_value;
	aoe_shareopts_t *opts = (aoe_shareopts_t *)cookie;

	if (strcmp(key, "on") == 0)
		return (SA_OK);

	/* Verify all options */
	if (strcmp(key, "shelf") != 0 &&
	    strcmp(key, "slot")  != 0 &&
	    strcmp(key, "netif") != 0)
		return (SA_SYNTAX_ERR);

	dup_value = strdup(value);
	if (dup_value == NULL)
		return (SA_NO_MEMORY);

	/* Get share option values */
	if (strcmp(key, "shelf") == 0)
		opts->shelf = atoi(dup_value);
	if (strcmp(key, "slot") == 0)
		opts->slot = atoi(dup_value);
	if (strcmp(key, "netif") == 0) {
		strncpy(opts->netif, dup_value, sizeof (opts->netif));
		opts->netif [sizeof (opts->netif)-1] = '\0';
	}

	return (SA_OK);
}

/*
 * Takes a string containing share options (e.g. "shelf=9,slot=0,netif=eth0")
 * and converts them to a NULL-terminated array of options.
 */
static int
aoe_get_shareopts(sa_share_impl_t impl_share, const char *shareopts,
		    aoe_shareopts_t **opts)
{
	int rc;
	aoe_shareopts_t *new_opts;

	assert(opts != NULL);
	*opts = NULL;

	new_opts = (aoe_shareopts_t *) calloc(sizeof (aoe_shareopts_t), 1);
	if (new_opts == NULL)
		return (SA_NO_MEMORY);

	/* Set defaults */
	new_opts->shelf = AOE_DEFAULT_SHELF;
	new_opts->slot  = AOE_DEFAULT_SLOT;
	strncpy(new_opts->netif, AOE_DEFAULT_IFACE, strlen(AOE_DEFAULT_IFACE));
	*opts = new_opts;

	rc = foreach_shareopt(shareopts, aoe_get_shareopts_cb, *opts);
	if (rc != SA_OK) {
		free(*opts);
		*opts = NULL;
	}

	return (rc);
}

/*
 * Checks whether the specified AoE share options are syntactically correct.
 */
static int
aoe_validate_shareopts(const char *shareopts)
{
	aoe_shareopts_t *opts;
	int rc = SA_OK;

	rc = aoe_get_shareopts(NULL, shareopts, &opts);

	free(opts);
	return (rc);
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
