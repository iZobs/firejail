/*
 * Copyright (C) 2014-2016 Firejail Authors
 *
 * This file is part of firejail project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "firejail.h"
#include <sys/mount.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <glob.h>
#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#ifndef _BSD_SOURCE
#define _BSD_SOURCE 
#endif
#include <sys/types.h>

typedef struct {
	const char *dev_fname;
	const char *run_fname;
} DevEntry;

static DevEntry dev[] = {
	{"/dev/snd", RUN_DEV_DIR "/snd"},
	{"/dev/dri", RUN_DEV_DIR "/dri"},
	{"/dev/nvidia0", RUN_DEV_DIR "/nvidia0"},
	{"/dev/nvidia1", RUN_DEV_DIR "/nvidia1"},
	{"/dev/nvidia2", RUN_DEV_DIR "/nvidia2"},
	{"/dev/nvidia3", RUN_DEV_DIR "/nvidia3"},
	{"/dev/nvidia4", RUN_DEV_DIR "/nvidia4"},
	{"/dev/nvidia5", RUN_DEV_DIR "/nvidia5"},
	{"/dev/nvidia6", RUN_DEV_DIR "/nvidia6"},
	{"/dev/nvidia7", RUN_DEV_DIR "/nvidia7"},
	{"/dev/nvidia8", RUN_DEV_DIR "/nvidia8"},
	{"/dev/nvidia9", RUN_DEV_DIR "/nvidia9"},
	{"/dev/nvidiactl", RUN_DEV_DIR "/nvidiactl"},
	{"/dev/nvidia-modset", RUN_DEV_DIR "/nvidia-modset"},
	{"/dev/nvidia-uvm", RUN_DEV_DIR "/nvidia-uvm"},
	{NULL, NULL}
};

static void deventry_mount(void) {
	int i = 0;
	while (dev[i].dev_fname != NULL) {
		struct stat s;
		if (stat(dev[i].run_fname, &s) == 0) {
			if (mkdir(dev[i].dev_fname, 0755) == -1)
				errExit("mkdir");
			if (chmod(dev[i].dev_fname, 0755) == -1)
				errExit("chmod");
			ASSERT_PERMS(dev[i].dev_fname, 0, 0, 0755);
			if (mount(dev[i].run_fname, dev[i].dev_fname, NULL, MS_BIND|MS_REC, NULL) < 0)
				errExit("mounting /dev/snd");
			fs_logger2("whitelist", dev[i].dev_fname);
		}
		
		i++;	
	}
}
	
static void create_char_dev(const char *path, mode_t mode, int major, int minor) {
	dev_t dev = makedev(major, minor);
	if (mknod(path, S_IFCHR | mode, dev) == -1)
		goto errexit;
	if (chmod(path, mode) < 0)
		goto errexit;
	ASSERT_PERMS(path, 0, 0, mode);

	return;
	
errexit:
	fprintf(stderr, "Error: cannot create %s device\n", path);
	exit(1);
}

static void create_link(const char *oldpath, const char *newpath) {
	if (symlink(oldpath, newpath) == -1)
		goto errexit;
	if (chown(newpath, 0, 0) < 0)
		goto errexit;
	return;

errexit:
	fprintf(stderr, "Error: cannot create %s device\n", newpath);
	exit(1);
}

void fs_private_dev(void){
	// install a new /dev directory
	if (arg_debug)
		printf("Mounting tmpfs on /dev\n");

	// create DRI_DIR
	fs_build_mnt_dir();
	
	// keep a copy of dev directory
	if (mkdir(RUN_DEV_DIR, 0755) == -1)
		errExit("mkdir");
	if (chmod(RUN_DEV_DIR, 0755) == -1)
		errExit("chmod");
	ASSERT_PERMS(RUN_DEV_DIR, 0, 0, 0755);
	if (mount("/dev", RUN_DEV_DIR, NULL, MS_BIND|MS_REC, NULL) < 0)
		errExit("mounting /dev/dri");

	// create DEVLOG_FILE
	int have_devlog = 0;
	struct stat s;
	if (stat("/dev/log", &s) == 0) {
		have_devlog = 1;
		FILE *fp = fopen(RUN_DEVLOG_FILE, "w");
		if (!fp)
			have_devlog = 0;
		else {
			fprintf(fp, "\n");
			fclose(fp);
			if (mount("/dev/log", RUN_DEVLOG_FILE, NULL, MS_BIND|MS_REC, NULL) < 0)
				errExit("mounting /dev/log");
		}
	}

	// mount tmpfs on top of /dev
	if (mount("tmpfs", "/dev", "tmpfs", MS_NOSUID | MS_STRICTATIME | MS_REC,  "mode=755,gid=0") < 0)
		errExit("mounting /dev");
	fs_logger("tmpfs /dev");
	
	deventry_mount();

	// bring back /dev/log
	if (have_devlog) {
		FILE *fp = fopen("/dev/log", "w");
		if (fp) {
			fprintf(fp, "\n");
			fclose(fp);
			if (mount(RUN_DEVLOG_FILE, "/dev/log", NULL, MS_BIND|MS_REC, NULL) < 0)
				errExit("mounting /dev/log");
			fs_logger("clone /dev/log");
		}
	}		
	if (mount(RUN_RO_DIR, RUN_DEV_DIR, "none", MS_BIND, "mode=400,gid=0") < 0)
		errExit("disable /dev/snd");

	
	// create /dev/shm
	if (arg_debug)
		printf("Create /dev/shm directory\n");
	if (mkdir("/dev/shm", 01777) == -1)
		errExit("mkdir");
	// mkdir sets only the file permission bits
	if (chmod("/dev/shm", 01777) < 0)
		errExit("chmod");
	ASSERT_PERMS("/dev/shm", 0, 0, 01777);
	fs_logger("mkdir /dev/shm");

	// create devices
	create_char_dev("/dev/zero", 0666, 1, 5); // mknod -m 666 /dev/zero c 1 5
	fs_logger("mknod /dev/zero");
	create_char_dev("/dev/null", 0666, 1, 3); // mknod -m 666 /dev/null c 1 3
	fs_logger("mknod /dev/null");
	create_char_dev("/dev/full", 0666, 1, 7); // mknod -m 666 /dev/full c 1 7
	fs_logger("mknod /dev/full");
	create_char_dev("/dev/random", 0666, 1, 8); // Mknod -m 666 /dev/random c 1 8
	fs_logger("mknod /dev/random");
	create_char_dev("/dev/urandom", 0666, 1, 9); // mknod -m 666 /dev/urandom c 1 9
	fs_logger("mknod /dev/urandom");
	create_char_dev("/dev/tty", 0666,  5, 0); // mknod -m 666 /dev/tty c 5 0
	fs_logger("mknod /dev/tty");
#if 0
	create_dev("/dev/tty0", "mknod -m 666 /dev/tty0 c 4 0");
	create_dev("/dev/console", "mknod -m 622 /dev/console c 5 1");
#endif

	// pseudo-terminal
	if (mkdir("/dev/pts", 0755) == -1)
		errExit("mkdir");
	if (chmod("/dev/pts", 0755) == -1)
		errExit("chmod");
	ASSERT_PERMS("/dev/pts", 0, 0, 0755);
	fs_logger("mkdir /dev/pts");
	create_char_dev("/dev/pts/ptmx", 0666, 5, 2); //"mknod -m 666 /dev/pts/ptmx c 5 2");
	fs_logger("mknod /dev/pts/ptmx");
	create_link("/dev/pts/ptmx", "/dev/ptmx");

// code before github issue #351
	// mount -vt devpts -o newinstance -o ptmxmode=0666 devpts //dev/pts
//	if (mount("devpts", "/dev/pts", "devpts", MS_MGC_VAL,  "newinstance,ptmxmode=0666") < 0)
//		errExit("mounting /dev/pts");


	// mount /dev/pts
	gid_t ttygid = get_tty_gid();
	char *data;
	if (asprintf(&data, "newinstance,gid=%d,mode=620,ptmxmode=0666", (int) ttygid) == -1)
		errExit("asprintf");
	if (mount("devpts", "/dev/pts", "devpts", MS_MGC_VAL,  data) < 0)
		errExit("mounting /dev/pts");
	free(data);
	fs_logger("clone /dev/pts");

#if 0
	// stdin, stdout, stderr
	create_link("/proc/self/fd", "/dev/fd");
	create_link("/proc/self/fd/0", "/dev/stdin");
	create_link("/proc/self/fd/1", "/dev/stdout");
	create_link("/proc/self/fd/2", "/dev/stderr");
#endif
}


void fs_dev_shm(void) {
	uid_t uid = getuid(); // set a new shm only if we started as root
	if (uid)
		return;

	if (is_dir("/dev/shm")) {
		if (arg_debug)
			printf("Mounting tmpfs on /dev/shm\n");
		if (mount("tmpfs", "/dev/shm", "tmpfs", MS_NOSUID | MS_STRICTATIME | MS_REC,  "mode=1777,gid=0") < 0)
			errExit("mounting /dev/shm");
		fs_logger("tmpfs /dev/shm");
	}
	else {
		char *lnk = realpath("/dev/shm", NULL);
		if (lnk) {
			if (!is_dir(lnk)) {
				// create directory
				if (mkdir(lnk, 01777))
					errExit("mkdir");
				// mkdir sets only the file permission bits
				if (chmod(lnk, 01777))
					errExit("chmod");
				ASSERT_PERMS(lnk, 0, 0, 01777);
			}
			if (arg_debug)
				printf("Mounting tmpfs on %s on behalf of /dev/shm\n", lnk);
			if (mount("tmpfs", lnk, "tmpfs", MS_NOSUID | MS_STRICTATIME | MS_REC,  "mode=1777,gid=0") < 0)
				errExit("mounting /var/tmp");
			fs_logger2("tmpfs", lnk);
			free(lnk);
		}
		else {
			fprintf(stderr, "Warning: /dev/shm not mounted\n");
			dbg_test_dir("/dev/shm");
		}
			
	}
}

void fs_dev_disable_sound() {
	if (mount(RUN_RO_DIR, "/dev/snd", "none", MS_BIND, "mode=400,gid=0") < 0)
		errExit("disable /dev/snd");
	fs_logger("blacklist /dev/snd");
}
