#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <libudev.h>
#include <signal.h>

#include <xwiimote.h>

#include "wiimoteglue.h"

struct wiimoteglue_state* gstate;
void signal_handler(int signum) {
  printf("Signal received, attempting to clean up.\n");
  gstate->keep_looping = 0;
}


int init_mapping(struct wiimoteglue_state *state);

int main(int argc, char *argv[]) {
  struct udev *udev;
  struct wiimoteglue_state state;
  gstate = &state;
  memset(&state, 0, sizeof(state));
  int monitor_fd;
  int epfd;
  int ret;
  //Ask how many fake controllers to make... (ASSUME 4 FOR NOW)
  printf("Creating %d uinput devices...",NUM_SLOTS);
  int i;
  state.virtual_keyboardmouse_fd = wiimoteglue_open_uinput_keyboardmouse_fd();
  for (i = 0; i < NUM_SLOTS; i++) {
    state.slots[i].slot_number = i+1;
    state.slots[i].has_wiimote = 0;
    state.slots[i].has_board = 0;
  }

  ret = wiimoteglue_uinput_init(NUM_SLOTS, state.slots, state.virtual_keyboardmouse_fd);

  if (ret) {
    printf("\nError in creating uinput devices, aborting.\nCheck the permissions.\n");
    wiimoteglue_uinput_close(2,state.slots);
    return -1;
  } else {
    printf(" okay.\n");
  }

  init_mapping(&state);



  //Start a monitor? (ASSUME YES FOR NOW)
  printf("Starting udev monitor...");
  ret = wiimoteglue_udev_monitor_init(&udev, &state.monitor, &monitor_fd);
  if (ret) {
    printf("\nError in creating udev monitor. No additional controllers will be detected.");
  } else {
    printf(" okay.\n");
  }

  wiimoteglue_epoll_init(&epfd);
  wiimoteglue_epoll_watch_monitor(epfd, monitor_fd, state.monitor);
  wiimoteglue_epoll_watch_stdin(epfd);

  state.epfd = epfd;

  //Check for existing wiimotes? (ASSUME NO FOR NOW)
  //Existing wiimotes mean our virtual slots won't
  //have earlier device nodes, so programs expecting
  //controllers may grab the wiimotes directly
  //rather than our virtual ones.

  printf("Any currently connected wiimotes will be ignored.\n");

  //Start forwarding input events.

  //Process user input.
  state.keep_looping = 1;
  state.dev_count = 0;

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGHUP, signal_handler);
  printf("WiimoteGlue is running. Enter \"help\" for available commands.\n");
  wiimoteglue_epoll_loop(epfd, &state);



  printf("Shutting down...\n");
  wiimoteglue_uinput_close(NUM_SLOTS, state.slots);
  close(state.virtual_keyboardmouse_fd);

  struct wii_device_list *list_node = state.devlist.next;
  for (; list_node != NULL; ) {
    xwii_iface_unref(list_node->device);
    struct wii_device_list *next = list_node->next;
    if (list_node->id != NULL) {
      free(list_node->id);
    }
    if (list_node->bluetooth_addr != NULL) {
      free(list_node->bluetooth_addr);
    }
    free(list_node);
    list_node = next;
  }

  udev_monitor_unref(state.monitor);
  udev_unref(udev);

  return 0;
}

int init_mapping(struct wiimoteglue_state *state) {


  /* Default wiimote only mapping.
   * Designed for playing simple games, with the wiimote
   * held horizontally. (DPAD on the left.)
   */
  int *button_map = state->mode_no_ext.button_map;
  struct event_map *map = &state->mode_no_ext;
  memset(map, 0, sizeof(state->mode_no_ext));
  button_map[XWII_KEY_LEFT] = BTN_DPAD_DOWN;
  button_map[XWII_KEY_RIGHT] = BTN_DPAD_UP;
  button_map[XWII_KEY_UP] = BTN_DPAD_LEFT;
  button_map[XWII_KEY_DOWN] = BTN_DPAD_RIGHT;
  button_map[XWII_KEY_A] = BTN_NORTH;
  button_map[XWII_KEY_B] = BTN_WEST;
  button_map[XWII_KEY_PLUS] = BTN_START;
  button_map[XWII_KEY_MINUS] = BTN_SELECT;
  button_map[XWII_KEY_HOME] = BTN_MODE;
  button_map[XWII_KEY_ONE] = BTN_SOUTH;
  button_map[XWII_KEY_TWO] = BTN_EAST;
  button_map[XWII_KEY_X] = 0;
  button_map[XWII_KEY_Y] = 0;
  button_map[XWII_KEY_TL] = 0;
  button_map[XWII_KEY_TR] = 0;
  button_map[XWII_KEY_ZL] = 0;
  button_map[XWII_KEY_ZR] = 0;
  button_map[XWII_KEY_THUMBL] = 0;
  button_map[XWII_KEY_THUMBR] = 0;
  button_map[XWII_KEY_C] = 0;
  button_map[XWII_KEY_Z] = 0;


  int no_ext_accel_map[6][2] = {
    {ABS_Y, -TILT_SCALE}, /*accelx*/
    {ABS_X, -TILT_SCALE}, /*accely*/
    {NO_MAP, TILT_SCALE},/*accelz*/
    {NO_MAP, TILT_SCALE},/*n_accelx*/
    {NO_MAP, TILT_SCALE},/*n_accely*/
    {NO_MAP, TILT_SCALE} /*n_accelz*/
  };
  memcpy(map->accel_map,no_ext_accel_map,sizeof(no_ext_accel_map));

  map->accel_active = 0;

  int no_ext_stick_map[6][2] = {
    {NO_MAP, CLASSIC_SCALE},/*left_x*/
    {NO_MAP, -CLASSIC_SCALE},/*left_y*/
    {NO_MAP, CLASSIC_SCALE},/*right_x*/
    {NO_MAP, -CLASSIC_SCALE},/*right_y*/
    {NO_MAP, NUNCHUK_SCALE},/*n_x*/
    {NO_MAP, -NUNCHUK_SCALE},/*n_y*/
  };
  memcpy(map->stick_map, no_ext_stick_map, sizeof(no_ext_stick_map));

  int no_ext_balance_map[6][2] = {
    {NO_MAP, ABS_LIMIT/600},/*bal_fl*/
    {NO_MAP, ABS_LIMIT/600},/*bal_fr*/
    {NO_MAP, ABS_LIMIT/600},/*bal_bl*/
    {NO_MAP, ABS_LIMIT/600},/*bal_br*/
    {ABS_X, ABS_LIMIT},/*bal_x*/
    {ABS_Y, ABS_LIMIT},/*bal_y*/
  };
  memcpy(map->balance_map, no_ext_balance_map, sizeof(no_ext_balance_map));

  int no_ext_IR_map[2][2] = {
    {ABS_RX, ABS_LIMIT/400},/*ir_x*/
    {ABS_RY, ABS_LIMIT/300},/*ir_y*/
  };
  memcpy(map->IR_map, no_ext_IR_map, sizeof(no_ext_IR_map));

  /*Default mapping with nunchuk.
   *No acceleration, just buttons.
   *
   *
   */
  button_map = state->mode_nunchuk.button_map;
  map = &state->mode_nunchuk;
  memset(&state->mode_nunchuk, 0, sizeof(state->mode_nunchuk));
  button_map[XWII_KEY_LEFT] = BTN_DPAD_LEFT;
  button_map[XWII_KEY_RIGHT] = BTN_DPAD_RIGHT;
  button_map[XWII_KEY_UP] = BTN_DPAD_UP;
  button_map[XWII_KEY_DOWN] = BTN_DPAD_DOWN;
  button_map[XWII_KEY_A] = BTN_SOUTH;
  button_map[XWII_KEY_B] = BTN_TR2;
  button_map[XWII_KEY_PLUS] = BTN_START;
  button_map[XWII_KEY_MINUS] = BTN_SELECT;
  button_map[XWII_KEY_HOME] = BTN_MODE;
  button_map[XWII_KEY_ONE] = BTN_EAST;
  button_map[XWII_KEY_TWO] = BTN_TR;
  button_map[XWII_KEY_X] = 0;
  button_map[XWII_KEY_Y] = 0;
  button_map[XWII_KEY_TL] = 0;
  button_map[XWII_KEY_TR] = 0;
  button_map[XWII_KEY_ZL] = 0;
  button_map[XWII_KEY_ZR] = 0;
  button_map[XWII_KEY_THUMBL] = 0;
  button_map[XWII_KEY_THUMBR] = 0;
  button_map[XWII_KEY_C] = BTN_TL;
  button_map[XWII_KEY_Z] = BTN_TL2;

  int nunchuk_accel_map[6][2] = {
    {ABS_RX, TILT_SCALE}, /*accelx*/
    {ABS_RY, TILT_SCALE}, /*accely*/
    {NO_MAP, TILT_SCALE},/*accelz*/
    {NO_MAP, TILT_SCALE},/*n_accelx*/
    {NO_MAP, TILT_SCALE},/*n_accely*/
    {NO_MAP, TILT_SCALE} /*n_accelz*/
  };
  memcpy(map->accel_map,nunchuk_accel_map,sizeof(nunchuk_accel_map));

  map->accel_active = 0;

  int nunchuk_stick_map[6][2] = {
    {NO_MAP, CLASSIC_SCALE},/*left_x*/
    {NO_MAP, -CLASSIC_SCALE},/*left_y*/
    {NO_MAP, CLASSIC_SCALE},/*right_x*/
    {NO_MAP, -CLASSIC_SCALE},/*right_y*/
    {ABS_X, NUNCHUK_SCALE},/*n_x*/
    {ABS_Y, -NUNCHUK_SCALE},/*n_y*/
  };
  memcpy(map->stick_map, nunchuk_stick_map, sizeof(nunchuk_stick_map));

  int nunchuk_balance_map[6][2] = {
    {NO_MAP, ABS_LIMIT/600},/*bal_fl*/
    {NO_MAP, ABS_LIMIT/600},/*bal_fr*/
    {NO_MAP, ABS_LIMIT/600},/*bal_bl*/
    {NO_MAP, ABS_LIMIT/600},/*bal_br*/
    {ABS_X, ABS_LIMIT},/*bal_x*/
    {ABS_Y, ABS_LIMIT},/*bal_y*/
  };
  memcpy(map->balance_map, nunchuk_balance_map, sizeof(nunchuk_balance_map));

  int nunchuk_IR_map[2][2] = {
    {ABS_RX, ABS_LIMIT/400},/*ir_x*/
    {ABS_RY, ABS_LIMIT/300},/*ir_y*/
  };
  memcpy(map->IR_map, nunchuk_IR_map, sizeof(nunchuk_IR_map));

  button_map = state->mode_classic.button_map;
  map = &state->mode_classic;
  memset(&state->mode_classic, 0, sizeof(state->mode_classic));
  button_map[XWII_KEY_LEFT] = BTN_DPAD_LEFT;
  button_map[XWII_KEY_RIGHT] = BTN_DPAD_RIGHT;
  button_map[XWII_KEY_UP] = BTN_DPAD_UP;
  button_map[XWII_KEY_DOWN] = BTN_DPAD_DOWN;
  button_map[XWII_KEY_A] = BTN_EAST;
  button_map[XWII_KEY_B] = BTN_SOUTH;
  button_map[XWII_KEY_PLUS] = BTN_START;
  button_map[XWII_KEY_MINUS] = BTN_SELECT;
  button_map[XWII_KEY_HOME] = BTN_MODE;
  button_map[XWII_KEY_ONE] = 0;
  button_map[XWII_KEY_TWO] = 0;
  button_map[XWII_KEY_X] = BTN_NORTH;
  button_map[XWII_KEY_Y] = BTN_WEST;
  button_map[XWII_KEY_TL] = BTN_TL;
  button_map[XWII_KEY_TR] = BTN_TR;
  button_map[XWII_KEY_ZL] = BTN_TL2;
  button_map[XWII_KEY_ZR] = BTN_TR2;
  button_map[XWII_KEY_THUMBL] = BTN_THUMBL;
  button_map[XWII_KEY_THUMBR] = BTN_THUMBR;
  button_map[XWII_KEY_C] = 0;
  button_map[XWII_KEY_Z] = 0;

  int classic_accel_map[6][2] = {
    {ABS_RX, TILT_SCALE}, /*accelx*/
    {ABS_RY, TILT_SCALE}, /*accely*/
    {NO_MAP, TILT_SCALE},/*accelz*/
    {ABS_X, TILT_SCALE},/*n_accelx*/
    {ABS_Y, TILT_SCALE},/*n_accely*/
    {NO_MAP, TILT_SCALE} /*n_accelz*/
  };
  memcpy(map->accel_map,classic_accel_map,sizeof(classic_accel_map));

  int classic_stick_map[6][2] = {
    {ABS_X, CLASSIC_SCALE},/*left_x*/
    {ABS_Y, -CLASSIC_SCALE},/*left_y*/
    {ABS_RX, CLASSIC_SCALE},/*right_x*/
    {ABS_RY, -CLASSIC_SCALE},/*right_y*/
    {NO_MAP, NUNCHUK_SCALE},/*n_x*/
    {NO_MAP, -NUNCHUK_SCALE},/*n_y*/
  };
  memcpy(map->stick_map, classic_stick_map, sizeof(classic_stick_map));

  int classic_balance_map[6][2] = {
    {NO_MAP, ABS_LIMIT/600},/*bal_fl*/
    {NO_MAP, ABS_LIMIT/600},/*bal_fr*/
    {NO_MAP, ABS_LIMIT/600},/*bal_bl*/
    {NO_MAP, ABS_LIMIT/600},/*bal_br*/
    {ABS_X, ABS_LIMIT},/*bal_x*/
    {ABS_Y, ABS_LIMIT},/*bal_y*/
  };
  memcpy(map->balance_map, classic_balance_map, sizeof(classic_balance_map));

  int classic_IR_map[2][2] = {
    {ABS_RX, ABS_LIMIT/400},/*ir_x*/
    {ABS_RY, ABS_LIMIT/300},/*ir_y*/
  };
  memcpy(map->IR_map, classic_IR_map, sizeof(classic_IR_map));

  return 0;

}

