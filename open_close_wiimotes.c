#include <stdio.h>
#include <errno.h>
#include <linux/input.h>
#include <string.h>
#include <unistd.h>

#include "wiimoteglue.h"

/*This logic was a bit too ugly
 *to place in the other files,
 *so here it is.
 */

int add_wii_device(struct wiimoteglue_state *state, char* syspath, const char* uniq) {
  int i;
  struct xwii_iface *wiidev;
  if (syspath == NULL)
    return -1;
  xwii_iface_new(&wiidev,syspath);
  xwii_iface_watch(wiidev,true);
  xwii_iface_open(wiidev,  XWII_IFACE_WRITABLE | (XWII_IFACE_ALL ^ XWII_IFACE_ACCEL ^ XWII_IFACE_IR));
  if (!(xwii_iface_available(wiidev) & (XWII_IFACE_CORE | XWII_IFACE_PRO_CONTROLLER | XWII_IFACE_BALANCE_BOARD))) {
    printf("Tried to add a non-wiimote device...\n");
    printf("Or we didn't have permission on the event devices?\n");
    printf("You might need to add a udev rule to give permission.\n");
    xwii_iface_unref(wiidev);
    return -1;
  }

  struct wii_device_list *list_node = calloc(1,sizeof(struct wii_device_list));



  list_node->device = wiidev;
  list_node->ifaces = xwii_iface_opened(wiidev);

  if (state->ignore_pro && (list_node->ifaces & XWII_IFACE_PRO_CONTROLLER)) {
    xwii_iface_unref(wiidev);
    free(list_node);
    return -0;
  }

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
    for (i = 1; i <= NUM_SLOTS; i++) {
      if (state->slots[i].has_wiimote == 0) {
	state->slots[i].has_wiimote++;
	list_node->slot = &state->slots[i];
	if (list_node->ifaces & XWII_IFACE_PRO_CONTROLLER) {
	  printf("Wii U Pro controller added to slot %d\n", i);
	  list_node->type = PRO;
	} else {
	  printf("Wiimote added to slot %d\n", i);
	  list_node->type = REMOTE;
	}


	list_node->fd = xwii_iface_get_fd(wiidev);

	wiimoteglue_epoll_watch_wiimote(state->epfd, list_node);

	break;
      }
    }

    if (list_node->slot == NULL) {
      printf("Wiimote or Wii U Pro detected, but all slots are filled.");
      printf("(WiimoteGlue still has it open and listening...)\n");
      list_node->slot = NULL;
    }


  } else {
    list_node->type = BALANCE;
    for (i = 1; i <= NUM_SLOTS; i++) {
      if (state->slots[i].has_board == 0) {
	state->slots[i].has_board++;
	list_node->slot = &state->slots[i];
	printf("Balance board added to slot %d\n", i);

	list_node->fd = xwii_iface_get_fd(wiidev);

	wiimoteglue_epoll_watch_wiimote(state->epfd, list_node);

	break;
      }
    }

    if (list_node->slot == NULL) {
      printf("Balance board detected, but all slots are filled.");
      printf("(WiimoteGlue still has it open and listening...)\n");

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

int close_wii_device(struct wii_device_list *dev) {
    printf("Controller %s (%s) has been removed.\n",dev->id,dev->bluetooth_addr);

    close(dev->fd);
    xwii_iface_unref(dev->device);
    if (dev->slot != NULL) {
      printf("(It was assigned slot #%d)\n",dev->slot->slot_number);
      if (dev->type == BALANCE) {
	dev->slot->has_board--;
      } else {
	dev->slot->has_wiimote--;
      }
    }
    if (dev->prev) {
      dev->prev->next = dev->next;
    }
    if (dev->next) {
      dev->next->prev = dev->prev;
    }

    if (dev->id != NULL) {
      free(dev->id);
    }
    if (dev->bluetooth_addr != NULL) {
      free(dev->bluetooth_addr);
    }

    free(dev);

    return 0;
}