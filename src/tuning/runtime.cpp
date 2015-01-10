/*
 * Copyright 2010, Intel Corporation
 *
 * This file is part of PowerTOP
 *
 * This program file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program in a file named COPYING; if not, write to the
 * Free Software Foundation, Inc,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 * or just google for it.
 *
 * Authors:
 *	Arjan van de Ven <arjan@linux.intel.com>
 */

#include "tuning.h"
#include "tunable.h"
#include "runtime.h"
#include <string.h>
#include <utility>
#include <unistd.h>
#include <sys/types.h>

#include "../lib.h"
#include "../devices/runtime_pm.h"

runtime_tunable::runtime_tunable(struct udev_device* udev_device) :
	tunable("", 0.4, _("Good"), _("Bad"), _("Unknown")), tun_dev(udev_device)
{
	udev_device_ref(tun_dev);
	const char *bus = udev_device_get_subsystem(udev_device);
	const char *dev = udev_device_get_sysname(udev_device);

	/* Give more accurate descriptions for PCI devices */
	if (strcmp(bus, "pci") == 0) {
		const char *uvendor = udev_device_get_sysattr_value(udev_device, "vendor");
		const char *udevice = udev_device_get_sysattr_value(udev_device, "device");
		uint16_t vendor = stoul(string(uvendor), NULL, 16);
		uint16_t device = stoul(string(udevice), NULL, 16);
		bus = "PCI";
		if (vendor && device) {
			char filename[4096];
			dev = pci_id_to_name(vendor, device, filename, 4095);
		}
	}

	if (!udevice_has_runtime_pm(udev_device))
		sprintf(desc, _("%s device %s has no runtime power management"), bus, dev);
	else
		sprintf(desc, _("Runtime PM for %s device %s"), bus, dev);

	const char *path = udev_device_get_syspath(udev_device);
	sprintf(toggle_good, "echo 'auto' > '%s/power/control';", path);
	sprintf(toggle_bad, "echo 'on' > '%s/power/control';", path);
}

runtime_tunable::~runtime_tunable()
{
	udev_device_unref(tun_dev);
}


int runtime_tunable::good_bad(void)
{
	const char * str1 = udev_device_get_sysattr_value(tun_dev, "power/control");
	return strcmp(str1, "auto") == 0 ? TUNE_GOOD : TUNE_BAD;
}

void runtime_tunable::toggle(void)
{
	std::string str = good_bad() == TUNE_GOOD ? "on" : "auto";
	udev_device_set_sysattr_value(tun_dev, "power/control", &str[0]);
}

const char *runtime_tunable::toggle_script(void)
{
	return good_bad() == TUNE_GOOD ? toggle_bad : toggle_good;
}

static void add_runtime_callback(const char *path)
{
	struct udev_device *dev = udev_device_new_from_syspath(get_udev(), path);
	runtime_tunable *runtime = new runtime_tunable(dev);

	if (!udevice_has_runtime_pm(dev))
		all_untunables.push_back(runtime);
	else
		all_tunables.push_back(runtime);

	/* Cleanup */
	udev_device_unref(dev);
}

void add_runtime_tunables(const char *bus)
{
	process_subsystem(bus, add_runtime_callback, 1, "+power/control", NULL);
}
