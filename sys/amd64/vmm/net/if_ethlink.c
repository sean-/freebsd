/*
 * Copyright (C) 2018 Matthew Macy <matt.macy@joyent.com>
 * Copyright (C) 2018 Joyent Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/priv.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/taskqueue.h>
#include <sys/limits.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if_vlan_var.h>
#include <net/iflib.h>
#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_media.h>

#include <net/if_vpc.h>

#include "ifdi_if.h"

static MALLOC_DEFINE(M_ETHLINK, "ethlink", "virtual private cloud interface");


#define ETHLINK_DEBUG

#ifdef ETHLINK_DEBUG
#define  DPRINTF printf
#else
#define DPRINTF(...)
#endif


/*
 * ifconfig ethlink0 create
 * ifconfig ethlink0 192.168.0.100
 * ifconfig ethlink0 attach vpc0
 */

struct ethlink_softc {
	if_softc_ctx_t shared;
	if_ctx_t vs_ctx;
	struct ifnet *ls_ifp;
};

static int clone_count;

static int
ethlink_mbuf_to_qid(if_t ifp __unused, struct mbuf *m __unused)
{
	return (0);
}

static int
ethlink_transmit(if_t ifp, struct mbuf *m)
{
	panic("%s should not be called\n", __func__);
	return (0);
}

#define ETHLINK_CAPS														\
	IFCAP_TSO | IFCAP_HWCSUM | IFCAP_VLAN_HWFILTER | IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_HWCSUM |	\
	IFCAP_VLAN_HWTSO | IFCAP_VLAN_MTU | IFCAP_TXCSUM_IPV6 | IFCAP_HWCSUM_IPV6 | IFCAP_JUMBO_MTU | \
	IFCAP_LINKSTATE

static int
ethlink_cloneattach(if_ctx_t ctx, struct if_clone *ifc, const char *name, caddr_t params)
{
	struct ethlink_softc *vs = iflib_get_softc(ctx);
	if_softc_ctx_t scctx;

	atomic_add_int(&clone_count, 1);
	vs->vs_ctx = ctx;

	scctx = vs->shared = iflib_get_softc_ctx(ctx);
	scctx->isc_capenable = ETHLINK_CAPS;
	scctx->isc_tx_csum_flags = CSUM_TCP | CSUM_UDP | CSUM_TSO | CSUM_IP6_TCP \
		| CSUM_IP6_UDP | CSUM_IP6_TCP;
	return (0);
}

static int
ethlink_attach_post(if_ctx_t ctx)
{
	struct ifnet *ifp;

	ifp = iflib_get_ifp(ctx);
	if_settransmitfn(ifp, ethlink_transmit);
	if_settransmittxqfn(ifp, ethlink_transmit);
	if_setmbuftoqidfn(ifp, ethlink_mbuf_to_qid);
	return (0);
}

static int
ethlink_detach(if_ctx_t ctx)
{
	struct ethlink_softc *ls = iflib_get_softc(ctx);

	if (ls->ls_ifp != NULL)
		if_rele(ls->ls_ifp);

	atomic_add_int(&clone_count, -1);

	return (0);
}

static void
ethlink_init(if_ctx_t ctx)
{
}

static void
ethlink_stop(if_ctx_t ctx)
{
}


struct ifnet *
ethlink_ifp_get(if_ctx_t ctx)
{
	struct ethlink_softc *ls = iflib_get_softc(ctx);

	return (ls->ls_ifp);
}

int
ethlink_ctl(vpc_ctx_t ctx, vpc_op_t op, size_t inlen, const void *in,
				 size_t *outlen, void **outdata)
{
	if_ctx_t ifctx = ctx->v_ifp->if_softc;
	struct ethlink_softc *ls;
	struct sockaddr_dl *sdl;
	char buf[IFNAMSIZ];
	int rc;


	rc = 0;
	ls = iflib_get_softc(ifctx);
	switch (op) {
		case VPC_ETHLINK_OP_ATTACH: {
			struct ifnet *ifp;

			bzero(buf, IFNAMSIZ);
			memcpy(buf, in, min(inlen, IFNAMSIZ-1));
			if ((ifp = ifunit_ref(buf)) == NULL)
				return (ENOENT);
			if (ifp->if_addr == NULL) {
				if_rele(ifp);
				return (ENXIO);
			}
			sdl = (struct sockaddr_dl *)ifp->if_addr->ifa_addr;
			if (sdl->sdl_type != IFT_ETHER) {
				if_rele(ifp);
				return (EINVAL);
			}
			iflib_set_mac(ifctx, LLADDR(sdl));
			ls->ls_ifp = ifp;
			break;
		}
		default:
			rc = EOPNOTSUPP;
			break;
	}
	return (rc);
}

static device_method_t ethlink_if_methods[] = {
	DEVMETHOD(ifdi_cloneattach, ethlink_cloneattach),
	DEVMETHOD(ifdi_attach_post, ethlink_attach_post),
	DEVMETHOD(ifdi_detach, ethlink_detach),
	DEVMETHOD(ifdi_init, ethlink_init),
	DEVMETHOD(ifdi_stop, ethlink_stop),
	DEVMETHOD_END
};

static driver_t ethlink_iflib_driver = {
	"ethlink", ethlink_if_methods, sizeof(struct ethlink_softc)
};

char ethlink_driver_version[] = "0.0.1";

static struct if_shared_ctx ethlink_sctx_init = {
	.isc_magic = IFLIB_MAGIC,
	.isc_driver_version = ethlink_driver_version,
	.isc_driver = &ethlink_iflib_driver,
	.isc_flags = IFLIB_PSEUDO,
	.isc_name = "ethlink",
};

if_shared_ctx_t ethlink_sctx = &ethlink_sctx_init;


static if_pseudo_t ethlink_pseudo;

static int
ethlink_module_init(void)
{
	ethlink_pseudo = iflib_clone_register(ethlink_sctx);

	return ethlink_pseudo != NULL ? 0 : ENXIO;
}

static void
ethlink_module_deinit(void)
{
	iflib_clone_deregister(ethlink_pseudo);
}

static int
ethlink_module_event_handler(module_t mod, int what, void *arg)
{
	int err;

	switch (what) {
	case MOD_LOAD:
		if ((err = ethlink_module_init()) != 0)
			return (err);
		break;
	case MOD_UNLOAD:
		if (clone_count == 0)
			ethlink_module_deinit();
		else
			return (EBUSY);
		break;
	default:
		return (EOPNOTSUPP);
	}

	return (0);
}

static moduledata_t ethlink_moduledata = {
	"ethlink",
	ethlink_module_event_handler,
	NULL
};

DECLARE_MODULE(ethlink, ethlink_moduledata, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(ethlink, 1);
MODULE_DEPEND(ethlink, iflib, 1, 1, 1);
