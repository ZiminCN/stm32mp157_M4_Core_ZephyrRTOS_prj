/*
 * Copyright (c) 2022, STMICROELECTRONICS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/ipm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <metal/device.h>
#include <openamp/open_amp.h>
#include <resource_table.h>
LOG_MODULE_REGISTER(openamp_rsc_table, LOG_LEVEL_DBG);

#define SHM_DEVICE_NAME "shm"

#if !DT_HAS_CHOSEN(zephyr_ipc_shm)
#error "Sample requires definition of shared memory for rpmsg"
#endif

/* Constants derived from device tree */
#define SHM_NODE       DT_CHOSEN(zephyr_ipc_shm)
#define SHM_START_ADDR DT_REG_ADDR(SHM_NODE)
#define SHM_SIZE       DT_REG_SIZE(SHM_NODE)

#define APP_TASK_STACK_SIZE (1024)

#define MAX_TTY_EPT 2
#define MAX_RAW_EPT 2

#define APP_TTY_TASK_STACK_SIZE (2048)
#define APP_RAW_TASK_STACK_SIZE (2048)

K_THREAD_STACK_DEFINE(thread_mng_stack, APP_TASK_STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_rp__client_stack, APP_TASK_STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_tty_stack, APP_TTY_TASK_STACK_SIZE);
// K_THREAD_STACK_DEFINE(thread_raw_stack, APP_RAW_TASK_STACK_SIZE);

static struct k_thread thread_mng_data;
// static struct k_thread thread_rp__client_data;
static struct k_thread thread_tty_data;
// static struct k_thread thread_raw_data;

static const struct device *const ipm_handle = DEVICE_DT_GET(DT_CHOSEN(zephyr_ipc));

static metal_phys_addr_t shm_physmap = SHM_START_ADDR;
static metal_phys_addr_t rsc_tab_physmap;

struct metal_device shm_device = {.name = SHM_DEVICE_NAME,
				  .num_regions = 2,
				  .regions =
					  {
						  {.virt = NULL}, /* shared memory */
						  {.virt = NULL}, /* rsc_table memory */
					  },
				  .node = {NULL},
				  .irq_num = 0,
				  .irq_info = NULL};

struct rpmsg_rcv_msg {
	void *data;
	size_t len;
	uint32_t src;
};

static struct metal_io_region *shm_io;
static struct rpmsg_virtio_shm_pool shpool;

static struct metal_io_region *rsc_io;
static struct rpmsg_virtio_device rvdev;

static struct fw_resource_table *rsc_table;
static struct rpmsg_device *rpdev;

static char rx_cs_msg[20]; /* should receive "Hello world!" */
static struct rpmsg_endpoint cs_ept;
static struct rpmsg_rcv_msg cs_msg = {.data = rx_cs_msg};

static struct rpmsg_endpoint tty_ept[MAX_TTY_EPT];
static struct rpmsg_rcv_msg tty_msg[MAX_TTY_EPT];

// static struct rpmsg_endpoint raw_ept[MAX_RAW_EPT];
// static struct rpmsg_rcv_msg raw_msg[MAX_RAW_EPT];

static K_SEM_DEFINE(data_sem, 0, 1);
static K_SEM_DEFINE(data_cs_sem, 0, 1);
static K_SEM_DEFINE(data_tty_sem, 0, 1);
// static K_SEM_DEFINE(data_raw_sem, 0, 1);

static void platform_ipm_callback(const struct device *dev, void *context, uint32_t id,
				  volatile void *data)
{
	LOG_DBG("%s: msg received from mb %d\n", __func__, id);
	k_sem_give(&data_sem);
}

static int rpmsg_recv_cs_callback(struct rpmsg_endpoint *ept, void *data, size_t len, uint32_t src,
				  void *priv)
{
	memcpy(cs_msg.data, data, len);
	cs_msg.len = len;
	k_sem_give(&data_cs_sem);

	return RPMSG_SUCCESS;
}

static int rpmsg_recv_tty_callback(struct rpmsg_endpoint *ept, void *data, size_t len, uint32_t src,
				   void *priv)
{
	struct rpmsg_rcv_msg *tty_msg = priv;

	rpmsg_hold_rx_buffer(ept, data);
	tty_msg->data = data;
	tty_msg->len = len;
	k_sem_give(&data_tty_sem);

	return RPMSG_SUCCESS;
}

// static int rpmsg_recv_raw_callback(struct rpmsg_endpoint *ept, void *data, size_t len,
// uint32_t
// src, 				   void *priv)
// {
// 	struct rpmsg_rcv_msg *raw_msg = priv;

// 	rpmsg_hold_rx_buffer(ept, data);
// 	raw_msg->data = data;
// 	raw_msg->len = len;
// 	raw_msg->src = src;

// 	k_sem_give(&data_raw_sem);

// 	return RPMSG_SUCCESS;
// }

static void receive_message(unsigned char **msg, unsigned int *len)
{
	int status = k_sem_take(&data_sem, K_FOREVER);

	if (status == 0) {
		rproc_virtio_notified(rvdev.vdev, VRING1_ID);
	}
}

static void rpmsg_service_unbind(struct rpmsg_endpoint *ept)
{
	LOG_INF("destroy end point name %s\n", ept->name);
	/*
	 * FIXME: need to set endpoint name to void to not send a ns destroy
	 * announcement that generates an error on Linux side
	 */
	ept->name[0] = 0;
	rpmsg_destroy_ept(ept);
}

static void new_service_cb(struct rpmsg_device *rdev, const char *name, uint32_t src)
{
	int ret;

	if (strcmp(name, "rpmsg-tty") == 0 && !tty_ept[1].rdev) {
		tty_ept[1].priv = &tty_msg[1];
		ret = rpmsg_create_ept(&tty_ept[1], rpdev, "rpmsg-tty", RPMSG_ADDR_ANY, src,
				       rpmsg_recv_tty_callback, rpmsg_service_unbind);
		if (ret != 0) {
			LOG_ERR("Creating remote endpoint %s failed with error %d", name, ret);
			return;
		}

		LOG_INF("request to bind new service %s\n", name);
		rpmsg_send(&tty_ept[1], "bound", sizeof("bound"));

		return;
	}
	LOG_ERR("%s: unexpected ns service receive for name %s\n", __func__, name);
}

int mailbox_notify(void *priv, uint32_t id)
{
	ARG_UNUSED(priv);

	LOG_DBG("%s: msg received\n", __func__);
	ipm_send(ipm_handle, 0, id, NULL, 0);

	return 0;
}

int platform_init(void)
{
	int rcs_size;
	struct metal_device *device;
	struct metal_init_params metal_params = METAL_INIT_DEFAULTS;
	int status;

	status = metal_init(&metal_params);
	if (status) {
		LOG_DBG("metal_init: failed: %d\n", status);
		return -1;
	}

	status = metal_register_generic_device(&shm_device);
	if (status) {
		LOG_DBG("Couldn't register shared memory: %d\n", status);
		return -1;
	}

	status = metal_device_open("generic", SHM_DEVICE_NAME, &device);
	if (status) {
		LOG_DBG("metal_device_open failed: %d\n", status);
		return -1;
	}

	/* declare shared memory region */
	metal_io_init(&device->regions[0], (void *)SHM_START_ADDR, &shm_physmap, SHM_SIZE, -1, 0,
		      NULL);

	shm_io = metal_device_io_region(device, 0);
	if (!shm_io) {
		LOG_DBG("Failed to get shm_io region\n");
		return -1;
	}

	/* declare resource table region */
	rsc_table_get(&rsc_table, &rcs_size);
	rsc_tab_physmap = (uintptr_t)rsc_table;

	metal_io_init(&device->regions[1], rsc_table, &rsc_tab_physmap, rcs_size, -1, 0, NULL);

	rsc_io = metal_device_io_region(device, 1);
	if (!rsc_io) {
		LOG_DBG("Failed to get rsc_io region\n");
		return -1;
	}

	/* setup IPM */
	if (!device_is_ready(ipm_handle)) {
		LOG_DBG("IPM device is not ready\n");
		return -1;
	}

	ipm_register_callback(ipm_handle, platform_ipm_callback, NULL);

	status = ipm_set_enabled(ipm_handle, 1);
	if (status) {
		LOG_DBG("ipm_set_enabled failed\n");
		return -1;
	}

	return 0;
}

static void cleanup_system(void)
{
	ipm_set_enabled(ipm_handle, 0);
	rpmsg_deinit_vdev(&rvdev);
	metal_finish();
}

struct rpmsg_device *platform_create_rpmsg_vdev(unsigned int vdev_index, unsigned int role,
						void (*rst_cb)(struct virtio_device *vdev),
						rpmsg_ns_bind_cb ns_cb)
{
	struct fw_rsc_vdev_vring *vring_rsc;
	struct virtio_device *vdev;
	int ret;

	vdev = rproc_virtio_create_vdev(VIRTIO_DEV_DEVICE, VDEV_ID, rsc_table_to_vdev(rsc_table),
					rsc_io, NULL, mailbox_notify, NULL);

	if (!vdev) {
		LOG_DBG("failed to create vdev\r\n");
		return NULL;
	}

	/* wait master rpmsg init completion */
	rproc_virtio_wait_remote_ready(vdev);

	vring_rsc = rsc_table_get_vring0(rsc_table);
	ret = rproc_virtio_init_vring(vdev, 0, vring_rsc->notifyid, (void *)vring_rsc->da, rsc_io,
				      vring_rsc->num, vring_rsc->align);
	if (ret) {
		LOG_DBG("failed to init vring 0\r\n");
		goto failed;
	}

	vring_rsc = rsc_table_get_vring1(rsc_table);
	ret = rproc_virtio_init_vring(vdev, 1, vring_rsc->notifyid, (void *)vring_rsc->da, rsc_io,
				      vring_rsc->num, vring_rsc->align);
	if (ret) {
		LOG_DBG("failed to init vring 1\r\n");
		goto failed;
	}

	rpmsg_virtio_init_shm_pool(&shpool, NULL, SHM_SIZE);
	ret = rpmsg_init_vdev(&rvdev, vdev, ns_cb, shm_io, &shpool);

	if (ret) {
		LOG_DBG("failed rpmsg_init_vdev\r\n");
		goto failed;
	}

	return rpmsg_virtio_get_rpmsg_device(&rvdev);

failed:
	rproc_virtio_remove_vdev(vdev);

	return NULL;
}

void app_rpmsg_client_sample(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	unsigned int msg_cnt = 0;
	int ret = 0;

	k_sem_take(&data_cs_sem, K_FOREVER);

	printk("\r\nOpenAMP[remote] Linux sample client responder started\r\n");

	ret = rpmsg_create_ept(&cs_ept, rpdev, "rpmsg-client-sample", RPMSG_ADDR_ANY,
			       RPMSG_ADDR_ANY, rpmsg_recv_cs_callback, NULL);

	while (msg_cnt < 100) {
		k_sem_take(&data_cs_sem, K_FOREVER);
		msg_cnt++;
		rpmsg_send(&cs_ept, cs_msg.data, cs_msg.len);
	}
	rpmsg_destroy_ept(&cs_ept);

	printk("OpenAMP Linux sample client responder ended\n");
}

void app_rpmsg_tty(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	unsigned char tx_buff[512];
	int i, ret = 0;

	k_sem_take(&data_tty_sem, K_FOREVER);

	printk("\r\nOpenAMP[remote] Linux tty responder started\r\n");

	/*
	 * The first TTY channel instance is created locally
	 * The second one will be instantiate on a name service announcement
	 */
	tty_ept[0].priv = &tty_msg[0];
	ret = rpmsg_create_ept(&tty_ept[0], rpdev, "rpmsg-tty", RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
			       rpmsg_recv_tty_callback, NULL);

	while (tty_ept[0].addr != RPMSG_ADDR_ANY) {
		k_sem_take(&data_tty_sem, K_FOREVER);
		for (i = 0; i < MAX_TTY_EPT; i++) {
			if (tty_msg[i].len) {
				snprintf(tx_buff, 8, "TTY %d: ", i);
				memcpy(&tx_buff[7], tty_msg[i].data, tty_msg[i].len);
				rpmsg_send(&tty_ept[i], tx_buff, tty_msg[i].len + 8);
				rpmsg_release_rx_buffer(&tty_ept[i], tty_msg[i].data);
			}
			tty_msg[i].len = 0;
			tty_msg[i].data = NULL;
		}
	}
	rpmsg_destroy_ept(&tty_ept[0]);

	printk("OpenAMP Linux TTY responder ended\n");
}

// void app_rpmsg_raw(void *arg1, void *arg2, void *arg3)
// {
// 	ARG_UNUSED(arg1);
// 	ARG_UNUSED(arg2);
// 	ARG_UNUSED(arg3);
// 	unsigned char buff[512];
// 	int i, ret = 0;

// 	k_sem_take(&data_raw_sem, K_FOREVER);

// 	printk("\r\nOpenAMP[remote] Linux raw data responder started\r\n");

// 	raw_ept[0].priv = &raw_msg[0];
// 	ret = rpmsg_create_ept(&raw_ept[0], rpdev, "rpmsg-raw", RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
// 			       rpmsg_recv_raw_callback, NULL);

// 	printk("\r\nOpenAMP[remote] create a endpoint with address and dest_address set to "
// 	       "0x1\r\n");

// 	ret = rpmsg_create_ept(&raw_ept[1], rpdev, "rpmsg-raw", 0x1, 0x1, rpmsg_recv_raw_callback,
// 			       NULL);

// 	raw_ept[1].priv = &raw_msg[1];
// 	while (raw_ept[0].addr != RPMSG_ADDR_ANY) {
// 		k_sem_take(&data_raw_sem, K_FOREVER);
// 		for (i = 0; i < MAX_RAW_EPT; i++) {
// 			if (raw_msg[i].len) {
// 				snprintf(buff, 18, "from ept 0x%04x: ", raw_ept[i].addr);
// 				memcpy(&buff[17], raw_msg[i].data, raw_msg[i].len);
// 				rpmsg_sendto(&raw_ept[i], buff, raw_msg[i].len + 18,
// 					     raw_msg[i].src);
// 				rpmsg_release_rx_buffer(&raw_ept[i], raw_msg[i].data);
// 			}
// 			raw_msg[i].len = 0;
// 			raw_msg[i].data = NULL;
// 		}
// 	}
// 	rpmsg_destroy_ept(&raw_ept[0]);
// 	rpmsg_destroy_ept(&raw_ept[1]);

// 	printk("OpenAMP Linux raw data responder ended\n");
// }

void rpmsg_mng_task(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	unsigned char *msg;
	unsigned int len;
	int ret = 0;

	printk("\r\nOpenAMP[remote]  linux responder demo started\r\n");

	/* Initialize platform */
	ret = platform_init();
	if (ret) {
		LOG_ERR("Failed to initialize platform\n");
		ret = -1;
		goto task_end;
	}

	rpdev = platform_create_rpmsg_vdev(0, VIRTIO_DEV_DEVICE, NULL, new_service_cb);
	if (!rpdev) {
		LOG_ERR("Failed to create rpmsg virtio device\n");
		ret = -1;
		goto task_end;
	}

	/* start the rpmsg clients */
	// k_sem_give(&data_cs_sem);
	k_sem_give(&data_tty_sem);
	// k_sem_give(&data_raw_sem);

	while (1) {
		receive_message(&msg, &len);
	}

task_end:
	cleanup_system();

	printk("OpenAMP demo ended\n");
}

int main(void)
{
	printk("Starting application threads!\n");
	k_thread_create(&thread_mng_data, thread_mng_stack, APP_TASK_STACK_SIZE,
			(k_thread_entry_t)rpmsg_mng_task, NULL, NULL, NULL, K_PRIO_COOP(8), 0,
			K_NO_WAIT);
	// k_thread_create(&thread_rp__client_data, thread_rp__client_stack, APP_TASK_STACK_SIZE,
	// 		(k_thread_entry_t)app_rpmsg_client_sample,
	// 		NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);
	k_thread_create(&thread_tty_data, thread_tty_stack, APP_TTY_TASK_STACK_SIZE,
			(k_thread_entry_t)app_rpmsg_tty, NULL, NULL, NULL, K_PRIO_COOP(7), 0,
			K_NO_WAIT);
	// k_thread_create(&thread_raw_data, thread_raw_stack, APP_TASK_STACK_SIZE * 2,
	// 		(k_thread_entry_t)app_rpmsg_raw,
	// 		NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);

	return 0;
}
