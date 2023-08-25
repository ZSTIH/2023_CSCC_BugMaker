// 本文件将一些spdk的结构体做声明，防止用到的一些结构体无法找到成员


struct nvme_ctrlr_opts {
	uint32_t prchk_flags;
	int32_t ctrlr_loss_timeout_sec;
	uint32_t reconnect_delay_sec;
	uint32_t fast_io_fail_timeout_sec;
	bool from_discovery_service;
};
struct nvme_ctrlr;
typedef void (*bdev_nvme_reset_cb)(void *cb_arg, bool success);
typedef void (*nvme_ctrlr_disconnected_cb)(struct nvme_ctrlr *nvme_ctrlr);
struct nvme_bdev_ctrlr {
	char				*name;
	TAILQ_HEAD(, nvme_ctrlr)	ctrlrs;
	TAILQ_HEAD(, nvme_bdev)		bdevs;
	TAILQ_ENTRY(nvme_bdev_ctrlr)	tailq;
};
struct nvme_ctrlr {
	/**
	 * points to pinned, physically contiguous memory region;
	 * contains 4KB IDENTIFY structure for controller which is
	 *  target for CONTROLLER IDENTIFY command during initialization
	 */
	struct spdk_nvme_ctrlr			*ctrlr;
	struct nvme_path_id			*active_path_id;
	int					ref;

	uint32_t				resetting : 1;
	uint32_t				reconnect_is_delayed : 1;
	uint32_t				fast_io_fail_timedout : 1;
	uint32_t				destruct : 1;
	uint32_t				ana_log_page_updating : 1;
	uint32_t				io_path_cache_clearing : 1;

	struct nvme_ctrlr_opts			opts;

	RB_HEAD(nvme_ns_tree, nvme_ns)		namespaces;

	struct spdk_opal_dev			*opal_dev;

	struct spdk_poller			*adminq_timer_poller;
	struct spdk_thread			*thread;

	bdev_nvme_reset_cb			reset_cb_fn;
	void					*reset_cb_arg;
	/* Poller used to check for reset/detach completion */
	struct spdk_poller			*reset_detach_poller;
	struct spdk_nvme_detach_ctx		*detach_ctx;

	uint64_t				reset_start_tsc;
	struct spdk_poller			*reconnect_delay_timer;

	nvme_ctrlr_disconnected_cb		disconnected_cb;

	/** linked list pointer for device list */
	TAILQ_ENTRY(nvme_ctrlr)			tailq;
	struct nvme_bdev_ctrlr			*nbdev_ctrlr;

	TAILQ_HEAD(nvme_paths, nvme_path_id)	trids;

	uint32_t				max_ana_log_page_size;
	struct spdk_nvme_ana_page		*ana_log_page;
	struct spdk_nvme_ana_group_descriptor	*copied_ana_desc;

	struct nvme_async_probe_ctx		*probe_ctx;

	pthread_mutex_t				mutex;
};

#define SPDK_MAX_THREAD_NAME_LEN	256
#define SPDK_MAX_POLLER_NAME_LEN	256
enum bdev_nvme_multipath_policy {
	BDEV_NVME_MP_POLICY_ACTIVE_PASSIVE,
	BDEV_NVME_MP_POLICY_ACTIVE_ACTIVE,
};

enum bdev_nvme_multipath_selector {
	BDEV_NVME_MP_SELECTOR_ROUND_ROBIN = 1,
	BDEV_NVME_MP_SELECTOR_QUEUE_DEPTH,
};

enum spdk_poller_state {
	/* The poller is registered with a thread but not currently executing its fn. */
	SPDK_POLLER_STATE_WAITING,

	/* The poller is currently running its fn. */
	SPDK_POLLER_STATE_RUNNING,

	/* The poller was unregistered during the execution of its fn. */
	SPDK_POLLER_STATE_UNREGISTERED,

	/* The poller is in the process of being paused.  It will be paused
	 * during the next time it's supposed to be executed.
	 */
	SPDK_POLLER_STATE_PAUSING,

	/* The poller is registered but currently paused.  It's on the
	 * paused_pollers list.
	 */
	SPDK_POLLER_STATE_PAUSED,
};
enum spdk_thread_state {
	/* The thread is processing poller and message by spdk_thread_poll(). */
	SPDK_THREAD_STATE_RUNNING,

	/* The thread is in the process of termination. It reaps unregistering
	 * poller are releasing I/O channel.
	 */
	SPDK_THREAD_STATE_EXITING,

	/* The thread is exited. It is ready to call spdk_thread_destroy(). */
	SPDK_THREAD_STATE_EXITED,
};
struct spdk_poller {
	TAILQ_ENTRY(spdk_poller)	tailq;
	RB_ENTRY(spdk_poller)		node;

	/* Current state of the poller; should only be accessed from the poller's thread. */
	enum spdk_poller_state		state;

	uint64_t			period_ticks;
	uint64_t			next_run_tick;
	uint64_t			run_count;
	uint64_t			busy_count;
	uint64_t			id;
	spdk_poller_fn			fn;
	void				*arg;
	struct spdk_thread		*thread;
	/* Native interruptfd for period or busy poller */
	int				interruptfd;
	spdk_poller_set_interrupt_mode_cb set_intr_cb_fn;
	void				*set_intr_cb_arg;

	char				name[SPDK_MAX_POLLER_NAME_LEN + 1];
};
struct spdk_thread {
	uint64_t			tsc_last;
	struct spdk_thread_stats	stats;
	/*
	 * Contains pollers actively running on this thread.  Pollers
	 *  are run round-robin. The thread takes one poller from the head
	 *  of the ring, executes it, then puts it back at the tail of
	 *  the ring.
	 */
	TAILQ_HEAD(active_pollers_head, spdk_poller)	active_pollers;
	/**
	 * Contains pollers running on this thread with a periodic timer.
	 */
	RB_HEAD(timed_pollers_tree, spdk_poller)	timed_pollers;
	struct spdk_poller				*first_timed_poller;
	/*
	 * Contains paused pollers.  Pollers on this queue are waiting until
	 * they are resumed (in which case they're put onto the active/timer
	 * queues) or unregistered.
	 */
	TAILQ_HEAD(paused_pollers_head, spdk_poller)	paused_pollers;
	struct spdk_ring		*messages;
	int				msg_fd;
	SLIST_HEAD(, spdk_msg)		msg_cache;
	size_t				msg_cache_count;
	spdk_msg_fn			critical_msg;
	uint64_t			id;
	uint64_t			next_poller_id;
	enum spdk_thread_state		state;
	int				pending_unregister_count;

	RB_HEAD(io_channel_tree, spdk_io_channel)	io_channels;
	TAILQ_ENTRY(spdk_thread)			tailq;

	char				name[SPDK_MAX_THREAD_NAME_LEN + 1];
	struct spdk_cpuset		cpumask;
	uint64_t			exit_timeout_tsc;

	int32_t				lock_count;

	/* Indicates whether this spdk_thread currently runs in interrupt. */
	bool				in_interrupt;
	bool				poller_unregistered;
	struct spdk_fd_group		*fgrp;

	/* User context allocated at the end */
	uint8_t				ctx[0];
};

static inline int
timed_poller_compare(struct spdk_poller *poller1, struct spdk_poller *poller2)
{
	if (poller1->next_run_tick < poller2->next_run_tick) {
		return -1;
	} else {
		return 1;
	}
}

RB_GENERATE_STATIC(timed_pollers_tree, spdk_poller, node, timed_poller_compare);
