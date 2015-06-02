#include <stdio.h>
#include <string.h>
#include "wiimoteglue.h"

/*This file has helper functions for dealing with
 *the virtual controller slots.
 */

#define NUM_SLOT_PATTERNS 9
bool slot_leds[NUM_SLOT_PATTERNS+1][4] = {
  {0,1,1,0}, /*keyboardmouse slot*/
  {1,0,0,0}, /*slot 1... etc. */
  {0,1,0,0},
  {0,0,1,0},
  {0,0,0,1},
  {1,0,0,1},
  {0,1,0,1},
  {0,0,1,1},
  {1,0,1,1},
  {0,1,1,1}
};


bool no_slot_leds[4] = {1,0,1,0};
bool slot_overflow_leds[4] = {1,1,1,1};

struct virtual_controller* find_open_wiimote_slot(struct wiimoteglue_state *state) {
  int i;
  for (i = 1; i <= state->num_slots; i++) {
    if (state->slots[i].has_wiimote == 0) {
      return &state->slots[i];
    }
  }
  return NULL;
}

struct virtual_controller* find_open_board_slot(struct wiimoteglue_state *state) {
  int i;
  for (i = 1; i <= state->num_slots; i++) {
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

int add_device_to_slot(struct wiimoteglue_state* state, struct wii_device *dev, struct virtual_controller *slot) {
  int ret = 0;
  /*We don't support a device being in two slots at once.*/
  if (dev == NULL || dev->slot != NULL || dev->slot_list == NULL)
    return -1;

  if (slot == NULL) {
    ret = set_led_state(state,dev,no_slot_leds);
    if (ret < 0 && ret != -2)
      printf("There were errors on setting the LEDs. Permissions?\n");

    return 0; /*This is actually okay!*/
  }





  dev->slot_list->next = slot->dev_list.next;
  dev->slot_list->prev = &slot->dev_list;

  if (slot->dev_list.next != NULL) {
    slot->dev_list.next->prev = dev->slot_list;
  }

  slot->dev_list.next = dev->slot_list;

  dev->slot = slot;



  if (dev->type == BALANCE) {
    slot->has_board++;
  } else {
    slot->has_wiimote++;
  }



  int num = slot->slot_number;


  if (num <= 0) {
    /*Keyboard/mouse slot. Let's just set a distinctive pattern.*/
   ret = set_led_state(state,dev,slot_leds[0]);

  } else if (num > NUM_SLOT_PATTERNS) {
    /*Future work: add some reasonable patterns here for higher nums?*/
    ret = set_led_state(state,dev,slot_overflow_leds);
  } else {
    ret = set_led_state(state,dev,&slot_leds[num]);
  }

  if (ret < 0 && ret != -2)
    printf("There were errors on setting the LEDs. Permissions?\n");

  compute_device_map(state,dev);



  return 0;
}

int remove_device_from_slot(struct wii_device *dev) {
  if (dev == NULL) {
    return -1;
  }


  if (dev->slot == NULL)
    return 0; /*This is okay!*/

  if (dev->slot_list == NULL) {
    return -1; /*Something has gone wrong...*/
  }

  if (dev->slot_list->next != NULL)
    dev->slot_list->next->prev = dev->slot_list->prev;

  if (dev->slot_list->prev != NULL)
    dev->slot_list->prev->next = dev->slot_list->next;



  if (dev->type == BALANCE) {
    dev->slot->has_board--;
  } else {
    dev->slot->has_wiimote--;
  }


  dev->slot = NULL;



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

    /*Try to do the right thing:
     *If it was the keyboardmouse map, unset it.
     *otherwise, maintain the specific mapping.
     */
    if (slot->slot_specific_mappings == state->slots[0].slot_specific_mappings) {
      printf("Switched slot's mapping to the gamepad mapping.\n");
      slot->slot_specific_mappings = NULL;
    }
    return 0;
  }

  if (type == SLOT_KEYBOARDMOUSE) {
    slot->uinput_fd = slot->keyboardmouse_fd;
    slot->type = SLOT_KEYBOARDMOUSE;
    /*If no specific map set, go ahead and use the keyboardmouse one.*/
    if (slot->slot_specific_mappings == NULL) {
      printf("Switched slot's mapping to the keyboardmouse mapping.\n");
      slot->slot_specific_mappings = state->slots[0].slot_specific_mappings;
    }
    return 0;
  }

  return -2;

}

int set_slot_specific_mappings(struct virtual_controller *slot, struct mode_mappings *maps) {
  if (slot ==  NULL)
    return -1;

  /*The keyboardmouse slot has its mapping set exactly once*/
  if (slot->slot_number == 0 && slot->slot_specific_mappings != NULL)
    return -2;


  if (slot->slot_specific_mappings != NULL)
    mappings_unref(slot->slot_specific_mappings);

  slot->slot_specific_mappings = maps;

  if (maps != NULL)
    mappings_ref(maps);

  return 0;

}

struct virtual_controller* lookup_slot(struct wiimoteglue_state* state, char* name) {
  if (name == NULL)
    return NULL;

  int i;
  for (i = 0; i <= state->num_slots; i++) {
    if (strncmp(name,state->slots[i].slot_name,WG_MAX_NAME_SIZE) == 0)
      return &state->slots[i];
  }

  return NULL;

}

