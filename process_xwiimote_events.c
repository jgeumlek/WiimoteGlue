#include <stdio.h>
#include <errno.h>
#include <linux/input.h>
#include <string.h>
#include <unistd.h>

#include "wiimoteglue.h"

/* This file uses the xwiimote library
 * for interacting with wiimotes.
 * It handles opening and closing interfaces
 * and translating wiimote events to gamepad events.
 *
 */



void handle_key(int uinput_fd, int button_map[], struct xwii_event_key *ev);
void handle_nunchuk(int uinput_fd, struct event_map *map, struct xwii_event_abs ev[]);
void handle_classic(int uinput_fd, struct event_map *map, struct xwii_event_abs ev[]);
void handle_pro(int uinput_fd, struct event_map *map, struct xwii_event_abs ev[]);
void handle_accel(int uinput_fd, struct event_map *map, struct xwii_event_abs ev[]);
void handle_IR(int uinput_fd, struct event_map *map, struct xwii_event_abs ev[]);
void handle_balance(int uinput_fd, struct event_map *map, struct xwii_event_abs ev[]);

int wiimoteglue_update_wiimote_ifaces(struct wii_device_list *devlist) {
  struct wii_device_list* dev = devlist;

  if (dev == NULL) {
    return -1;
  }
  if (dev->device != NULL) {
    if (dev->map == NULL) {
      return -1;
    }

    if (dev->map->accel_active) {
      xwii_iface_open(dev->device,XWII_IFACE_ACCEL);
    } else {
      xwii_iface_close(dev->device,XWII_IFACE_ACCEL);
    }

    if (dev->map->IR_count) {
      xwii_iface_open(dev->device,XWII_IFACE_IR);
    } else {
      xwii_iface_close(dev->device,XWII_IFACE_IR);
    }
  }


  if (dev->next != NULL) {
    wiimoteglue_update_wiimote_ifaces(dev->next);
  }
  return 0;
}



int wiimoteglue_handle_wii_event(struct wiimoteglue_state *state, struct wii_device_list *dev) {
  struct xwii_event ev;
  if (dev == NULL) {
    return -1;
  }
  if (dev->slot == NULL) {
    /*Just ignore this event, but be sure to read it to clear it*/
    xwii_iface_dispatch(dev->device,&ev,sizeof(ev));
    return -1;
  }
  int ret = xwii_iface_dispatch(dev->device,&ev,sizeof(ev));
  if (ret < 0 && ret != -EAGAIN) {
    printf("Error reading controller. ");
    close_wii_device(dev);

  } else if (ret != -EAGAIN) {

    struct event_map* mapping;


    mapping = dev->map;

    switch(ev.type) {
    case XWII_EVENT_KEY:
    case XWII_EVENT_CLASSIC_CONTROLLER_KEY:
    case XWII_EVENT_PRO_CONTROLLER_KEY:
    case XWII_EVENT_NUNCHUK_KEY:
      handle_key(dev->slot->uinput_fd, mapping->button_map, &ev.v.key);
      break;
    case XWII_EVENT_CLASSIC_CONTROLLER_MOVE:
      handle_classic(dev->slot->uinput_fd, mapping, ev.v.abs);
      break;
    case XWII_EVENT_NUNCHUK_MOVE:
      handle_nunchuk(dev->slot->uinput_fd, mapping, ev.v.abs);
      break;
    case XWII_EVENT_PRO_CONTROLLER_MOVE:
      handle_pro(dev->slot->uinput_fd, mapping, ev.v.abs);
      break;
    case XWII_EVENT_ACCEL:
      handle_accel(dev->slot->uinput_fd, mapping, ev.v.abs);
      break;
    case XWII_EVENT_IR:
      handle_IR(dev->slot->uinput_fd, mapping, ev.v.abs);
      break;
    case XWII_EVENT_BALANCE_BOARD:
      handle_balance(dev->slot->uinput_fd, mapping, ev.v.abs);
      break;
    case XWII_EVENT_WATCH:
    case XWII_EVENT_GONE:
      xwii_iface_open(dev->device,XWII_IFACE_CLASSIC_CONTROLLER | XWII_IFACE_NUNCHUK | XWII_IFACE_PRO_CONTROLLER | XWII_IFACE_BALANCE_BOARD);
      dev->ifaces = xwii_iface_opened(dev->device);

      compute_device_map(state,dev);

      if (dev->map->accel_active) {
	xwii_iface_open(dev->device,XWII_IFACE_ACCEL);
      } else {
	xwii_iface_close(dev->device,XWII_IFACE_ACCEL);
      }

      if (dev->map->IR_count) {
	xwii_iface_open(dev->device,XWII_IFACE_IR);
      } else {
	xwii_iface_close(dev->device,XWII_IFACE_IR);
      }

      if (xwii_iface_available(dev->device) == 0 || dev->ifaces == 0) {
	//Controller removed.
	close_wii_device(dev);
      }
      break;

    }
  }

  return 0;
}

void handle_key(int uinput_fd, int button_map[], struct xwii_event_key *ev) {
  struct input_event out;
  memset(&out,0,sizeof(out));
  out.type = EV_KEY;
  out.code = button_map[ev->code];
  out.value = ev->state;
  write(uinput_fd, &out, sizeof(out));

  out.type  = EV_SYN;
  out.code = SYN_REPORT;
  out.value = 0;
  write(uinput_fd, &out, sizeof(out));
}

void handle_nunchuk(int uinput_fd, struct event_map *map, struct xwii_event_abs ev[]) {
  struct input_event out;
  memset(&out,0,sizeof(out));
  out.type = EV_ABS;
  out.code = map->stick_map[WG_N_X][AXIS_CODE];
  out.value = ev[0].x * map->stick_map[WG_N_X][AXIS_SCALE];
  write(uinput_fd, &out, sizeof(out));
  out.code = map->stick_map[WG_N_Y][AXIS_CODE];
  out.value = ev[0].y * map->stick_map[WG_N_Y][AXIS_SCALE];
  write(uinput_fd, &out, sizeof(out));

  out.type  = EV_SYN;
  out.code = SYN_REPORT;
  out.value = 0;
  write(uinput_fd, &out, sizeof(out));

  if (!map->accel_active) return; /*skip the accel values.*/

  out.type = EV_ABS;
  out.code = map->accel_map[WG_N_ACCELX][AXIS_CODE];
  out.value = ev[1].x * map->accel_map[WG_N_ACCELX][AXIS_SCALE];
  write(uinput_fd, &out, sizeof(out));
  out.code = map->accel_map[WG_N_ACCELY][AXIS_CODE];
  out.value = ev[1].y * map->accel_map[WG_N_ACCELY][AXIS_SCALE];
  write(uinput_fd, &out, sizeof(out));
  out.code = map->accel_map[WG_N_ACCELZ][AXIS_CODE];
  out.value = ev[1].z * map->accel_map[WG_N_ACCELZ][AXIS_SCALE];
  write(uinput_fd, &out, sizeof(out));

  out.type  = EV_SYN;
  out.code = SYN_REPORT;
  out.value = 0;
  write(uinput_fd, &out, sizeof(out));

}
void handle_classic(int uinput_fd, struct event_map *map, struct xwii_event_abs ev[]) {
  struct input_event out;
  memset(&out,0,sizeof(out));
  out.type = EV_ABS;
  out.code = map->stick_map[WG_LEFT_X][AXIS_CODE];
  out.value = ev[0].x * map->stick_map[WG_LEFT_X][AXIS_SCALE];
  write(uinput_fd, &out, sizeof(out));
  out.code = map->stick_map[WG_LEFT_Y][AXIS_CODE];
  out.value = ev[0].y * map->stick_map[WG_LEFT_Y][AXIS_SCALE];
  write(uinput_fd, &out, sizeof(out));
  out.code = map->stick_map[WG_RIGHT_X][AXIS_CODE];
  out.value = ev[1].x * map->stick_map[WG_RIGHT_X][AXIS_SCALE];
  write(uinput_fd, &out, sizeof(out));
  out.code = map->stick_map[WG_RIGHT_Y][AXIS_CODE];
  out.value = ev[1].y * map->stick_map[WG_RIGHT_Y][AXIS_SCALE];
  write(uinput_fd, &out, sizeof(out));

  out.type  = EV_SYN;
  out.code = SYN_REPORT;
  out.value = 0;
  write(uinput_fd, &out, sizeof(out));
  /*analog trigger values are ignored.
   *only the original classic controllers have them.
   */
}
void handle_pro(int uinput_fd, struct event_map *map, struct xwii_event_abs ev[]) {
  struct input_event out;
  memset(&out,0,sizeof(out));
  out.type = EV_ABS;
  out.code = map->stick_map[WG_LEFT_X][AXIS_CODE];
  out.value = ev[0].x * 32;
  write(uinput_fd, &out, sizeof(out));
  out.code = map->stick_map[WG_LEFT_Y][AXIS_CODE];
  out.value = ev[0].y * 32;
  write(uinput_fd, &out, sizeof(out));
  out.code = map->stick_map[WG_RIGHT_X][AXIS_CODE];
  out.value = ev[1].x * 32;
  write(uinput_fd, &out, sizeof(out));
  out.code = map->stick_map[WG_RIGHT_Y][AXIS_CODE];
  out.value = ev[1].y * 32;
  write(uinput_fd, &out, sizeof(out));

  /*Wii U Pro has different axis limits, hardcoded above to
   * scale from ~1024 to 32,768, the reported scale of
   * the virtual gamepads.
   * This means the Wii U Pro does not support inverting
   * the axes!
   */

  out.type  = EV_SYN;
  out.code = SYN_REPORT;
  out.value = 0;
  write(uinput_fd, &out, sizeof(out));

}
void handle_accel(int uinput_fd, struct event_map *map, struct xwii_event_abs ev[]) {
  struct input_event out;
  memset(&out,0,sizeof(out));
  out.type = EV_ABS;
  out.code = map->accel_map[WG_ACCELX][AXIS_CODE];
  out.value = ev[0].x * map->accel_map[WG_ACCELX][AXIS_SCALE];
  write(uinput_fd, &out, sizeof(out));
  out.code = map->accel_map[WG_ACCELY][AXIS_CODE];
  out.value = ev[0].y * map->accel_map[WG_ACCELY][AXIS_SCALE];
  write(uinput_fd, &out, sizeof(out));
  out.code = map->accel_map[WG_ACCELZ][AXIS_CODE];
  out.value = ev[0].z * map->accel_map[WG_ACCELZ][AXIS_SCALE];
  write(uinput_fd, &out, sizeof(out));



  out.type  = EV_SYN;
  out.code = SYN_REPORT;
  out.value = 0;
  write(uinput_fd, &out, sizeof(out));
}
void handle_IR(int uinput_fd, struct event_map *map, struct xwii_event_abs ev[]) {
  struct input_event out;
  memset(&out,0,sizeof(out));
  out.type = EV_ABS;
  int num = 0;
  float x = 1023;
  float y = 1023;
  int i;
  for (i = 0; i < 4; i++) {
    int ir_x = ev[i].x;
    int ir_y = ev[i].y;
    if (ir_x < x && ir_x != 1023 && ir_x > 1) {
      x = ir_x;
      y = ir_y;
      num++;
    }
  }
  if (num != 0) {
  out.code = map->IR_map[WG_IR_X][AXIS_CODE];
  out.value = (int) (-((x - 512) * map->IR_map[WG_IR_X][AXIS_SCALE]));
  write(uinput_fd, &out, sizeof(out));
  out.code = map->IR_map[WG_IR_Y][AXIS_CODE];
  out.value = (int) (((y - 380) * map->IR_map[WG_IR_Y][AXIS_SCALE]));
  write(uinput_fd, &out, sizeof(out));
  }

  out.type  = EV_SYN;
  out.code = SYN_REPORT;
  out.value = 0;
  write(uinput_fd, &out, sizeof(out));
}
void handle_balance(int uinput_fd, struct event_map *map, struct xwii_event_abs ev[]) {
  struct input_event out;
  memset(&out,0,sizeof(out));
  out.type = EV_ABS;
  int total = ev[0].x + ev[1].x + ev[2].x + ev[3].x;
  int left = ev[2].x + ev[3].x;
  int right = total - left;
  int front = ev[0].x + ev[2].x;
  int back = total - front;

  /*Beware: lots of constants from experimentation below!*/

  float x = (right - left)/((total + 1)*0.7f);
  float y = (back - front)/((total + 1)*0.7f);

  if (total < 125) {
    x = 0;
    y = 0;
  }

  out.code = map->balance_map[WG_BAL_X][AXIS_CODE];
  out.value = (int)(x * map->balance_map[WG_BAL_X][AXIS_SCALE]);
  write(uinput_fd, &out, sizeof(out));

  out.code = map->balance_map[WG_BAL_Y][AXIS_CODE];
  out.value = (int)(y * map->balance_map[WG_BAL_Y][AXIS_SCALE]);
  write(uinput_fd, &out, sizeof(out));


  out.type  = EV_SYN;
  out.code = SYN_REPORT;
  out.value = 0;
  write(uinput_fd, &out, sizeof(out));
}



