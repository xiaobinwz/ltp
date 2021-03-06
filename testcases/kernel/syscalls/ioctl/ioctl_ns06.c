// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2019 Federico Bonfiglio fedebonfi95@gmail.com
 */

/*
 * Test ioctl_ns with NS_GET_USERNS request.
 *
 * After the call to clone with the CLONE_NEWUSER flag,
 * child is created in a new user namespace. That's checked by
 * comparing its /proc/self/ns/user symlink and the parent's one,
 * which should be different.
 *
 */
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <sched.h>
#include "tst_test.h"
#include "lapi/ioctl_ns.h"
#include "lapi/namespaces_constants.h"

#define STACK_SIZE (1024 * 1024)

static char child_stack[STACK_SIZE];

static void setup(void)
{
	int exists = access("/proc/self/ns/user", F_OK);

	if (exists < 0)
		tst_res(TCONF, "namespace not available");
}

static int child(void *arg)
{
	TST_CHECKPOINT_WAIT(0);
	return 0;
}

static void run(void)
{
	pid_t pid = ltp_clone(CLONE_NEWUSER, &child, 0,
		STACK_SIZE, child_stack);
	char child_namespace[20];

	sprintf(child_namespace, "/proc/%i/ns/user", pid);
	int my_fd, child_fd, parent_fd;

	my_fd = SAFE_OPEN("/proc/self/ns/user", O_RDONLY);
	child_fd = SAFE_OPEN(child_namespace, O_RDONLY);
	parent_fd = ioctl(child_fd, NS_GET_USERNS);

	if (parent_fd == -1) {
		TST_CHECKPOINT_WAKE(0);

		if (errno == ENOTTY)
			tst_brk(TCONF, "ioctl(NS_GET_USERNS) not implemented");

		tst_brk(TBROK | TERRNO, "ioctl(NS_GET_USERNS) failed");
	}

	struct stat my_stat, child_stat, parent_stat;

	SAFE_FSTAT(my_fd, &my_stat);
	SAFE_FSTAT(child_fd, &child_stat);
	SAFE_FSTAT(parent_fd, &parent_stat);
	if (my_stat.st_ino != parent_stat.st_ino)
		tst_res(TFAIL, "parents have different inodes");
	else if (parent_stat.st_ino == child_stat.st_ino)
		tst_res(TFAIL, "child and parent have same inode");
	else
		tst_res(TPASS, "child and parent are consistent");
	SAFE_CLOSE(my_fd);
	SAFE_CLOSE(parent_fd);
	SAFE_CLOSE(child_fd);
	TST_CHECKPOINT_WAKE(0);
}

static struct tst_test test = {
	.test_all = run,
	.forks_child = 1,
	.needs_root = 1,
	.needs_checkpoints = 1,
	.min_kver = "4.9",
	.setup = setup
};
