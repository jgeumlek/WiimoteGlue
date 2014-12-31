
#include <stdio.h>
#include <string.h>

#include "wiimoteglue.h"

/*
 * Udev functionality. Handles finding and adding wiimotes.
 * Note that xwii.c handles removing wiimotes.
 */

int add_wii_device(struct wiimoteglue_state *state, struct xwii_iface *wiidev, const char* uniq) {
  int i;
  struct wii_device_list *list_node = calloc(1,sizeof(struct wii_device_list));

  if (!(xwii_iface_available(wiidev) & (XWII_IFACE_CORE | XWII_IFACE_PRO_CONTROLLER | XWII_IFACE_BALANCE_BOARD))) {
    printf("Tried to add a non-wiimote deviced...\n");
    xwii_iface_unref(wiidev);
    free(list_node);
    return -1;
  }

  list_node->device = wiidev;
  list_node->ifaces = xwii_iface_opened(wiidev);
  if (list_node->ifaces & XWII_IFACE_NUNCHUK) {
    list_node->map = &state->mode_nunchuk;
  } else if (list_node->ifaces & (XWII_IFACE_CLASSIC_CONTROLLER | XWII_IFACE_PRO_CONTROLLER)) {
    list_node->map = &state->mode_classic;
  } else {
    list_node->map = &state->mode_no_ext;
  }

  if (list_node->map->accel_active) {
    xwii_iface_open(wiidev,XWII_IFACE_ACCEL);
  } else {
    xwii_iface_close(wiidev,XWII_IFACE_ACCEL);
  }

  if (list_node->map->IR_count) {
    xwii_iface_open(wiidev,XWII_IFACE_IR);
  } else {
    xwii_iface_close(wiidev,XWII_IFACE_IR);
  }

  if (! (list_node->ifaces & XWII_IFACE_BALANCE_BOARD)) {
    for (i = 0; i < NUM_SLOTS; i++) {
      if (state->slots[i].has_wiimote == 0) {
	state->slots[i].has_wiimote = 1;
	list_node->slot = &state->slots[i];
	if (list_node->ifaces & XWII_IFACE_PRO_CONTROLLER) {
	  printf("Wii U Pro controller added to slot %d\n", i+1);
	  list_node->type = PRO;
	} else {
	  printf("Wiimote added to slot %d\n", i+1);
	  list_node->type = REMOTE;
	}


	list_node->fd = xwii_iface_get_fd(wiidev);

	wiimoteglue_epoll_watch_wiimote(state->epfd, list_node);

	break;
      }
    }

    if (list_node->slot == NULL) {
      printf("Wiimote or Wii U Pro detected, but all slots are filled.");
      xwii_iface_unref(wiidev);
      free(list_node);
      return -1;
    }


  } else {
    list_node->type = BALANCE;
    for (i = 0; i < NUM_SLOTS; i++) {
      if (state->slots[i].has_board == 0) {
	state->slots[i].has_board = 1;
	list_node->slot = &state->slots[i];
	printf("Balance board added to slot %d\n", i+1);

	list_node->fd = xwii_iface_get_fd(wiidev);

	wiimoteglue_epoll_watch_wiimote(state->epfd, list_node);

	break;
      }
    }

    if (list_node->slot == NULL) {
      printf("Balance board detected, but all slots are filled.");
      xwii_iface_unref(wiidev);
      free(list_node);
      return -1;
    }


  }


  list_node->bluetooth_addr = malloc(18*sizeof(char));
  strncpy(list_node->bluetooth_addr, uniq, 17);
  list_node->bluetooth_addr[17] = '\0';

  list_node->id = malloc(32*sizeof(char));
  snprintf(list_node->id,32,"dev%d",++(state->dev_count));

  printf("\tid: %s\n\taddress %s\n",list_node->id, list_node->bluetooth_addr);


  list_node->next = state->devlist.next;
  list_node->prev = &state->devlist;
  state->devlist.next = list_node;

  return 0;

}

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

	  struct xwii_iface *wiidev;
	  xwii_iface_new(&wiidev,syspath);
	  xwii_iface_watch(wiidev,true);

	  xwii_iface_open(wiidev,  XWII_IFACE_WRITABLE | (XWII_IFACE_ALL ^ XWII_IFACE_ACCEL ^ XWII_IFACE_IR));


	  add_wii_device(state,wiidev,uniq);
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