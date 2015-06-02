
#include <stdio.h>
#include <string.h>

#include "wiimoteglue.h"

/*
 * Udev functionality. Handles finding and adding wiimotes.
 *
 */

struct wii_device * lookup_syspath(struct wii_device_list *devlist, char *syspath);

int wiimoteglue_udev_monitor_init(struct udev **udev, struct udev_monitor **monitor, int *mon_fd) {

  if (*udev == NULL) {
    *udev = udev_new();
    if (*udev == NULL) {
      printf("error opening udev");
      return -1;
    }
  }



  *monitor = udev_monitor_new_from_netlink(*udev,"udev");
  udev_monitor_filter_add_match_subsystem_devtype(*monitor,"hid",NULL);
  udev_monitor_filter_add_match_subsystem_devtype(*monitor,"input",NULL);

  udev_monitor_enable_receiving(*monitor);

  *mon_fd = udev_monitor_get_fd(*monitor);

  return 0;
}

int wiimoteglue_udev_enumerate(struct wiimoteglue_state *state, struct udev **udev) {
    if (*udev == NULL) {
    *udev = udev_new();
    if (*udev == NULL) {
      printf("error opening udev");
      return -1;
    }
  }


  struct udev_enumerate *enumerate;
  struct udev_list_entry *devices, *dev_list_entry;
  struct udev_device *dev;

  enumerate = udev_enumerate_new(*udev);
  udev_enumerate_add_match_subsystem(enumerate,"hid");

  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);

  udev_list_entry_foreach(dev_list_entry, devices) {
    const char* driver;
    const char* subsystem;
    const char* path;
    path = udev_list_entry_get_name(dev_list_entry);
    dev = udev_device_new_from_syspath(*udev, path);
    driver = udev_device_get_driver(dev);
    subsystem = udev_device_get_subsystem(dev);
    


    if (subsystem != NULL && strcmp(subsystem, "hid") == 0) {
      if (driver != NULL && strcmp(driver,"wiimote") == 0) {
	add_wii_device(state,udev_device_ref(dev));
      }
    }

    udev_device_unref(dev);

  }

  udev_enumerate_unref(enumerate);

  return 0;
}


int wiimoteglue_udev_handle_event(struct wiimoteglue_state *state) {
  struct udev_device *dev;
  dev = udev_monitor_receive_device(state->monitor);
  if (dev) {
    const char* action;
    const char* driver;
    const char* subsystem;
    action = udev_device_get_action(dev);
    driver = udev_device_get_driver(dev);
    subsystem = udev_device_get_subsystem(dev);
    

    if (strcmp(action,"change") == 0) {
      //It seems "change" happens once the wiimote is ready,
      //"add" happens too early.
      if (subsystem != NULL && strcmp(subsystem, "hid") == 0) {
	if (driver != NULL && strcmp(driver,"wiimote") == 0) {

	  add_wii_device(state,udev_device_ref(dev));
	}
      }
    }
    
    if (strcmp(action,"add") == 0) {
      struct udev_device *parentdev = udev_device_get_parent_with_subsystem_devtype(dev,"hid",NULL);
      char* syspath = udev_device_get_syspath(parentdev);
      struct wii_device *wiidev = lookup_syspath(&state->dev_list,syspath);
      if (wiidev != NULL)
        wiimoteglue_update_extensions(state,wiidev);
    }
    
    if (strcmp(action,"remove") == 0) {
      
      if (subsystem != NULL && strcmp(subsystem, "input")) {
        struct udev_device *parentdev = udev_device_get_parent_with_subsystem_devtype(dev,"hid",NULL);
        char* syspath = udev_device_get_syspath(parentdev);
        struct wii_device *wiidev = lookup_syspath(&state->dev_list,syspath);
        if (wiidev != NULL)
          wiimoteglue_update_extensions(state,wiidev);
      }
      
      if (subsystem != NULL && strcmp(subsystem, "hid") == 0) {
	
        char* syspath = udev_device_get_syspath(dev);
        struct wii_device *wiidev = lookup_syspath(&state->dev_list,syspath);
        if (wiidev != NULL) {
          remove_device_from_slot(wiidev);
          udev_device_unref(wiidev->udev);
          wiidev->udev = NULL;
          printf("Device %s disconnected from system.\n",wiidev->id);
          close_wii_device(state,wiidev);
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


struct wii_device * lookup_syspath(struct wii_device_list *devlist, char *syspath) {

  if (devlist == NULL) {
    return NULL;
  }
  if (syspath == NULL) {
    return NULL;
  }

  struct wii_device_list* list_node = devlist->next;

  while (*KEEP_LOOPING && list_node != devlist && list_node != NULL) {


    if (list_node->dev != NULL) {
      if (list_node->dev->udev != NULL) {
        char* devpath = udev_device_get_syspath(list_node->dev->udev);
      
        if (strncmp(syspath,devpath,2048) == 0)
          return list_node->dev;
        
      }

      list_node = list_node->next;
    }
  }


  return NULL;
}