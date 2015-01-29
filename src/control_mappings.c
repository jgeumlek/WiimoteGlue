#include <linux/input.h>
#include <string.h>
#include "wiimoteglue.h"
#include <stdio.h>


/*This file handles the data structures
 *used for storing control mappings.
 *
 *If you are looking for the code that
 *actually translates wiimote events,
 *look in process_xwiimote_events.c
 */

void mappings_ref(struct mode_mappings *maps);
void mappings_unref(struct mode_mappings *maps);

int compute_device_map(struct wiimoteglue_state* state, struct wii_device* dev) {
  if (dev == NULL)
    return -1;
  struct mode_mappings* maps = &state->head_map.maps;

  if (dev->slot != NULL && dev->slot->slot_specific_mappings != NULL) {
    maps = dev->slot->slot_specific_mappings;
  }

  if (dev->dev_specific_mappings != NULL)
    maps = dev->dev_specific_mappings;

  if (dev->ifaces & XWII_IFACE_NUNCHUK) {
    dev->map = &maps->mode_nunchuk;
  } else if (dev->ifaces & (XWII_IFACE_CLASSIC_CONTROLLER | XWII_IFACE_PRO_CONTROLLER)) {
    dev->map = &maps->mode_classic;
  } else {
    dev->map = &maps->mode_no_ext;
  }

  wiimoteglue_update_wiimote_ifaces(dev);

  return 0;

}

int wiimoteglue_compute_all_device_maps(struct wiimoteglue_state* state, struct wii_device_list *devlist) {
  if (devlist == NULL) {
    return -1;
  }

  struct wii_device_list* list_node = devlist->next;

  while (list_node != devlist && list_node != NULL) {
    struct wii_device* dev = list_node->dev;


    if (dev != NULL) {
      compute_device_map(state,dev);
    }



    list_node = list_node->next;
  }
  return 0;
}

int mode_name_check(char* mode_name) {
  if (mode_name == NULL)
    return -1;

  if (strcmp(mode_name,"all") == 0)
    return 0;
  if (strcmp(mode_name,"wiimote") == 0)
    return 1;
  if (strcmp(mode_name,"nunchuk") == 0)
    return 2;
  if (strcmp(mode_name,"classic") == 0)
    return 3;

  return -2;
}

struct mode_mappings* lookup_mappings(struct wiimoteglue_state* state, char* map_name) {
  if (map_name == NULL)
    return NULL;

  if (strncmp(map_name,state->head_map.maps.name,WG_MAX_NAME_SIZE) == 0)
    return &state->head_map.maps;
  struct map_list *list_node;
  list_node = state->head_map.next;
  for (; *KEEP_LOOPING && list_node != NULL && list_node != &state->head_map; list_node = list_node->next) {
    if (strncmp(map_name,list_node->maps.name,WG_MAX_NAME_SIZE) == 0)
      return &list_node->maps;
  }

  return NULL;
}

int copy_mappings(struct mode_mappings *dest, struct mode_mappings *src) {
 char *name = dest->name;
 memcpy(dest,src,sizeof(struct mode_mappings));
 dest->name = name;
 return 0;
}

struct map_list* create_mappings(struct wiimoteglue_state *state, char *name) {
  if (name == NULL)
    return NULL;

  struct map_list *new_map = calloc(1,sizeof(struct map_list));
  new_map->maps.name = calloc(WG_MAX_NAME_SIZE,sizeof(char));
  strncpy(new_map->maps.name,name,WG_MAX_NAME_SIZE);

  new_map->next = &state->head_map;
  new_map->prev = state->head_map.prev;

  state->head_map.prev = new_map;
  new_map->prev->next = new_map;

  mappings_ref(&new_map->maps);

  init_blank_mappings(&new_map->maps);

  return new_map;

}

struct map_list * get_map_list_container(struct mode_mappings *maps) {
  /*ugly code for a quick
    *and dirty container_of
    */
  struct map_list temp;
  const int offset = (void*)(&temp.maps) - (void*)(&temp);
  void *list_entry = (void*)maps - offset;

  return (struct map_list*)list_entry;
}

int forget_mapping(struct wiimoteglue_state *state, struct mode_mappings *maps) {
  if (maps == &state->head_map.maps || maps == state->slots[0].slot_specific_mappings) {
    printf("Cannot forget default mapping \"%s\"\n",maps->name);
    return -1;
  }

  /*cut it out of the mapping list,
   *but don't delete it if some slot
   *or device is still using it.
   */
  struct map_list *list_node = get_map_list_container(maps);
  if (list_node->next)
    list_node->next->prev = list_node->prev;
  if (list_node->prev)
    list_node->prev->next = list_node->next;

  list_node->next = NULL;
  list_node->prev = NULL;

  mappings_unref(maps);

  return 0;

}


void mappings_ref(struct mode_mappings *maps) {
  maps->reference_count++;
}

void mappings_unref(struct mode_mappings *maps) {
  maps->reference_count--;

  if (maps->reference_count == 0) {

    printf("mapping \"%s\" is unreferenced and deleted.\n",maps->name);
    free(maps->name);
    free(get_map_list_container(maps));
  }

}


int init_keyboardmouse_mappings(struct mode_mappings *maps) {


  /* Default wiimote only mapping.
   * Mainly useful for controlling media playback
   */
  int *button_map = maps->mode_no_ext.button_map;
  struct event_map *map = &maps->mode_no_ext;
  memset(map, 0, sizeof(maps->mode_no_ext));
  button_map[XWII_KEY_LEFT] = KEY_LEFT;
  button_map[XWII_KEY_RIGHT] = KEY_RIGHT;
  button_map[XWII_KEY_UP] = KEY_UP;
  button_map[XWII_KEY_DOWN] = KEY_DOWN;
  button_map[XWII_KEY_A] = BTN_LEFT;
  button_map[XWII_KEY_B] = BTN_RIGHT;
  button_map[XWII_KEY_PLUS] = KEY_VOLUMEUP;
  button_map[XWII_KEY_MINUS] = KEY_VOLUMEDOWN;
  button_map[XWII_KEY_HOME] = KEY_ESC;
  button_map[XWII_KEY_ONE] = BTN_MIDDLE;
  button_map[XWII_KEY_TWO] = KEY_MUTE;
  button_map[XWII_KEY_X] = NO_MAP;
  button_map[XWII_KEY_Y] = NO_MAP;
  button_map[XWII_KEY_TL] = NO_MAP;
  button_map[XWII_KEY_TR] = NO_MAP;
  button_map[XWII_KEY_ZL] = NO_MAP;
  button_map[XWII_KEY_ZR] = NO_MAP;
  button_map[XWII_KEY_THUMBL] = NO_MAP;
  button_map[XWII_KEY_THUMBR] = NO_MAP;
  button_map[XWII_KEY_C] = NO_MAP;
  button_map[XWII_KEY_Z] = NO_MAP;


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
    {ABS_X, ABS_LIMIT/400},/*ir_x*/
    {ABS_Y, ABS_LIMIT/300},/*ir_y*/
  };
  memcpy(map->IR_map, no_ext_IR_map, sizeof(no_ext_IR_map));

  /*Default mapping with nunchuk.
   *I don't know what keyboard/mouse
   *mappings are reasonable.
   */
  button_map = maps->mode_nunchuk.button_map;
  map = &maps->mode_nunchuk;
  memset(map, 0, sizeof(maps->mode_nunchuk));
  button_map[XWII_KEY_LEFT] = KEY_LEFT;
  button_map[XWII_KEY_RIGHT] = KEY_RIGHT;
  button_map[XWII_KEY_UP] = KEY_UP;
  button_map[XWII_KEY_DOWN] = KEY_DOWN;
  button_map[XWII_KEY_A] = BTN_LEFT;
  button_map[XWII_KEY_B] = BTN_RIGHT;
  button_map[XWII_KEY_PLUS] = KEY_VOLUMEUP;
  button_map[XWII_KEY_MINUS] = KEY_VOLUMEDOWN;
  button_map[XWII_KEY_HOME] = KEY_ESC;
  button_map[XWII_KEY_ONE] = BTN_MIDDLE;
  button_map[XWII_KEY_TWO] = KEY_MUTE;
  button_map[XWII_KEY_X] = NO_MAP;
  button_map[XWII_KEY_Y] = NO_MAP;
  button_map[XWII_KEY_TL] = NO_MAP;
  button_map[XWII_KEY_TR] = NO_MAP;
  button_map[XWII_KEY_ZL] = NO_MAP;
  button_map[XWII_KEY_ZR] = NO_MAP;
  button_map[XWII_KEY_THUMBL] = NO_MAP;
  button_map[XWII_KEY_THUMBR] = NO_MAP;
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
    {ABS_X, ABS_LIMIT/400},/*ir_x*/
    {ABS_Y, ABS_LIMIT/300},/*ir_y*/
  };
  memcpy(map->IR_map, nunchuk_IR_map, sizeof(nunchuk_IR_map));


  /*Mapping for Classic-style controllers.
   *I don't know what keyboard/mouse
   *mappings are reasonable.
   */
  button_map = maps->mode_classic.button_map;
  map = &maps->mode_classic;
  memset(map, 0, sizeof(maps->mode_classic));
  button_map[XWII_KEY_LEFT] = KEY_LEFT;
  button_map[XWII_KEY_RIGHT] = KEY_RIGHT;
  button_map[XWII_KEY_UP] = KEY_UP;
  button_map[XWII_KEY_DOWN] = KEY_DOWN;
  button_map[XWII_KEY_A] = BTN_LEFT;
  button_map[XWII_KEY_B] = BTN_RIGHT;
  button_map[XWII_KEY_PLUS] = KEY_VOLUMEUP;
  button_map[XWII_KEY_MINUS] = KEY_VOLUMEDOWN;
  button_map[XWII_KEY_HOME] = KEY_ESC;
  button_map[XWII_KEY_ONE] = BTN_MIDDLE;
  button_map[XWII_KEY_TWO] = KEY_MUTE;
  button_map[XWII_KEY_X] = BTN_NORTH;
  button_map[XWII_KEY_Y] = BTN_WEST;
  button_map[XWII_KEY_TL] = BTN_TL;
  button_map[XWII_KEY_TR] = BTN_TR;
  button_map[XWII_KEY_ZL] = BTN_TL2;
  button_map[XWII_KEY_ZR] = BTN_TR2;
  button_map[XWII_KEY_THUMBL] = BTN_THUMBL;
  button_map[XWII_KEY_THUMBR] = BTN_THUMBR;
  button_map[XWII_KEY_C] = NO_MAP;
  button_map[XWII_KEY_Z] = NO_MAP;

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
    {ABS_X, ABS_LIMIT/400},/*ir_x*/
    {ABS_Y, ABS_LIMIT/300},/*ir_y*/
  };
  memcpy(map->IR_map, classic_IR_map, sizeof(classic_IR_map));



  return 0;

}

int init_gamepad_mappings(struct mode_mappings *maps, char *name) {


  /* Default wiimote only mapping.
   * Designed for playing simple games, with the wiimote
   * held horizontally. (DPAD on the left.)
   */
  int *button_map = maps->mode_no_ext.button_map;
  struct event_map *map = &maps->mode_no_ext;
  memset(map, 0, sizeof(maps->mode_no_ext));
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
  button_map[XWII_KEY_X] = NO_MAP;
  button_map[XWII_KEY_Y] = NO_MAP;
  button_map[XWII_KEY_TL] = NO_MAP;
  button_map[XWII_KEY_TR] = NO_MAP;
  button_map[XWII_KEY_ZL] = NO_MAP;
  button_map[XWII_KEY_ZR] = NO_MAP;
  button_map[XWII_KEY_THUMBL] = NO_MAP;
  button_map[XWII_KEY_THUMBR] = NO_MAP;
  button_map[XWII_KEY_C] = NO_MAP;
  button_map[XWII_KEY_Z] = NO_MAP;


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
  button_map = maps->mode_nunchuk.button_map;
  map = &maps->mode_nunchuk;
  memset(map, 0, sizeof(maps->mode_nunchuk));
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
  button_map[XWII_KEY_X] = NO_MAP;
  button_map[XWII_KEY_Y] = NO_MAP;
  button_map[XWII_KEY_TL] = NO_MAP;
  button_map[XWII_KEY_TR] = NO_MAP;
  button_map[XWII_KEY_ZL] = NO_MAP;
  button_map[XWII_KEY_ZR] = NO_MAP;
  button_map[XWII_KEY_THUMBL] = NO_MAP;
  button_map[XWII_KEY_THUMBR] = NO_MAP;
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


  /*Mapping for Classic-style controllers.
   *Wii Classic Controllers and Wii U Pro
   *controllers use this. It matches the
   *Linux gamepad layout.
   */
  button_map = maps->mode_classic.button_map;
  map = &maps->mode_classic;
  memset(map, 0, sizeof(maps->mode_classic));
  button_map[XWII_KEY_LEFT] = BTN_DPAD_LEFT;
  button_map[XWII_KEY_RIGHT] = BTN_DPAD_RIGHT;
  button_map[XWII_KEY_UP] = BTN_DPAD_UP;
  button_map[XWII_KEY_DOWN] = BTN_DPAD_DOWN;
  button_map[XWII_KEY_A] = BTN_EAST;
  button_map[XWII_KEY_B] = BTN_SOUTH;
  button_map[XWII_KEY_PLUS] = BTN_START;
  button_map[XWII_KEY_MINUS] = BTN_SELECT;
  button_map[XWII_KEY_HOME] = BTN_MODE;
  button_map[XWII_KEY_ONE] = NO_MAP;
  button_map[XWII_KEY_TWO] =NO_MAP;
  button_map[XWII_KEY_X] = BTN_NORTH;
  button_map[XWII_KEY_Y] = BTN_WEST;
  button_map[XWII_KEY_TL] = BTN_TL;
  button_map[XWII_KEY_TR] = BTN_TR;
  button_map[XWII_KEY_ZL] = BTN_TL2;
  button_map[XWII_KEY_ZR] = BTN_TR2;
  button_map[XWII_KEY_THUMBL] = BTN_THUMBL;
  button_map[XWII_KEY_THUMBR] = BTN_THUMBR;
  button_map[XWII_KEY_C] = NO_MAP;
  button_map[XWII_KEY_Z] = NO_MAP;

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

  maps->name = name;

  return 0;

}

int init_blank_mappings(struct mode_mappings *maps) {


  /* Default wiimote only mapping.
   * This mapping is blank;
   * everthing is not mapped.
   */
  int *button_map = maps->mode_no_ext.button_map;
  struct event_map *map = &maps->mode_no_ext;
  memset(map, 0, sizeof(maps->mode_no_ext));
  button_map[XWII_KEY_LEFT] = NO_MAP;
  button_map[XWII_KEY_RIGHT] = NO_MAP;
  button_map[XWII_KEY_UP] = NO_MAP;
  button_map[XWII_KEY_DOWN] = NO_MAP;
  button_map[XWII_KEY_A] = NO_MAP;
  button_map[XWII_KEY_B] = NO_MAP;
  button_map[XWII_KEY_PLUS] = NO_MAP;
  button_map[XWII_KEY_MINUS] = NO_MAP;
  button_map[XWII_KEY_HOME] = NO_MAP;
  button_map[XWII_KEY_ONE] = NO_MAP;
  button_map[XWII_KEY_TWO] = NO_MAP;
  button_map[XWII_KEY_X] = NO_MAP;
  button_map[XWII_KEY_Y] = NO_MAP;
  button_map[XWII_KEY_TL] = NO_MAP;
  button_map[XWII_KEY_TR] = NO_MAP;
  button_map[XWII_KEY_ZL] = NO_MAP;
  button_map[XWII_KEY_ZR] = NO_MAP;
  button_map[XWII_KEY_THUMBL] = NO_MAP;
  button_map[XWII_KEY_THUMBR] = NO_MAP;
  button_map[XWII_KEY_C] = NO_MAP;
  button_map[XWII_KEY_Z] = NO_MAP;


  int no_ext_accel_map[6][2] = {
    {NO_MAP, -TILT_SCALE}, /*accelx*/
    {NO_MAP, -TILT_SCALE}, /*accely*/
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
    {NO_MAP, ABS_LIMIT},/*bal_x*/
    {NO_MAP, ABS_LIMIT},/*bal_y*/
  };
  memcpy(map->balance_map, no_ext_balance_map, sizeof(no_ext_balance_map));

  int no_ext_IR_map[2][2] = {
    {NO_MAP, ABS_LIMIT/400},/*ir_x*/
    {NO_MAP, ABS_LIMIT/300},/*ir_y*/
  };
  memcpy(map->IR_map, no_ext_IR_map, sizeof(no_ext_IR_map));

  /*Default mapping for nunchuk
   *and classic are the same:
   *all empty
   */
  memcpy(&maps->mode_nunchuk,&maps->mode_no_ext,sizeof(struct event_map));
  memcpy(&maps->mode_classic,&maps->mode_no_ext,sizeof(struct event_map));





  return 0;

}