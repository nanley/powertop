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
#include "tuningusb.h"
#include <string.h>

usb_tunable::usb_tunable(struct udev_device *usb_dev) : tunable("", 0.9, _("Good"), _("Bad"), _("Unknown"))
{
	tun_dev = usb_dev;
	udev_device_ref(tun_dev);

	const char * name = udev_device_get_sysname(usb_dev);
	const char * vendor = udev_device_get_sysattr_value(usb_dev, "manufacturer");
	const char * product = udev_device_get_sysattr_value(usb_dev, "product");

	if (vendor && product && strstr(vendor, "Linux "))
		sprintf(desc, _("Autosuspend for USB device %s [%s]"), product, vendor);
	else if (product)
		sprintf(desc, _("Autosuspend for USB device %s [%s]"), product, name);
	else if (vendor && strstr(vendor, "Linux "))
		sprintf(desc, _("Autosuspend for USB device %s [%s]"), vendor, name);
	else {
		const char * str1 = udev_device_get_sysattr_value(usb_dev, "idVendor");
		const char * str2 = udev_device_get_sysattr_value(usb_dev, "idProduct");
		sprintf(desc, _("Autosuspend for unknown USB device %s (%s:%s)"), name, str1, str2);
	}

	const char * path = udev_device_get_syspath(usb_dev);
	sprintf(toggle_good, "echo 'auto' > '%s/power/control';", path);
	sprintf(toggle_bad, "echo 'on' > '%s/power/control';", path);
}

usb_tunable::~usb_tunable()
{
	udev_device_unref(tun_dev);
}

int usb_tunable::good_bad(void)
{
	const char * str1 = udev_device_get_sysattr_value(tun_dev, "power/control");
	return strcmp(str1, "auto") == 0 ? TUNE_GOOD : TUNE_BAD;
}

void usb_tunable::toggle(void)
{
	std::string str = good_bad() == TUNE_GOOD ? "on" : "auto";
	udev_device_set_sysattr_value(tun_dev, "power/control", &str[0]);
}

const char *usb_tunable::toggle_script(void)
{
	return good_bad() == TUNE_GOOD ? toggle_bad : toggle_good;
}

static void add_usb_callback(const char *d_name)
{
	struct udev *udev = get_udev();
	struct udev_device *dev = udev_device_new_from_syspath(udev, d_name);
	struct udev_enumerate *dev_enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_parent(dev_enumerate, dev);
	udev_enumerate_add_match_sysattr(dev_enumerate, "supports_autosuspend", "0");
	udev_enumerate_scan_devices(dev_enumerate);

	/* If no devices in tree lack autosuspend support, add to tunables */
	if (udev_enumerate_get_list_entry(dev_enumerate) == NULL)
		all_tunables.push_back(new usb_tunable(dev));

	/* Cleanup */
	udev_device_unref(dev);
	udev_enumerate_unref(dev_enumerate);
}

void add_usb_tunables(void)
{
	process_subsystem("usb", add_usb_callback, 2,
					  "+power/control", NULL,
					  "+power/active_duration", NULL);
}
