#include <stdio.h>
#include <string.h>
#include "wiimoteglue.h"

/*This file has helper functions for dealing with
 *the virtual controller slots.
 */

struct virtual_controller* find_open_wiimote_slot(struct wiimoteglue_state *state) {
  int i;
  for (i = 1; i <= NUM_SLOTS; i++) {
    if (state->slots[i].has_wiimote == 0) {
      return &state->slots[i];
    }
  }
  return NULL;
}

struct virtual_controller* find_open_board_slot(struct wiimoteglue_state *state) {
  int i;
  for (i = 1; i <= NUM_SLOTS; i++) {
    if (state->slots[i].has_board == 0) {
      return &state->slots[i];
    }
  }
  return NULL;
}

struct virtual_controller* find_open_slot(struct wiimoteglue_state *state, int dev_type) {
  if (dev_type == BALANCE)
    return find_open_board_slot(state);

  return find_open_wiimote_slot(state);
}

int add_device_to_slot(struct wiimoteglue_state* state, struct wii_device_list *dev, struct virtual_controller *slot) {
  int ret = 0;
  if (dev == NULL)
    return -1;
  dev->slot = slot;
  if (slot == NULL) {
    ret += xwii_iface_set_led(dev->device,XWII_LED(1),0);
    ret += xwii_iface_set_led(dev->device,XWII_LED(2),1);
    ret += xwii_iface_set_led(dev->device,XWII_LED(3),0);
    ret += xwii_iface_set_led(dev->device,XWII_LED(4),1);
    if (ret < 0)
      printf("There were errors on setting the LEDs. Permissions?\n");
    return 0; /*This is actually okay!*/
  }
  if (dev->type == BALANCE) {
    slot->has_board++;
  } else {
    slot->has_wiimote++;
  }



  int num = slot->slot_number;


  if (num <= 0) {
    /*Keyboard/mouse slot. Let's just set a distinctive pattern.*/
    ret += xwii_iface_set_led(dev->device,XWII_LED(1),1);
    ret += xwii_iface_set_led(dev->device,XWII_LED(2),0);
    ret += xwii_iface_set_led(dev->device,XWII_LED(3),1);
    ret += xwii_iface_set_led(dev->device,XWII_LED(4),0);

  } else if (num > 4) {
    /*Future work: add some reasonable patterns here for higher nums?*/
    ret += xwii_iface_set_led(dev->device,XWII_LED(1),1);
    ret += xwii_iface_set_led(dev->device,XWII_LED(2),1);
    ret += xwii_iface_set_led(dev->device,XWII_LED(3),1);
    ret += xwii_iface_set_led(dev->device,XWII_LED(4),1);
  } else {
    int i;
    ret += xwii_iface_set_led(dev->device,XWII_LED(num),1);
    for (i = 1; i <= 4; i++) {
      if (i != num)
	ret += xwii_iface_set_led(dev->device,XWII_LED(i),0);
    }
  }

  if (ret < 0)
    printf("There were errors on setting the LEDs. Permissions?\n");

  compute_device_map(state,dev);

  return 0;
}

int remove_device_from_slot(struct wii_device_list *dev) {
  if (dev == NULL)
    return -1;

  if (dev->slot == NULL)
    return 0; /*This is okay!*/
  if (dev->type == BALANCE) {
    dev->slot->has_board--;
  } else {
    dev->slot->has_wiimote--;
  }

  return 0;
}

int change_slot_type(struct wiimoteglue_state* state, struct virtual_controller *slot,int type) {
  if (slot == NULL)
    return -1;
  if (slot->slot_number == 0) {
    return -1;
    /*We do not support switching slot 0's type.
     *It is assumed to always be a keyboard/mouse.
     */
  }

  if (type == SLOT_GAMEPAD) {
    slot->uinput_fd = slot->gamepad_fd;
    slot->type = SLOT_GAMEPAD;
    /*For now, we'll abuse the slot's personal map
     *to make the keyboard mode more useful.
     *This behavior will change.
     */
    printf("Switched slot's mapping to the gamepad mapping.\n");
    slot->slot_specific_mappings = NULL;
    return 0;
  }

  if (type == SLOT_KEYBOARDMOUSE) {
    slot->uinput_fd = slot->keyboardmouse_fd;
    slot->type = SLOT_KEYBOARDMOUSE;
    printf("Switched slot's mapping to the keyboardmouse mapping.\n");

    slot->slot_specific_mappings = state->slots[0].slot_specific_mappings;
    return 0;
  }

  return -2;

}

int set_slot_specific_mappings(struct virtual_controller *slot, struct mode_mappings *maps) {
  if (slot ==  NULL)
    return -1;

  slot->slot_specific_mappings = maps;

}

