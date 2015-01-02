
#include <stdio.h>
#include <string.h>

#include "wiimoteglue.h"

/*
 * Udev functionality. Handles finding and adding wiimotes.
 *
 */



int wiimoteglue_udev_monitor_init(struct udev **udev, struct udev_monitor **monitor, int *mon_fd) {

  *udev = udev_new();
  if (!*udev) {
    printf("error opening udev");
    return -1;
  }



  *monitor = udev_monitor_new_from_netlink(*udev,"udev");
  udev_monitor_filter_add_match_subsystem_devtype(*monitor,"hid",NULL);

  udev_monitor_enable_receiving(*monitor);

  *mon_fd = udev_monitor_get_fd(*monitor);

  return 0;
}

int wiimoteglue_udev_handle_event(struct wiimoteglue_state *state) {
  struct udev_device *dev;
  dev = udev_monitor_receive_device(state->monitor);
  if (dev) {
    const char* action;
    const char* syspath;
    const char* driver;
    const char* subsystem;
    const char* uniq; /*should be the bluetooth MAC*/
    action = udev_device_get_action(dev);
    syspath = udev_device_get_syspath(dev);
    driver = udev_device_get_driver(dev);
    subsystem = udev_device_get_subsystem(dev);
    uniq = udev_device_get_property_value(dev, "HID_UNIQ");


    if (strcmp(action,"change") == 0) {
      //It seems "change" happens once the wiimote is ready,
      //"add" happens too early.
      if (subsystem != NULL && strcmp(subsystem, "hid") == 0) {
	if (driver != NULL && strcmp(driver,"wiimote") == 0) {




	  add_wii_device(state,syspath,uniq);
	}
      }
    }
    //The controller interface will detect its removal.
    //Trying to use the udev remove event would require
    //searching the device list to find the matching entry.
    //xwiimote says not to do this, but I had trouble
    //handling the extension insertions/removals.
    //Seemed like one couldn't add multiple parent-match filters?

    //With most use cases having only a handful of controllers,
    //this might be okay?
  }

  udev_device_unref(dev);

  return 0;
}