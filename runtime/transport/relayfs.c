/* -*- linux-c -*- 
 * relayfs.c - relayfstransport functions
 *
 * Copyright (C) IBM Corporation, 2005, 2006
 * Copyright (C) Red Hat Inc, 2005, 2006, 2007
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

/* This file is only for older kernels that have no debugfs. */

/* relayfs is required! */
#if !defined (CONFIG_RELAYFS_FS) && !defined (CONFIG_RELAYFS_FS_MODULE)
#error "RelayFS does not appear to be in this kernel!"
#endif
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/init.h>
#include <linux/relayfs_fs.h>
#include <linux/namei.h>
#include "utt.h"

static int _stp_relay_flushing = 0;

static void _stp_remove_relay_dir(struct dentry *dir)
{
	if (dir)
		relayfs_remove_dir(dir);
}


/**
 *	_stp_subbuf_start - subbuf_start() relayfs callback implementation
 */
static int _stp_subbuf_start(struct rchan_buf *buf,
			     void *subbuf,
			     unsigned prev_subbuf_idx,
			     void *prev_subbuf)
{
	unsigned padding = buf->padding[prev_subbuf_idx];
	if (prev_subbuf)
		*((unsigned *)prev_subbuf) = padding;

	return sizeof(padding); /* reserve space for padding */
}

/**
 *	_stp_buf_full - buf_full() relayfs callback implementation
 */
static void _stp_buf_full(struct rchan_buf *buf,
			  unsigned subbuf_idx,
			  void *subbuf)
{
	unsigned padding = buf->padding[subbuf_idx];
	*((unsigned *)subbuf) = padding;
}

static struct rchan_callbacks stp_rchan_callbacks =
{
	.subbuf_start = _stp_subbuf_start,
	.buf_full = _stp_buf_full,
};


static void _stp_remove_relay_root(struct dentry *root)
{
	if (root)
		_stp_remove_relay_dir(root);
}

struct utt_trace *utt_trace_setup(struct utt_trace_setup *utts)
{
	struct utt_trace *utt;

	utt = _stp_kzalloc(sizeof(*utt));
	if (!utt)
		return NULL;

	utt->utt_tree_root = relayfs_create_dir(utts->root, NULL);
	if (!utt->utt_tree_root) {
		errk("couldn't get relay root dir.\n");
		return NULL;
	}

	
	kbug("relay_open %d %d\n",  utts->buf_size, utts->buf_nr);
	utt->rchan = relay_open("trace", utt->utt_tree_root, utts->buf_size, utts->buf_nr, 0, &stp_rchan_callbacks);
	if (!utt->rchan) {
		errk("couldn't create relay channel.\n");
		_stp_remove_relay_root(utt->utt_tree_root);
		return NULL;
	}

	utt->rchan->private_data = utt;
	utt->trace_state = Utt_trace_setup;
	utts->err = 0;
	return utt;
}

int utt_trace_startstop(struct utt_trace *utt, int start,
			unsigned int *trace_seq)
{
	int ret;

	if (!utt)
		return 0;

	/*
	 * For starting a trace, we can transition from a setup or stopped
	 * trace. For stopping a trace, the state must be running
	 */
	ret = -EINVAL;
	if (start) {
		if (utt->trace_state == Utt_trace_setup ||
		    utt->trace_state == Utt_trace_stopped) {
			if (trace_seq)
				(*trace_seq)++;
			smp_mb();
			utt->trace_state = Utt_trace_running;
			ret = 0;
		}
	} else {
		if (utt->trace_state == Utt_trace_running) {
			utt->trace_state = Utt_trace_stopped;
			_stp_relay_flushing = 1;
			relay_flush(utt->rchan);
			ret = 0;
		}
	}

	return ret;
}


int utt_trace_remove(struct utt_trace *utt)
{
	kbug("removing relayfs files. %d\n", utt->trace_state);
	if (utt && (utt->trace_state == Utt_trace_setup || utt->trace_state == Utt_trace_stopped)) {
		relay_close(utt->rchan);
		_stp_remove_relay_root(utt->utt_tree_root);
		kfree(utt);
	}
	return 0;
}
