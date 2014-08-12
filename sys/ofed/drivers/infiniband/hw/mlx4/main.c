/*
 * Copyright (c) 2006, 2007 Cisco Systems, Inc. All rights reserved.
 * Copyright (c) 2007, 2008 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/rtnetlink.h>
#include <linux/if_vlan.h>

#include <rdma/ib_smi.h>
#include <rdma/ib_user_verbs.h>
#include <rdma/ib_addr.h>

#include <linux/mlx4/driver.h>
#include <linux/mlx4/cmd.h>

#include "mlx4_ib.h"
#include "user.h"
#include "wc.h"

#define DRV_NAME	MLX4_IB_DRV_NAME
#define DRV_VERSION	"1.0-ofed1.5.2"
#define DRV_RELDATE	"August 4, 2010"

MODULE_AUTHOR("Roland Dreier");
MODULE_DESCRIPTION("Mellanox ConnectX HCA InfiniBand driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VERSION);

#ifdef CONFIG_MLX4_DEBUG

int mlx4_ib_debug_level = 0;
module_param_named(debug_level, mlx4_ib_debug_level, int, 0644);
MODULE_PARM_DESC(debug_level, "Enable debug tracing if > 0");

#endif /* CONFIG_MLX4_DEBUG */

static const char mlx4_ib_version[] =
	DRV_NAME ": Mellanox ConnectX InfiniBand driver v"
	DRV_VERSION " (" DRV_RELDATE ")\n";

static void *get_ibdev(struct mlx4_dev *dev, void *ctx, u8 port)
{
       struct mlx4_ib_dev *mlxibdev = ctx;
       return &mlxibdev->ib_dev;
}

struct update_gid_work {
	struct work_struct work;
	union ib_gid gids[128];
	int port;
	struct mlx4_ib_dev *dev;
};

static struct workqueue_struct *wq;

static void init_query_mad(struct ib_smp *mad)
{
	mad->base_version  = 1;
	mad->mgmt_class    = IB_MGMT_CLASS_SUBN_LID_ROUTED;
	mad->class_version = 1;
	mad->method	   = IB_MGMT_METHOD_GET;
}

static union ib_gid zgid;

static int mlx4_ib_query_device(struct ib_device *ibdev,
				struct ib_device_attr *props)
{
	struct mlx4_ib_dev *dev = to_mdev(ibdev);
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof *in_mad, GFP_KERNEL);
	out_mad = kmalloc(sizeof *out_mad, GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id = IB_SMP_ATTR_NODE_INFO;

	err = mlx4_MAD_IFC(to_mdev(ibdev), 1, 1, 1, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memset(props, 0, sizeof *props);

	props->fw_ver = dev->dev->caps.fw_ver;
	props->device_cap_flags    = IB_DEVICE_CHANGE_PHY_PORT |
		IB_DEVICE_PORT_ACTIVE_EVENT		|
		IB_DEVICE_SYS_IMAGE_GUID		|
		IB_DEVICE_RC_RNR_NAK_GEN		|
		IB_DEVICE_BLOCK_MULTICAST_LOOPBACK;
	if (dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_BAD_PKEY_CNTR)
		props->device_cap_flags |= IB_DEVICE_BAD_PKEY_CNTR;
	if (dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_BAD_QKEY_CNTR)
		props->device_cap_flags |= IB_DEVICE_BAD_QKEY_CNTR;
	if (dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_APM)
		props->device_cap_flags |= IB_DEVICE_AUTO_PATH_MIG;
	if (dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_UD_AV_PORT)
		props->device_cap_flags |= IB_DEVICE_UD_AV_PORT_ENFORCE;
	if (dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_IPOIB_CSUM)
		props->device_cap_flags |= IB_DEVICE_UD_IP_CSUM;
	if (dev->dev->caps.max_gso_sz && dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_BLH)
		props->device_cap_flags |= IB_DEVICE_UD_TSO;
	if (dev->dev->caps.bmme_flags & MLX4_BMME_FLAG_RESERVED_LKEY)
		props->device_cap_flags |= IB_DEVICE_LOCAL_DMA_LKEY;
	if ((dev->dev->caps.bmme_flags & MLX4_BMME_FLAG_LOCAL_INV) &&
	    (dev->dev->caps.bmme_flags & MLX4_BMME_FLAG_REMOTE_INV) &&
	    (dev->dev->caps.bmme_flags & MLX4_BMME_FLAG_FAST_REG_WR))
		props->device_cap_flags |= IB_DEVICE_MEM_MGT_EXTENSIONS;
	if (dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_XRC)
		props->device_cap_flags |= IB_DEVICE_XRC;
	if (dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_RAW_ETY)
		props->max_raw_ethy_qp = dev->ib_dev.phys_port_cnt;

	props->vendor_id	   = be32_to_cpup((__be32 *) (out_mad->data + 36)) &
		0xffffff;
	props->vendor_part_id	   = be16_to_cpup((__be16 *) (out_mad->data + 30));
	props->hw_ver		   = be32_to_cpup((__be32 *) (out_mad->data + 32));
	memcpy(&props->sys_image_guid, out_mad->data +	4, 8);

	props->max_mr_size	   = ~0ull;
	props->page_size_cap	   = dev->dev->caps.page_size_cap;
	props->max_qp		   = dev->dev->caps.num_qps - dev->dev->caps.reserved_qps;
	props->max_qp_wr	   = dev->dev->caps.max_wqes - MLX4_IB_SQ_MAX_SPARE;
	props->max_sge		   = min(dev->dev->caps.max_sq_sg,
					 dev->dev->caps.max_rq_sg);
	props->max_cq		   = dev->dev->caps.num_cqs - dev->dev->caps.reserved_cqs;
	props->max_cqe		   = dev->dev->caps.max_cqes;
	props->max_mr		   = dev->dev->caps.num_mpts - dev->dev->caps.reserved_mrws;
	props->max_pd		   = dev->dev->caps.num_pds - dev->dev->caps.reserved_pds;
	props->max_qp_rd_atom	   = dev->dev->caps.max_qp_dest_rdma;
	props->max_qp_init_rd_atom = dev->dev->caps.max_qp_init_rdma;
	props->max_res_rd_atom	   = props->max_qp_rd_atom * props->max_qp;
	props->max_srq		   = dev->dev->caps.num_srqs - dev->dev->caps.reserved_srqs;
	props->max_srq_wr	   = dev->dev->caps.max_srq_wqes - 1;
	props->max_srq_sge	   = dev->dev->caps.max_srq_sge;
	props->max_fast_reg_page_list_len = MAX_FAST_REG_PAGES;
	props->local_ca_ack_delay  = dev->dev->caps.local_ca_ack_delay;
	props->atomic_cap	   = dev->dev->caps.flags & MLX4_DEV_CAP_FLAG_ATOMIC ?
		IB_ATOMIC_HCA : IB_ATOMIC_NONE;
	props->masked_atomic_cap   = IB_ATOMIC_HCA;
	props->max_pkeys	   = dev->dev->caps.pkey_table_len[1];
	props->max_mcast_grp	   = dev->dev->caps.num_mgms + dev->dev->caps.num_amgms;
	props->max_mcast_qp_attach = dev->dev->caps.num_qp_per_mgm;
	props->max_total_mcast_qp_attach = props->max_mcast_qp_attach *
					   props->max_mcast_grp;
	props->max_map_per_fmr = (1 << (32 - ilog2(dev->dev->caps.num_mpts))) - 1;

out:
	kfree(in_mad);
	kfree(out_mad);

	return err;
}

static enum rdma_link_layer
mlx4_ib_port_link_layer(struct ib_device *device, u8 port_num)
{
	struct mlx4_dev *dev = to_mdev(device)->dev;

	return dev->caps.port_mask[port_num] == MLX4_PORT_TYPE_IB ?
		IB_LINK_LAYER_INFINIBAND : IB_LINK_LAYER_ETHERNET;
}

static void ib_link_query_port(struct ib_device *ibdev, u8 port,
			       struct ib_port_attr *props,
			       struct ib_smp *out_mad)
{
	props->lid		= be16_to_cpup((__be16 *) (out_mad->data + 16));
	props->lmc		= out_mad->data[34] & 0x7;
	props->sm_lid		= be16_to_cpup((__be16 *) (out_mad->data + 18));
	props->sm_sl		= out_mad->data[36] & 0xf;
	props->state		= out_mad->data[32] & 0xf;
	props->phys_state	= out_mad->data[33] >> 4;
	props->port_cap_flags	= be32_to_cpup((__be32 *) (out_mad->data + 20));
	props->gid_tbl_len	= to_mdev(ibdev)->dev->caps.gid_table_len[port];
	props->max_msg_sz	= to_mdev(ibdev)->dev->caps.max_msg_sz;
	props->pkey_tbl_len	= to_mdev(ibdev)->dev->caps.pkey_table_len[port];
	props->bad_pkey_cntr	= be16_to_cpup((__be16 *) (out_mad->data + 46));
	props->qkey_viol_cntr	= be16_to_cpup((__be16 *) (out_mad->data + 48));
	props->active_width	= out_mad->data[31] & 0xf;
	props->active_speed	= out_mad->data[35] >> 4;
	props->max_mtu		= out_mad->data[41] & 0xf;
	props->active_mtu	= out_mad->data[36] >> 4;
	props->subnet_timeout	= out_mad->data[51] & 0x1f;
	props->max_vl_num	= out_mad->data[37] >> 4;
	props->init_type_reply	= out_mad->data[41] >> 4;
	props->link_layer	= IB_LINK_LAYER_INFINIBAND;
}

#ifdef notyet
static int eth_to_ib_width(int w)
{
	switch (w) {
	case 4:
		return IB_WIDTH_4X;
	case 8:
	case 16:
		return IB_WIDTH_8X;
	case 32:
		return IB_WIDTH_12X;
	default:
		return IB_WIDTH_1X;
	}
}

static int eth_to_ib_speed(int s)
{
	switch (s) {
	case 256:
		return 1;
	case 512:
		return 2;
	case 1024:
		return 4;
	default:
		return 1;
	}
}
#endif

static u8 state_to_phys_state(enum ib_port_state state)
{
	return state == IB_PORT_ACTIVE ? 5 : 3;
}

static int eth_link_query_port(struct ib_device *ibdev, u8 port,
			       struct ib_port_attr *props,
			       struct ib_smp *out_mad)
{
	struct mlx4_ib_iboe *iboe = &to_mdev(ibdev)->iboe;
	struct net_device *ndev;
	enum ib_mtu tmp;

	props->active_width	= IB_WIDTH_4X;
	props->active_speed	= 1;
	props->port_cap_flags	= IB_PORT_CM_SUP;
	props->gid_tbl_len	= to_mdev(ibdev)->dev->caps.gid_table_len[port];
	props->max_msg_sz	= to_mdev(ibdev)->dev->caps.max_msg_sz;
	props->pkey_tbl_len	= 1;
	props->bad_pkey_cntr	= be16_to_cpup((__be16 *) (out_mad->data + 46));
	props->qkey_viol_cntr	= be16_to_cpup((__be16 *) (out_mad->data + 48));
	props->max_mtu		= IB_MTU_2048;
	props->subnet_timeout	= 0;
	props->max_vl_num	= out_mad->data[37] >> 4;
	props->init_type_reply	= 0;
	props->link_layer	= IB_LINK_LAYER_ETHERNET;
	props->state		= IB_PORT_DOWN;
	props->phys_state	= state_to_phys_state(props->state);
	props->active_mtu	= IB_MTU_256;
	spin_lock(&iboe->lock);
	ndev = iboe->netdevs[port - 1];
	if (!ndev)
		goto out;

#ifdef __linux__
	tmp = iboe_get_mtu(ndev->mtu);
#else
	tmp = iboe_get_mtu(ndev->if_mtu);
#endif
	props->active_mtu = tmp ? min(props->max_mtu, tmp) : IB_MTU_256;
	props->state		= netif_carrier_ok(ndev) &&  netif_oper_up(ndev) ?
					IB_PORT_ACTIVE : IB_PORT_DOWN;
	props->phys_state	= state_to_phys_state(props->state);

out:
	spin_unlock(&iboe->lock);
	return 0;
}

static int mlx4_ib_query_port(struct ib_device *ibdev, u8 port,
			      struct ib_port_attr *props)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof *in_mad, GFP_KERNEL);
	out_mad = kmalloc(sizeof *out_mad, GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	memset(props, 0, sizeof *props);

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_PORT_INFO;
	in_mad->attr_mod = cpu_to_be32(port);

	err = mlx4_MAD_IFC(to_mdev(ibdev), 1, 1, port, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	mlx4_ib_port_link_layer(ibdev, port) == IB_LINK_LAYER_INFINIBAND ?
		ib_link_query_port(ibdev, port, props, out_mad) :
		eth_link_query_port(ibdev, port, props, out_mad);

out:
	kfree(in_mad);
	kfree(out_mad);

	return err;
}

static int __mlx4_ib_query_gid(struct ib_device *ibdev, u8 port, int index,
			       union ib_gid *gid)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof *in_mad, GFP_KERNEL);
	out_mad = kmalloc(sizeof *out_mad, GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_PORT_INFO;
	in_mad->attr_mod = cpu_to_be32(port);

	err = mlx4_MAD_IFC(to_mdev(ibdev), 1, 1, port, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memcpy(gid->raw, out_mad->data + 8, 8);

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_GUID_INFO;
	in_mad->attr_mod = cpu_to_be32(index / 8);

	err = mlx4_MAD_IFC(to_mdev(ibdev), 1, 1, port, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memcpy(gid->raw + 8, out_mad->data + (index % 8) * 8, 8);

out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

static int iboe_query_gid(struct ib_device *ibdev, u8 port, int index,
			    union ib_gid *gid)
{
	struct mlx4_ib_dev *dev = to_mdev(ibdev);

	*gid = dev->iboe.gid_table[port - 1][index];

	return 0;
}

static int mlx4_ib_query_gid(struct ib_device *ibdev, u8 port, int index,
			     union ib_gid *gid)
{
	if (rdma_port_get_link_layer(ibdev, port) == IB_LINK_LAYER_INFINIBAND)
		return __mlx4_ib_query_gid(ibdev, port, index, gid);
	else
		return iboe_query_gid(ibdev, port, index, gid);
}

static int mlx4_ib_query_pkey(struct ib_device *ibdev, u8 port, u16 index,
			      u16 *pkey)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof *in_mad, GFP_KERNEL);
	out_mad = kmalloc(sizeof *out_mad, GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_PKEY_TABLE;
	in_mad->attr_mod = cpu_to_be32(index / 32);

	err = mlx4_MAD_IFC(to_mdev(ibdev), 1, 1, port, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	*pkey = be16_to_cpu(((__be16 *) out_mad->data)[index % 32]);

out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

static int mlx4_ib_modify_device(struct ib_device *ibdev, int mask,
				 struct ib_device_modify *props)
{
	struct mlx4_cmd_mailbox *mailbox;
	int err;

	if (mask & ~IB_DEVICE_MODIFY_NODE_DESC)
		return -EOPNOTSUPP;

	if (!(mask & IB_DEVICE_MODIFY_NODE_DESC))
		return 0;

	spin_lock(&to_mdev(ibdev)->sm_lock);
	memcpy(ibdev->node_desc, props->node_desc, 64);
	spin_unlock(&to_mdev(ibdev)->sm_lock);

	/* if possible, pass node desc to FW, so it can generate
	 * a 144 trap. If cmd fails, just ignore.
	 */
	mailbox = mlx4_alloc_cmd_mailbox(to_mdev(ibdev)->dev);
	if (IS_ERR(mailbox))
		return 0;

	memset(mailbox->buf, 0, 256);
	memcpy(mailbox->buf, props->node_desc, 64);
	err = mlx4_cmd(to_mdev(ibdev)->dev, mailbox->dma, 1, 0,
		       MLX4_CMD_SET_NODE, MLX4_CMD_TIME_CLASS_A);
	if (err)
		mlx4_ib_dbg("SET_NODE command failed (%d)", err);

	mlx4_free_cmd_mailbox(to_mdev(ibdev)->dev, mailbox);

	return 0;
}

static int mlx4_SET_PORT(struct mlx4_ib_dev *dev, u8 port, int reset_qkey_viols,
			 u32 cap_mask)
{
	struct mlx4_cmd_mailbox *mailbox;
	int err;
	u8 is_eth = dev->dev->caps.port_type[port] == MLX4_PORT_TYPE_ETH;

	mailbox = mlx4_alloc_cmd_mailbox(dev->dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);

	memset(mailbox->buf, 0, 256);

	if (dev->dev->flags & MLX4_FLAG_OLD_PORT_CMDS) {
		*(u8 *) mailbox->buf	     = !!reset_qkey_viols << 6;
		((__be32 *) mailbox->buf)[2] = cpu_to_be32(cap_mask);
	} else {
		((u8 *) mailbox->buf)[3]     = !!reset_qkey_viols;
		((__be32 *) mailbox->buf)[1] = cpu_to_be32(cap_mask);
	}

	err = mlx4_cmd(dev->dev, mailbox->dma, port, is_eth, MLX4_CMD_SET_PORT,
		       MLX4_CMD_TIME_CLASS_B);

	mlx4_free_cmd_mailbox(dev->dev, mailbox);
	return err;
}

static int mlx4_ib_modify_port(struct ib_device *ibdev, u8 port, int mask,
			       struct ib_port_modify *props)
{
	struct ib_port_attr attr;
	u32 cap_mask;
	int err;

	mutex_lock(&to_mdev(ibdev)->cap_mask_mutex);

	err = mlx4_ib_query_port(ibdev, port, &attr);
	if (err)
		goto out;

	cap_mask = (attr.port_cap_flags | props->set_port_cap_mask) &
		~props->clr_port_cap_mask;

	err = mlx4_SET_PORT(to_mdev(ibdev), port,
			    !!(mask & IB_PORT_RESET_QKEY_CNTR),
			    cap_mask);

out:
	mutex_unlock(&to_mdev(ibdev)->cap_mask_mutex);
	return err;
}

static struct ib_ucontext *mlx4_ib_alloc_ucontext(struct ib_device *ibdev,
						  struct ib_udata *udata)
{
	struct mlx4_ib_dev *dev = to_mdev(ibdev);
	struct mlx4_ib_ucontext *context;
	struct mlx4_ib_alloc_ucontext_resp resp;
	int err;

	if (!dev->ib_active)
		return ERR_PTR(-EAGAIN);

	resp.qp_tab_size      = dev->dev->caps.num_qps;

	if (mlx4_wc_enabled()) {
		resp.bf_reg_size      = dev->dev->caps.bf_reg_size;
		resp.bf_regs_per_page = dev->dev->caps.bf_regs_per_page;
	} else {
		resp.bf_reg_size      = 0;
		resp.bf_regs_per_page = 0;
	}

	context = kzalloc(sizeof *context, GFP_KERNEL);
	if (!context)
		return ERR_PTR(-ENOMEM);

	err = mlx4_uar_alloc(to_mdev(ibdev)->dev, &context->uar);
	if (err) {
		kfree(context);
		return ERR_PTR(err);
	}

	INIT_LIST_HEAD(&context->db_page_list);
	mutex_init(&context->db_page_mutex);

	err = ib_copy_to_udata(udata, &resp, sizeof resp);
	if (err) {
		mlx4_uar_free(to_mdev(ibdev)->dev, &context->uar);
		kfree(context);
		return ERR_PTR(-EFAULT);
	}

	return &context->ibucontext;
}

static int mlx4_ib_dealloc_ucontext(struct ib_ucontext *ibcontext)
{
	struct mlx4_ib_ucontext *context = to_mucontext(ibcontext);

	mlx4_uar_free(to_mdev(ibcontext->device)->dev, &context->uar);
	kfree(context);

	return 0;
}

static int mlx4_ib_mmap(struct ib_ucontext *context, struct vm_area_struct *vma)
{
	struct mlx4_ib_dev *dev = to_mdev(context->device);

	if (vma->vm_end - vma->vm_start != PAGE_SIZE)
		return -EINVAL;

	if (vma->vm_pgoff == 0) {
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

		if (io_remap_pfn_range(vma, vma->vm_start,
				       to_mucontext(context)->uar.pfn,
				       PAGE_SIZE, vma->vm_page_prot))
			return -EAGAIN;
	} else if (vma->vm_pgoff == 1 && dev->dev->caps.bf_reg_size != 0) {
		vma->vm_page_prot = pgprot_wc(vma->vm_page_prot);

		if (io_remap_pfn_range(vma, vma->vm_start,
				       to_mucontext(context)->uar.pfn +
				       dev->dev->caps.num_uars,
				       PAGE_SIZE, vma->vm_page_prot))
			return -EAGAIN;
	} else
		return -EINVAL;

	return 0;
}

static struct ib_pd *mlx4_ib_alloc_pd(struct ib_device *ibdev,
				      struct ib_ucontext *context,
				      struct ib_udata *udata)
{
	struct mlx4_ib_pd *pd;
	int err;

	pd = kzalloc(sizeof *pd, GFP_KERNEL);
	if (!pd)
		return ERR_PTR(-ENOMEM);

	err = mlx4_pd_alloc(to_mdev(ibdev)->dev, &pd->pdn);
	if (err) {
		kfree(pd);
		return ERR_PTR(err);
	}

	if (context)
		if (ib_copy_to_udata(udata, &pd->pdn, sizeof (__u32))) {
			mlx4_pd_free(to_mdev(ibdev)->dev, pd->pdn);
			kfree(pd);
			return ERR_PTR(-EFAULT);
		}

	return &pd->ibpd;
}

static int mlx4_ib_dealloc_pd(struct ib_pd *pd)
{
	mlx4_pd_free(to_mdev(pd->device)->dev, to_mpd(pd)->pdn);
	kfree(pd);

	return 0;
}

static int add_gid_entry(struct ib_qp *ibqp, union ib_gid *gid)
{
	struct mlx4_ib_qp *mqp = to_mqp(ibqp);
	struct mlx4_ib_dev *mdev = to_mdev(ibqp->device);
	struct gid_entry *ge;

	ge = kzalloc(sizeof *ge, GFP_KERNEL);
	if (!ge)
		return -ENOMEM;

	ge->gid = *gid;
	if (mlx4_ib_add_mc(mdev, mqp, gid)) {
		ge->port = mqp->port;
		ge->added = 1;
	}

	mutex_lock(&mqp->mutex);
	list_add_tail(&ge->list, &mqp->gid_list);
	mutex_unlock(&mqp->mutex);

	return 0;
}

int mlx4_ib_add_mc(struct mlx4_ib_dev *mdev, struct mlx4_ib_qp *mqp,
		   union ib_gid *gid)
{
	u8 mac[6];
	struct net_device *ndev;
	int ret = 0;

	if (!mqp->port)
		return 0;
	spin_lock(&mdev->iboe.lock);
	ndev = mdev->iboe.netdevs[mqp->port - 1];
	if (ndev)
		dev_hold(ndev);
	spin_unlock(&mdev->iboe.lock);
	if (ndev) {
		rdma_get_mcast_mac((struct in6_addr *)gid, mac);
		rtnl_lock();
		dev_mc_add(mdev->iboe.netdevs[mqp->port - 1], mac, 6, 0);
		ret = 1;
		rtnl_unlock();
		dev_put(ndev);
	}

	return ret;
}

static int mlx4_ib_mcg_attach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid)
{
	int err;
	struct mlx4_ib_dev *mdev = to_mdev(ibqp->device);
	struct mlx4_ib_qp *mqp = to_mqp(ibqp);

	err = mlx4_multicast_attach(mdev->dev, &mqp->mqp, gid->raw, !!(mqp->flags &
				MLX4_IB_QP_BLOCK_MULTICAST_LOOPBACK),
				(ibqp->qp_type == IB_QPT_RAW_ETH) ?
				MLX4_MCAST_PROT_EN : MLX4_MCAST_PROT_IB);
	if (err)
		return err;

	err = add_gid_entry(ibqp, gid);
	if (err)
		goto err_add;

	return 0;

err_add:
	mlx4_multicast_detach(mdev->dev, &mqp->mqp, gid->raw,
				(ibqp->qp_type == IB_QPT_RAW_ETH) ?
				MLX4_MCAST_PROT_EN : MLX4_MCAST_PROT_IB);
	return err;
}

static struct gid_entry *find_gid_entry(struct mlx4_ib_qp *qp, u8 *raw)
{
	struct gid_entry *ge;
	struct gid_entry *tmp;
	struct gid_entry *ret = NULL;

	list_for_each_entry_safe(ge, tmp, &qp->gid_list, list) {
		if (!memcmp(raw, ge->gid.raw, 16)) {
			ret = ge;
			break;
		}
	}

	return ret;
}

static int mlx4_ib_mcg_detach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid)
{
	int err;
	struct mlx4_ib_dev *mdev = to_mdev(ibqp->device);
	struct mlx4_ib_qp *mqp = to_mqp(ibqp);
	u8 mac[6];
	struct net_device *ndev;
	struct gid_entry *ge;

	err = mlx4_multicast_detach(mdev->dev, &mqp->mqp, gid->raw,
				(ibqp->qp_type == IB_QPT_RAW_ETH) ?
				MLX4_MCAST_PROT_EN : MLX4_MCAST_PROT_IB);
	if (err)
		return err;

	mutex_lock(&mqp->mutex);
	ge = find_gid_entry(mqp, gid->raw);
	if (ge) {
		spin_lock(&mdev->iboe.lock);
		ndev = ge->added ? mdev->iboe.netdevs[ge->port - 1] : NULL;
		if (ndev)
			dev_hold(ndev);
		spin_unlock(&mdev->iboe.lock);
		rdma_get_mcast_mac((struct in6_addr *)gid, mac);
		if (ndev) {
			rtnl_lock();
			dev_mc_delete(mdev->iboe.netdevs[ge->port - 1], mac, 6, 0);
			rtnl_unlock();
			dev_put(ndev);
		}
		list_del(&ge->list);
		kfree(ge);
	} else
		printk(KERN_WARNING "could not find mgid entry\n");

	mutex_unlock(&mqp->mutex);

	return 0;
}

static void mlx4_dummy_comp_handler(struct ib_cq *cq, void *cq_context)
{
}

static struct ib_xrcd *mlx4_ib_alloc_xrcd(struct ib_device *ibdev,
					  struct ib_ucontext *context,
					  struct ib_udata *udata)
{
	struct mlx4_ib_xrcd *xrcd;
	struct mlx4_ib_dev *mdev = to_mdev(ibdev);
	struct ib_pd *pd;
	struct ib_cq *cq;
	int err;

	if (!(mdev->dev->caps.flags & MLX4_DEV_CAP_FLAG_XRC))
		return ERR_PTR(-ENOSYS);

	xrcd = kmalloc(sizeof *xrcd, GFP_KERNEL);
	if (!xrcd)
		return ERR_PTR(-ENOMEM);

	err = mlx4_xrcd_alloc(mdev->dev, &xrcd->xrcdn);
	if (err)
		goto err_xrcd;

	pd = mlx4_ib_alloc_pd(ibdev, NULL, NULL);
	if (IS_ERR(pd)) {
		err = PTR_ERR(pd);
		goto err_pd;
	}
	pd->device  = ibdev;

	cq = mlx4_ib_create_cq(ibdev, 1, 0, NULL, NULL);
	if (IS_ERR(cq)) {
		err = PTR_ERR(cq);
		goto err_cq;
	}
	cq->device        = ibdev;
	cq->comp_handler  = mlx4_dummy_comp_handler;

	if (context)
		if (ib_copy_to_udata(udata, &xrcd->xrcdn, sizeof(__u32))) {
			err = -EFAULT;
			goto err_copy;
		}

	xrcd->cq = cq;
	xrcd->pd = pd;
	return &xrcd->ibxrcd;

err_copy:
	mlx4_ib_destroy_cq(cq);
err_cq:
	mlx4_ib_dealloc_pd(pd);
err_pd:
	mlx4_xrcd_free(mdev->dev, xrcd->xrcdn);
err_xrcd:
	kfree(xrcd);
	return ERR_PTR(err);
}

static int mlx4_ib_dealloc_xrcd(struct ib_xrcd *xrcd)
{
	struct mlx4_ib_xrcd *mxrcd = to_mxrcd(xrcd);

	mlx4_ib_destroy_cq(mxrcd->cq);
	mlx4_ib_dealloc_pd(mxrcd->pd);
	mlx4_xrcd_free(to_mdev(xrcd->device)->dev, to_mxrcd(xrcd)->xrcdn);
	kfree(xrcd);

	return 0;
}


static int init_node_data(struct mlx4_ib_dev *dev)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof *in_mad, GFP_KERNEL);
	out_mad = kmalloc(sizeof *out_mad, GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id = IB_SMP_ATTR_NODE_DESC;

	err = mlx4_MAD_IFC(dev, 1, 1, 1, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memcpy(dev->ib_dev.node_desc, out_mad->data, 64);

	in_mad->attr_id = IB_SMP_ATTR_NODE_INFO;

	err = mlx4_MAD_IFC(dev, 1, 1, 1, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	dev->dev->rev_id = be32_to_cpup((__be32 *) (out_mad->data + 32));
	memcpy(&dev->ib_dev.node_guid, out_mad->data + 12, 8);

out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

static ssize_t show_hca(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct mlx4_ib_dev *dev =
		container_of(device, struct mlx4_ib_dev, ib_dev.dev);
	return sprintf(buf, "MT%d\n", dev->dev->pdev->device);
}

static ssize_t show_fw_ver(struct device *device, struct device_attribute *attr,
			   char *buf)
{
	struct mlx4_ib_dev *dev =
		container_of(device, struct mlx4_ib_dev, ib_dev.dev);
	return sprintf(buf, "%d.%d.%d\n", (int) (dev->dev->caps.fw_ver >> 32),
		       (int) (dev->dev->caps.fw_ver >> 16) & 0xffff,
		       (int) dev->dev->caps.fw_ver & 0xffff);
}

static ssize_t show_rev(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct mlx4_ib_dev *dev =
		container_of(device, struct mlx4_ib_dev, ib_dev.dev);
	return sprintf(buf, "%x\n", dev->dev->rev_id);
}

static ssize_t show_board(struct device *device, struct device_attribute *attr,
			  char *buf)
{
	struct mlx4_ib_dev *dev =
		container_of(device, struct mlx4_ib_dev, ib_dev.dev);
	return sprintf(buf, "%.*s\n", MLX4_BOARD_ID_LEN,
		       dev->dev->board_id);
}

static DEVICE_ATTR(hw_rev,   S_IRUGO, show_rev,    NULL);
static DEVICE_ATTR(fw_ver,   S_IRUGO, show_fw_ver, NULL);
static DEVICE_ATTR(hca_type, S_IRUGO, show_hca,    NULL);
static DEVICE_ATTR(board_id, S_IRUGO, show_board,  NULL);

static struct device_attribute *mlx4_class_attributes[] = {
	&dev_attr_hw_rev,
	&dev_attr_fw_ver,
	&dev_attr_hca_type,
	&dev_attr_board_id
};

/*
 * create show function and a device_attribute struct pointing to
 * the function for _name
 */
#define DEVICE_DIAG_RPRT_ATTR(_name, _offset, _op_mod)		\
static ssize_t show_rprt_##_name(struct device *dev,		\
				 struct device_attribute *attr,	\
				 char *buf){			\
	return show_diag_rprt(dev, buf, _offset, _op_mod);	\
}								\
static DEVICE_ATTR(_name, S_IRUGO, show_rprt_##_name, NULL);

#define MLX4_DIAG_RPRT_CLEAR_DIAGS 3

static size_t show_diag_rprt(struct device *device, char *buf,
                              u32 offset, u8 op_modifier)
{
	size_t ret;
	u32 counter_offset = offset;
	u32 diag_counter = 0;
	struct mlx4_ib_dev *dev = container_of(device, struct mlx4_ib_dev,
					       ib_dev.dev);

	ret = mlx4_query_diag_counters(dev->dev, 1, op_modifier,
				       &counter_offset, &diag_counter);
	if (ret)
		return ret;

	return sprintf(buf,"%d\n", diag_counter);
}

static ssize_t clear_diag_counters(struct device *device,
				   struct device_attribute *attr,
				   const char *buf, size_t length)
{
	size_t ret;
	struct mlx4_ib_dev *dev = container_of(device, struct mlx4_ib_dev,
					       ib_dev.dev);

	ret = mlx4_query_diag_counters(dev->dev, 0, MLX4_DIAG_RPRT_CLEAR_DIAGS,
				       NULL, NULL);
	if (ret)
		return ret;

	return length;
}

DEVICE_DIAG_RPRT_ATTR(rq_num_lle	, 0x00, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_lle	, 0x04, 2);
DEVICE_DIAG_RPRT_ATTR(rq_num_lqpoe	, 0x08, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_lqpoe 	, 0x0C, 2);
DEVICE_DIAG_RPRT_ATTR(rq_num_leeoe	, 0x10, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_leeoe	, 0x14, 2);
DEVICE_DIAG_RPRT_ATTR(rq_num_lpe	, 0x18, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_lpe	, 0x1C, 2);
DEVICE_DIAG_RPRT_ATTR(rq_num_wrfe	, 0x20, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_wrfe	, 0x24, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_mwbe	, 0x2C, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_bre	, 0x34, 2);
DEVICE_DIAG_RPRT_ATTR(rq_num_lae	, 0x38, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_rire	, 0x44, 2);
DEVICE_DIAG_RPRT_ATTR(rq_num_rire	, 0x48, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_rae	, 0x4C, 2);
DEVICE_DIAG_RPRT_ATTR(rq_num_rae	, 0x50, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_roe	, 0x54, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_tree	, 0x5C, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_rree	, 0x64, 2);
DEVICE_DIAG_RPRT_ATTR(rq_num_rnr	, 0x68, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_rnr	, 0x6C, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_rabrte	, 0x7C, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_ieecne	, 0x84, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_ieecse	, 0x8C, 2);
DEVICE_DIAG_RPRT_ATTR(rq_num_oos	, 0x100, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_oos	, 0x104, 2);
DEVICE_DIAG_RPRT_ATTR(rq_num_mce	, 0x108, 2);
DEVICE_DIAG_RPRT_ATTR(rq_num_rsync	, 0x110, 2);
DEVICE_DIAG_RPRT_ATTR(sq_num_rsync	, 0x114, 2);
DEVICE_DIAG_RPRT_ATTR(rq_num_udsdprd	, 0x118, 2);
DEVICE_DIAG_RPRT_ATTR(rq_num_ucsdprd	, 0x120, 2);
DEVICE_DIAG_RPRT_ATTR(num_cqovf		, 0x1A0, 2);
DEVICE_DIAG_RPRT_ATTR(num_eqovf		, 0x1A4, 2);
DEVICE_DIAG_RPRT_ATTR(num_baddb		, 0x1A8, 2);

static DEVICE_ATTR(clear_diag, S_IWUGO, NULL, clear_diag_counters);

static struct attribute *diag_rprt_attrs[] = {
	&dev_attr_rq_num_lle.attr,
	&dev_attr_sq_num_lle.attr,
	&dev_attr_rq_num_lqpoe.attr,
	&dev_attr_sq_num_lqpoe.attr,
	&dev_attr_rq_num_leeoe.attr,
	&dev_attr_sq_num_leeoe.attr,
	&dev_attr_rq_num_lpe.attr,
	&dev_attr_sq_num_lpe.attr,
	&dev_attr_rq_num_wrfe.attr,
	&dev_attr_sq_num_wrfe.attr,
	&dev_attr_sq_num_mwbe.attr,
	&dev_attr_sq_num_bre.attr,
	&dev_attr_rq_num_lae.attr,
	&dev_attr_sq_num_rire.attr,
	&dev_attr_rq_num_rire.attr,
	&dev_attr_sq_num_rae.attr,
	&dev_attr_rq_num_rae.attr,
	&dev_attr_sq_num_roe.attr,
	&dev_attr_sq_num_tree.attr,
	&dev_attr_sq_num_rree.attr,
	&dev_attr_rq_num_rnr.attr,
	&dev_attr_sq_num_rnr.attr,
	&dev_attr_sq_num_rabrte.attr,
	&dev_attr_sq_num_ieecne.attr,
	&dev_attr_sq_num_ieecse.attr,
	&dev_attr_rq_num_oos.attr,
	&dev_attr_sq_num_oos.attr,
	&dev_attr_rq_num_mce.attr,
	&dev_attr_rq_num_rsync.attr,
	&dev_attr_sq_num_rsync.attr,
	&dev_attr_rq_num_udsdprd.attr,
	&dev_attr_rq_num_ucsdprd.attr,
	&dev_attr_num_cqovf.attr,
	&dev_attr_num_eqovf.attr,
	&dev_attr_num_baddb.attr,
	&dev_attr_clear_diag.attr,
	NULL
};

struct attribute_group diag_counters_group = {
	.name  = "diag_counters",
	.attrs  = diag_rprt_attrs
};

static void mlx4_addrconf_ifid_eui48(u8 *eui, u16 vlan_id, struct net_device *dev)
{
#ifdef __linux__
	memcpy(eui, dev->dev_addr, 3);
	memcpy(eui + 5, dev->dev_addr + 3, 3);
#else
	memcpy(eui, IF_LLADDR(dev), 3);
	memcpy(eui + 5, IF_LLADDR(dev) + 3, 3);
#endif
	if (vlan_id < 0x1000) {
		eui[3] = vlan_id >> 8;
		eui[4] = vlan_id & 0xff;
	} else {
		eui[3] = 0xff;
		eui[4] = 0xfe;
	}
	eui[0] ^= 2;
}

static void update_gids_task(struct work_struct *work)
{
	struct update_gid_work *gw = container_of(work, struct update_gid_work, work);
	struct mlx4_cmd_mailbox *mailbox;
	union ib_gid *gids;
	int err;
	struct mlx4_dev	*dev = gw->dev->dev;
	struct ib_event event;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox)) {
		printk(KERN_WARNING "update gid table failed %ld\n", PTR_ERR(mailbox));
		return;
	}

	gids = mailbox->buf;
	memcpy(gids, gw->gids, sizeof gw->gids);

	err = mlx4_cmd(dev, mailbox->dma, MLX4_SET_PORT_GID_TABLE << 8 | gw->port,
		       1, MLX4_CMD_SET_PORT, MLX4_CMD_TIME_CLASS_B);
	if (err)
		printk(KERN_WARNING "set port command failed\n");
	else {
		memcpy(gw->dev->iboe.gid_table[gw->port - 1], gw->gids, sizeof gw->gids);
		event.device = &gw->dev->ib_dev;
		event.element.port_num = gw->port;
		event.event    = IB_EVENT_GID_CHANGE;
		ib_dispatch_event(&event);
	}

	mlx4_free_cmd_mailbox(dev, mailbox);
	kfree(gw);
}

enum {
	MLX4_MAX_EFF_VLANS = 128 - MLX4_VLAN_REGULAR,
};

static int update_ipv6_gids(struct mlx4_ib_dev *dev, int port, int clear)
{
	struct net_device *ndev = dev->iboe.netdevs[port - 1];
	struct update_gid_work *work;
	struct net_device *tmp;
	int i;
	u8 *hits;
	int ret;
	union ib_gid gid;
	int tofree;
	int found;
	int need_update = 0;
	u16 vid;

	work = kzalloc(sizeof *work, GFP_ATOMIC);
	if (!work)
		return -ENOMEM;

	hits = kzalloc(MLX4_MAX_EFF_VLANS + 1, GFP_ATOMIC);
	if (!hits) {
		ret = -ENOMEM;
		goto out;
	}

#ifdef __linux__
	read_lock(&dev_base_lock);
	for_each_netdev(&init_net, tmp) {
#else
	IFNET_RLOCK();
	TAILQ_FOREACH(tmp, &V_ifnet, if_link) {
#endif
		if (ndev && (tmp == ndev || rdma_vlan_dev_real_dev(tmp) == ndev)) {
			gid.global.subnet_prefix = cpu_to_be64(0xfe80000000000000LL);
			vid = rdma_vlan_dev_vlan_id(tmp);
			mlx4_addrconf_ifid_eui48(&gid.raw[8], vid, ndev);
			found = 0;
			tofree = -1;
			for (i = 0; i < MLX4_MAX_EFF_VLANS + 1; ++i) {
				if (tofree < 0 &&
				    !memcmp(&dev->iboe.gid_table[port - 1][i], &zgid, sizeof zgid))
					tofree = i;
				if (!memcmp(&dev->iboe.gid_table[port - 1][i], &gid, sizeof gid)) {
					hits[i] = 1;
					found = 1;
					break;
				}
			}

			if (!found) {
				if (tmp == ndev  && (memcmp(&dev->iboe.gid_table[port - 1][0], &gid, sizeof gid) || !memcmp(&dev->iboe.gid_table[port - 1][0], &zgid, sizeof gid))) {
					dev->iboe.gid_table[port - 1][0] = gid;
					++need_update;
					hits[0] = 1;
				} else if (tofree >= 0) {
					dev->iboe.gid_table[port - 1][tofree] = gid;
					hits[tofree] = 1;
					++need_update;
				}
			}
		}
#ifdef __linux__
	}
	read_unlock(&dev_base_lock);
#else
	}
	IFNET_RUNLOCK();
#endif

	for (i = 0; i < MLX4_MAX_EFF_VLANS + 1; ++i)
		if (!hits[i]) {
			if (memcmp(&dev->iboe.gid_table[port - 1][i], &zgid, sizeof zgid))
				++need_update;
			dev->iboe.gid_table[port - 1][i] = zgid;
		}


	if (need_update) {
		memcpy(work->gids, dev->iboe.gid_table[port - 1], sizeof work->gids);
		INIT_WORK(&work->work, update_gids_task);
		work->port = port;
		work->dev = dev;
		queue_work(wq, &work->work);
	} else
		kfree(work);

	kfree(hits);
	return 0;

out:
	kfree(work);
	return ret;
}

static void handle_en_event(struct mlx4_ib_dev *dev, int port, unsigned long event)
{
	switch (event) {
	case NETDEV_UP:
#ifdef __linux__
	case NETDEV_CHANGEADDR:
#endif
		update_ipv6_gids(dev, port, 0);
		break;

	case NETDEV_DOWN:
		update_ipv6_gids(dev, port, 1);
		dev->iboe.netdevs[port - 1] = NULL;
	}
}

static void netdev_added(struct mlx4_ib_dev *dev, int port)
{
	update_ipv6_gids(dev, port, 0);
}

static void netdev_removed(struct mlx4_ib_dev *dev, int port)
{
	update_ipv6_gids(dev, port, 1);
}

static int mlx4_ib_netdev_event(struct notifier_block *this, unsigned long event,
				void *ptr)
{
	struct net_device *dev = ptr;
	struct mlx4_ib_dev *ibdev;
	struct net_device *oldnd;
	struct mlx4_ib_iboe *iboe;
	int port;

#ifdef __linux__
	if (!net_eq(dev_net(dev), &init_net))
		return NOTIFY_DONE;
#endif

	ibdev = container_of(this, struct mlx4_ib_dev, iboe.nb);
	iboe = &ibdev->iboe;

	spin_lock(&iboe->lock);
	mlx4_foreach_ib_transport_port(port, ibdev->dev) {
		oldnd = iboe->netdevs[port - 1];
		iboe->netdevs[port - 1] = mlx4_get_prot_dev(ibdev->dev, MLX4_PROT_EN, port);
		if (oldnd != iboe->netdevs[port - 1]) {
			if (iboe->netdevs[port - 1])
				netdev_added(ibdev, port);
			else
				netdev_removed(ibdev, port);
		}
	}

	if (dev == iboe->netdevs[0] ||
	    (iboe->netdevs[0] && rdma_vlan_dev_real_dev(dev) == iboe->netdevs[0]))
		handle_en_event(ibdev, 1, event);
	else if (dev == iboe->netdevs[1]
		 || (iboe->netdevs[1] && rdma_vlan_dev_real_dev(dev) == iboe->netdevs[1]))
		handle_en_event(ibdev, 2, event);

	spin_unlock(&iboe->lock);

	return NOTIFY_DONE;
}

static void *mlx4_ib_add(struct mlx4_dev *dev)
{
	static int mlx4_ib_version_printed;
	struct mlx4_ib_dev *ibdev;
	int num_ports = 0;
	int i;
	int err;
	struct mlx4_ib_iboe *iboe;
	int k;

	if (!mlx4_ib_version_printed) {
		printk(KERN_INFO "%s", mlx4_ib_version);
		++mlx4_ib_version_printed;
	}

	mlx4_foreach_ib_transport_port(i, dev)
		num_ports++;

	/* No point in registering a device with no ports... */
	if (num_ports == 0)
		return NULL;

	ibdev = (struct mlx4_ib_dev *) ib_alloc_device(sizeof *ibdev);
	if (!ibdev) {
		dev_err(&dev->pdev->dev, "Device struct alloc failed\n");
		return NULL;
	}

	iboe = &ibdev->iboe;

	if (mlx4_pd_alloc(dev, &ibdev->priv_pdn))
		goto err_dealloc;

	if (mlx4_uar_alloc(dev, &ibdev->priv_uar))
		goto err_pd;

	ibdev->priv_uar.map = ioremap(ibdev->priv_uar.pfn << PAGE_SHIFT, PAGE_SIZE);
	if (!ibdev->priv_uar.map)
		goto err_uar;
	MLX4_INIT_DOORBELL_LOCK(&ibdev->uar_lock);

	ibdev->dev = dev;

	strlcpy(ibdev->ib_dev.name, "mlx4_%d", IB_DEVICE_NAME_MAX);
	ibdev->ib_dev.owner		= THIS_MODULE;
	ibdev->ib_dev.node_type		= RDMA_NODE_IB_CA;
	ibdev->ib_dev.local_dma_lkey	= dev->caps.reserved_lkey;
	ibdev->num_ports		= num_ports;
	ibdev->ib_dev.phys_port_cnt     = ibdev->num_ports;
	ibdev->ib_dev.num_comp_vectors	= dev->caps.num_comp_vectors;
	ibdev->ib_dev.dma_device	= &dev->pdev->dev;

	ibdev->ib_dev.uverbs_abi_ver	= MLX4_IB_UVERBS_ABI_VERSION;
	ibdev->ib_dev.uverbs_cmd_mask	=
		(1ull << IB_USER_VERBS_CMD_GET_CONTEXT)		|
		(1ull << IB_USER_VERBS_CMD_QUERY_DEVICE)	|
		(1ull << IB_USER_VERBS_CMD_QUERY_PORT)		|
		(1ull << IB_USER_VERBS_CMD_ALLOC_PD)		|
		(1ull << IB_USER_VERBS_CMD_DEALLOC_PD)		|
		(1ull << IB_USER_VERBS_CMD_REG_MR)		|
		(1ull << IB_USER_VERBS_CMD_DEREG_MR)		|
		(1ull << IB_USER_VERBS_CMD_CREATE_COMP_CHANNEL)	|
		(1ull << IB_USER_VERBS_CMD_CREATE_CQ)		|
		(1ull << IB_USER_VERBS_CMD_RESIZE_CQ)		|
		(1ull << IB_USER_VERBS_CMD_DESTROY_CQ)		|
		(1ull << IB_USER_VERBS_CMD_CREATE_QP)		|
		(1ull << IB_USER_VERBS_CMD_MODIFY_QP)		|
		(1ull << IB_USER_VERBS_CMD_QUERY_QP)		|
		(1ull << IB_USER_VERBS_CMD_DESTROY_QP)		|
		(1ull << IB_USER_VERBS_CMD_ATTACH_MCAST)	|
		(1ull << IB_USER_VERBS_CMD_DETACH_MCAST)	|
		(1ull << IB_USER_VERBS_CMD_CREATE_SRQ)		|
		(1ull << IB_USER_VERBS_CMD_MODIFY_SRQ)		|
		(1ull << IB_USER_VERBS_CMD_QUERY_SRQ)		|
		(1ull << IB_USER_VERBS_CMD_DESTROY_SRQ);

	ibdev->ib_dev.query_device	= mlx4_ib_query_device;
	ibdev->ib_dev.query_port	= mlx4_ib_query_port;
	ibdev->ib_dev.get_link_layer	= mlx4_ib_port_link_layer;
	ibdev->ib_dev.query_gid		= mlx4_ib_query_gid;
	ibdev->ib_dev.query_pkey	= mlx4_ib_query_pkey;
	ibdev->ib_dev.modify_device	= mlx4_ib_modify_device;
	ibdev->ib_dev.modify_port	= mlx4_ib_modify_port;
	ibdev->ib_dev.alloc_ucontext	= mlx4_ib_alloc_ucontext;
	ibdev->ib_dev.dealloc_ucontext	= mlx4_ib_dealloc_ucontext;
	ibdev->ib_dev.mmap		= mlx4_ib_mmap;
	ibdev->ib_dev.alloc_pd		= mlx4_ib_alloc_pd;
	ibdev->ib_dev.dealloc_pd	= mlx4_ib_dealloc_pd;
	ibdev->ib_dev.create_ah		= mlx4_ib_create_ah;
	ibdev->ib_dev.query_ah		= mlx4_ib_query_ah;
	ibdev->ib_dev.destroy_ah	= mlx4_ib_destroy_ah;
	ibdev->ib_dev.create_srq	= mlx4_ib_create_srq;
	ibdev->ib_dev.modify_srq	= mlx4_ib_modify_srq;
	ibdev->ib_dev.query_srq		= mlx4_ib_query_srq;
	ibdev->ib_dev.destroy_srq	= mlx4_ib_destroy_srq;
	ibdev->ib_dev.post_srq_recv	= mlx4_ib_post_srq_recv;
	ibdev->ib_dev.create_qp		= mlx4_ib_create_qp;
	ibdev->ib_dev.modify_qp		= mlx4_ib_modify_qp;
	ibdev->ib_dev.query_qp		= mlx4_ib_query_qp;
	ibdev->ib_dev.destroy_qp	= mlx4_ib_destroy_qp;
	ibdev->ib_dev.post_send		= mlx4_ib_post_send;
	ibdev->ib_dev.post_recv		= mlx4_ib_post_recv;
	ibdev->ib_dev.create_cq		= mlx4_ib_create_cq;
	ibdev->ib_dev.modify_cq		= mlx4_ib_modify_cq;
	ibdev->ib_dev.resize_cq		= mlx4_ib_resize_cq;
	ibdev->ib_dev.destroy_cq	= mlx4_ib_destroy_cq;
	ibdev->ib_dev.poll_cq		= mlx4_ib_poll_cq;
	ibdev->ib_dev.req_notify_cq	= mlx4_ib_arm_cq;
	ibdev->ib_dev.get_dma_mr	= mlx4_ib_get_dma_mr;
	ibdev->ib_dev.reg_user_mr	= mlx4_ib_reg_user_mr;
	ibdev->ib_dev.dereg_mr		= mlx4_ib_dereg_mr;
	ibdev->ib_dev.alloc_fast_reg_mr = mlx4_ib_alloc_fast_reg_mr;
	ibdev->ib_dev.alloc_fast_reg_page_list = mlx4_ib_alloc_fast_reg_page_list;
	ibdev->ib_dev.free_fast_reg_page_list  = mlx4_ib_free_fast_reg_page_list;
	ibdev->ib_dev.attach_mcast	= mlx4_ib_mcg_attach;
	ibdev->ib_dev.detach_mcast	= mlx4_ib_mcg_detach;
	ibdev->ib_dev.process_mad	= mlx4_ib_process_mad;

	ibdev->ib_dev.alloc_fmr		= mlx4_ib_fmr_alloc;
	ibdev->ib_dev.map_phys_fmr	= mlx4_ib_map_phys_fmr;
	ibdev->ib_dev.unmap_fmr		= mlx4_ib_unmap_fmr;
	ibdev->ib_dev.dealloc_fmr	= mlx4_ib_fmr_dealloc;
	if (dev->caps.flags & MLX4_DEV_CAP_FLAG_XRC) {
		ibdev->ib_dev.create_xrc_srq = mlx4_ib_create_xrc_srq;
		ibdev->ib_dev.alloc_xrcd = mlx4_ib_alloc_xrcd;
		ibdev->ib_dev.dealloc_xrcd = mlx4_ib_dealloc_xrcd;
		ibdev->ib_dev.create_xrc_rcv_qp = mlx4_ib_create_xrc_rcv_qp;
		ibdev->ib_dev.modify_xrc_rcv_qp = mlx4_ib_modify_xrc_rcv_qp;
		ibdev->ib_dev.query_xrc_rcv_qp = mlx4_ib_query_xrc_rcv_qp;
		ibdev->ib_dev.reg_xrc_rcv_qp = mlx4_ib_reg_xrc_rcv_qp;
		ibdev->ib_dev.unreg_xrc_rcv_qp = mlx4_ib_unreg_xrc_rcv_qp;
		ibdev->ib_dev.uverbs_cmd_mask |=
			(1ull << IB_USER_VERBS_CMD_CREATE_XRC_SRQ)	|
			(1ull << IB_USER_VERBS_CMD_OPEN_XRC_DOMAIN)	|
			(1ull << IB_USER_VERBS_CMD_CLOSE_XRC_DOMAIN)	|
			(1ull << IB_USER_VERBS_CMD_CREATE_XRC_RCV_QP)	|
			(1ull << IB_USER_VERBS_CMD_MODIFY_XRC_RCV_QP)	|
			(1ull << IB_USER_VERBS_CMD_QUERY_XRC_RCV_QP)	|
			(1ull << IB_USER_VERBS_CMD_REG_XRC_RCV_QP)	|
			(1ull << IB_USER_VERBS_CMD_UNREG_XRC_RCV_QP);
	}


	spin_lock_init(&iboe->lock);
	if (init_node_data(ibdev))
		goto err_map;

	for (k = 0; k < ibdev->num_ports; ++k) {
		err = mlx4_counter_alloc(ibdev->dev, &ibdev->counters[k]);
		if (err)
			ibdev->counters[k] = -1;
		else
			mlx4_set_iboe_counter(dev, ibdev->counters[k], k + 1);
	}

	spin_lock_init(&ibdev->sm_lock);
	mutex_init(&ibdev->cap_mask_mutex);
	mutex_init(&ibdev->xrc_reg_mutex);

	if (ib_register_device(&ibdev->ib_dev))
		goto err_counter;

	if (mlx4_ib_mad_init(ibdev))
		goto err_reg;
	if (dev->caps.flags & MLX4_DEV_CAP_FLAG_IBOE && !iboe->nb.notifier_call) {
		iboe->nb.notifier_call = mlx4_ib_netdev_event;
		err = register_netdevice_notifier(&iboe->nb);
		if (err)
			goto err_reg;
	}
	for (i = 0; i < ARRAY_SIZE(mlx4_class_attributes); ++i) {
		if (device_create_file(&ibdev->ib_dev.dev,
				       mlx4_class_attributes[i]))
			goto err_notif;
	}

	if(sysfs_create_group(&ibdev->ib_dev.dev.kobj, &diag_counters_group))
		goto err_notif;

	ibdev->ib_active = 1;

	return ibdev;

err_notif:
	if (unregister_netdevice_notifier(&ibdev->iboe.nb))
		printk(KERN_WARNING "failure unregistering notifier\n");
	flush_workqueue(wq);

err_reg:
	ib_unregister_device(&ibdev->ib_dev);

err_counter:
	for (; k; --k)
		mlx4_counter_free(ibdev->dev, ibdev->counters[k - 1]);

err_map:
	iounmap(ibdev->priv_uar.map);

err_uar:
	mlx4_uar_free(dev, &ibdev->priv_uar);

err_pd:
	mlx4_pd_free(dev, ibdev->priv_pdn);

err_dealloc:
	ib_dealloc_device(&ibdev->ib_dev);

	return NULL;
}

static void mlx4_ib_remove(struct mlx4_dev *dev, void *ibdev_ptr)
{
	struct mlx4_ib_dev *ibdev = ibdev_ptr;
	int p;
	int k;

	sysfs_remove_group(&ibdev->ib_dev.dev.kobj, &diag_counters_group);

	mlx4_ib_mad_cleanup(ibdev);
	ib_unregister_device(&ibdev->ib_dev);
	for (k = 0; k < ibdev->num_ports; ++k)
		mlx4_counter_free(ibdev->dev, ibdev->counters[k]);

	if (ibdev->iboe.nb.notifier_call) {
		unregister_netdevice_notifier(&ibdev->iboe.nb);
		flush_workqueue(wq);
		ibdev->iboe.nb.notifier_call = NULL;
	}
	iounmap(ibdev->priv_uar.map);

	mlx4_foreach_port(p, dev, MLX4_PORT_TYPE_IB)
		mlx4_CLOSE_PORT(dev, p);

	mlx4_uar_free(dev, &ibdev->priv_uar);
	mlx4_pd_free(dev, ibdev->priv_pdn);
	ib_dealloc_device(&ibdev->ib_dev);
}

static void mlx4_ib_event(struct mlx4_dev *dev, void *ibdev_ptr,
			  enum mlx4_dev_event event, int port)
{
	struct ib_event ibev;
	struct mlx4_ib_dev *ibdev = to_mdev((struct ib_device *) ibdev_ptr);

	if (port > ibdev->num_ports)
		return;

	switch (event) {
	case MLX4_DEV_EVENT_PORT_UP:
		ibev.event = IB_EVENT_PORT_ACTIVE;
		break;

	case MLX4_DEV_EVENT_PORT_DOWN:
		ibev.event = IB_EVENT_PORT_ERR;
		break;

	case MLX4_DEV_EVENT_CATASTROPHIC_ERROR:
		ibdev->ib_active = 0;
		ibev.event = IB_EVENT_DEVICE_FATAL;
		break;

	default:
		return;
	}

	ibev.device	      = ibdev_ptr;
	ibev.element.port_num = port;

	ib_dispatch_event(&ibev);
}

static struct mlx4_interface mlx4_ib_interface = {
	.add	= mlx4_ib_add,
	.remove	= mlx4_ib_remove,
       .event  = mlx4_ib_event,
       .get_prot_dev = get_ibdev,
       .protocol     = MLX4_PROT_IB,
};

static int __init mlx4_ib_init(void)
{
	int err;

	wq = create_singlethread_workqueue("mlx4_ib");
	if (!wq)
		return -ENOMEM;

	err = mlx4_register_interface(&mlx4_ib_interface);
	if (err) {
		destroy_workqueue(wq);
		return err;
	}

	return 0;
}

static void __exit mlx4_ib_cleanup(void)
{
	mlx4_unregister_interface(&mlx4_ib_interface);
	destroy_workqueue(wq);
}

module_init_order(mlx4_ib_init, SI_ORDER_MIDDLE);
module_exit(mlx4_ib_cleanup);

#undef MODULE_VERSION
#include <sys/module.h>
static int
mlx4ib_evhand(module_t mod, int event, void *arg)
{
        return (0);
}
static moduledata_t mlx4ib_mod = {
        .name = "mlx4ib",
        .evhand = mlx4ib_evhand,
};
DECLARE_MODULE(mlx4ib, mlx4ib_mod, SI_SUB_OFED_PREINIT, SI_ORDER_ANY);
MODULE_DEPEND(mlx4ib, mlx4, 1, 1, 1);
