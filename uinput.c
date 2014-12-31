#include <linux/uinput.h>
#include <linux/input.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "wiimoteglue.h"

/*Handles opening uinput and creating virtual gamepads.
 *Change the location /dev/uinput below if you need to.
 */

#define UINPUT_FILE "/dev/uinput"

int open_uinput_fd();


int wiimoteglue_uinput_init(int num_slots, struct virtual_controller slots[]) {
  int uinput_fd = 0;
  int i;
  for (i = 0; i < num_slots; i++) {
    uinput_fd = open_uinput_fd();
    if (uinput_fd < 0) {
      return -1;
    }
    slots[i].uinput_fd = uinput_fd;
  }

  return 0;

}

int wiimoteglue_uinput_close(int num_slots, struct virtual_controller slots[]) {
  int i;
  for (i = 0; i < num_slots; i++) {
    if (ioctl(slots[i].uinput_fd, UI_DEV_DESTROY) < 0) {
      //printf("Error destroying uinput device.\n");
    }
    close(slots[i].uinput_fd);
  }

  return 0;
}


int open_uinput_fd() {
  /* as far as I know, you can't change the reported event types
   * after creating the virtual device.
   * So we just create a gamepad with all of them, even if unmapped.
   */
  static int abs[] = { ABS_X, ABS_Y, ABS_RX, ABS_RY};
  static int key[] = { BTN_SOUTH, BTN_EAST, BTN_NORTH, BTN_WEST, BTN_SELECT, BTN_MODE, BTN_START, BTN_TL, BTN_TL2, BTN_TR, BTN_TR2, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT, BTN_DPAD_UP};
  struct uinput_user_dev uidev;
  int fd;
  int i;
  //TODO: handle other locations of uinput.
  //TODO: Read from uinput for rumble events?
  fd = open(UINPUT_FILE, O_WRONLY | O_NONBLOCK);
  if (fd < 0) {
    perror("open uinput");
    return -1;
  }
  memset(&uidev, 0, sizeof(uidev));
  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "WiimoteGlue Virtual Gamepad");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor = 0x1;
  uidev.id.product = 0x1;
  uidev.id.version = 1;

  ioctl(fd, UI_SET_EVBIT, EV_ABS);
  for (i = 0; i < 4; i++) {
    ioctl(fd, UI_SET_ABSBIT, abs[i]);
    uidev.absmin[abs[i]] = -32768;
    uidev.absmax[abs[i]] = 32768;
    uidev.absflat[abs[i]] = 4096;
  }

  ioctl(fd, UI_SET_EVBIT, EV_KEY);
  for (i = 0; i < 15; i++) {
    ioctl(fd, UI_SET_KEYBIT, key[i]);
  }

  write(fd, &uidev, sizeof(uidev));
  if (ioctl(fd, UI_DEV_CREATE) < 0)
    perror("uinput device creation");
  return fd;
}