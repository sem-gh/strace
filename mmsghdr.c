/*
 * Copyright (c) 2010 Andreas Schwab <schwab@linux-m68k.org>
 * Copyright (c) 2012-2013 Denys Vlasenko <vda.linux@googlemail.com>
 * Copyright (c) 2014 Masatake YAMATO <yamato@redhat.com>
 * Copyright (c) 2010-2016 Dmitry V. Levin <ldv@altlinux.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "defs.h"
#include "msghdr.h"
#include <limits.h>

static int
fetch_struct_mmsghdr_or_printaddr(struct tcb *tcp, const long addr,
				  const unsigned int len, void *const mh)
{
	if ((entering(tcp) || !syserror(tcp))
	    && fetch_struct_mmsghdr(tcp, addr, mh)) {
		return 0;
	} else {
		printaddr(addr);
		return -1;
	}
}

struct print_struct_mmsghdr_config {
	const int *p_user_msg_namelen;
	unsigned int count;
	bool use_msg_len;
};

static bool
print_struct_mmsghdr(struct tcb *tcp, void *elem_buf,
		     size_t elem_size, void *data)
{
	const struct mmsghdr *const mmsg = elem_buf;
	struct print_struct_mmsghdr_config *const c = data;

	if (!c->count) {
		tprints("...");
		return false;
	}
	--c->count;

	tprints("{msg_hdr=");
	print_struct_msghdr(tcp, &mmsg->msg_hdr, c->p_user_msg_namelen,
			    c->use_msg_len ? mmsg->msg_len : -1UL);
	tprintf(", msg_len=%u}", mmsg->msg_len);

	return true;
}

static void
decode_mmsgvec(struct tcb *tcp, const unsigned long addr,
	       const unsigned int len, const bool use_msg_len)
{
	struct mmsghdr mmsg;
	struct print_struct_mmsghdr_config c = {
		.count = IOV_MAX,
		.use_msg_len = use_msg_len
	};

	print_array(tcp, addr, len, &mmsg, sizeof_struct_mmsghdr(),
		    fetch_struct_mmsghdr_or_printaddr,
		    print_struct_mmsghdr, &c);
}

void
dumpiov_in_mmsghdr(struct tcb *tcp, long addr)
{
	unsigned int len = tcp->u_rval;
	unsigned int i, fetched;
	struct mmsghdr mmsg;

	for (i = 0; i < len; ++i, addr += fetched) {
		fetched = fetch_struct_mmsghdr(tcp, addr, &mmsg);
		if (!fetched)
			break;
		tprintf(" = %lu buffers in vector %u\n",
			(unsigned long) mmsg.msg_hdr.msg_iovlen, i);
		dumpiov_upto(tcp, mmsg.msg_hdr.msg_iovlen,
			(long) mmsg.msg_hdr.msg_iov, mmsg.msg_len);
	}
}

SYS_FUNC(sendmmsg)
{
	if (entering(tcp)) {
		/* sockfd */
		printfd(tcp, tcp->u_arg[0]);
		tprints(", ");
		if (!verbose(tcp)) {
			printaddr(tcp->u_arg[1]);
			/* vlen */
			tprintf(", %u, ", (unsigned int) tcp->u_arg[2]);
			/* flags */
			printflags(msg_flags, tcp->u_arg[3], "MSG_???");
			return RVAL_DECODED;
		}
	} else {
		decode_mmsgvec(tcp, tcp->u_arg[1], tcp->u_rval, false);
		/* vlen */
		tprintf(", %u, ", (unsigned int) tcp->u_arg[2]);
		/* flags */
		printflags(msg_flags, tcp->u_arg[3], "MSG_???");
	}
	return 0;
}

SYS_FUNC(recvmmsg)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprints(", ");
		if (verbose(tcp)) {
			char *sts = xstrdup(sprint_timespec(tcp, tcp->u_arg[4]));
			set_tcb_priv_data(tcp, sts, free);
		} else {
			printaddr(tcp->u_arg[1]);
			/* vlen */
			tprintf(", %u, ", (unsigned int) tcp->u_arg[2]);
			/* flags */
			printflags(msg_flags, tcp->u_arg[3], "MSG_???");
			tprints(", ");
			print_timespec(tcp, tcp->u_arg[4]);
		}
		return 0;
	} else {
		if (verbose(tcp)) {
			decode_mmsgvec(tcp, tcp->u_arg[1], tcp->u_rval, true);
			/* vlen */
			tprintf(", %u, ", (unsigned int) tcp->u_arg[2]);
			/* flags */
			printflags(msg_flags, tcp->u_arg[3], "MSG_???");
			tprints(", ");
			/* timeout on entrance */
			tprints(get_tcb_priv_data(tcp));
		}
		if (syserror(tcp))
			return 0;
		if (tcp->u_rval == 0) {
			tcp->auxstr = "Timeout";
			return RVAL_STR;
		}
		if (!verbose(tcp) || !tcp->u_arg[4])
			return 0;
		/* timeout on exit */
		static char str[sizeof("left") + TIMESPEC_TEXT_BUFSIZE];
		snprintf(str, sizeof(str), "left %s",
			 sprint_timespec(tcp, tcp->u_arg[4]));
		tcp->auxstr = str;
		return RVAL_STR;
	}
}