#include <stdio.h>
#include <string.h>
#include "wiimoteglue.h"

#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>

/* Handles user input from STDIN and command files. */

/* Line reading and splitting based off of basic snippets
 * seen online in various places.
 *
 * Command parsing done entirely by hand. No need to
 * over-engineer this just yet, although consider
 * this fair warning that ugly code lies below.
 */

/* should be sufficient, and lets us avoid chewing memory
 * if the user opens a file with huge lines.
 * any command has limited length keywords,
 * so this should be a fair assumption.
 *
 * The maximum linelength is
 * (start size) * 2^n
 * since each realloc doubles the buffer.
 */
#define MAX_REALLOC 5

char * getwholeline(int file) {
  char *buffer = malloc(50);
  char *ptr = buffer;
  size_t totallength = 50;
  int spaceleft = totallength;
  char nextchar[2];
  int ret;
  int reallocs = 0;

  if(buffer == NULL)
    return NULL;

  /*Loop until a break, or the program as a whole receives a signal
   *to stop
   */
  while(*KEEP_LOOPING) {
    ret = read(file,nextchar,1);
    if(ret <= 0)
      break;

    if(spaceleft > 0 && --spaceleft == 0 && reallocs < MAX_REALLOC ) {
      spaceleft = totallength;
      char *newbuff = realloc(buffer, totallength *= 2);

      if(newbuff == NULL) {
	free(buffer);
	return NULL;
      }

      ptr = newbuff + (ptr - buffer);
      buffer = newbuff;
      reallocs++;
    }

    if (spaceleft > 0 )
      *ptr++ = nextchar[0];

    if (nextchar[0] == '\n')
      break;
  }

  if (ptr == buffer) {
    /* nothing was read! Something wrong, EOF? */
    free(buffer);
    return NULL;
  }

  *ptr = '\0';
  return buffer;
}

#define NUM_WORDS 6
void process_command(struct wiimoteglue_state *state, char *args[]);
void update_mapping(struct wiimoteglue_state *state, struct mode_mappings* maps, char *mode, char *in, char *out, char *opt);
void toggle_setting(struct wiimoteglue_state *state, struct mode_mappings* maps, int active, char *mode, char *setting, char *opt);
int change_slot(struct wiimoteglue_state *state, char *slotname, char *setting);
int list_devices(struct wii_device_list *devlist, char *option);
struct wii_device* lookup_device(struct wii_device_list *devlist, char *name);
int * get_input_key(char *key_name, int button_map[]);
int * get_input_axis(char *axis_name, struct event_map *map);
int get_output_key(char *key_name);
int get_output_axis(char *key_name);

int wiimoteglue_handle_input(struct wiimoteglue_state *state, int file) {
  char *line = getwholeline(file);
  if (line != NULL) {
    char *lineptr = line;
    char *args[NUM_WORDS];
    char **argptr;

    for (argptr = args; (*argptr = strsep(&lineptr, " \t\n")) != NULL;) {
      if (**argptr != '\0') {
	if (++argptr >= &args[NUM_WORDS]) {
          break;
	}
      }
    }


    process_command(state,args);


    free(line);
    return 0;
  }
  return -1;
}

void process_command(struct wiimoteglue_state *state, char *args[]) {
  /* lets avoid endless loops on our main thread, and
   * add some sanity if a person accidentally tries
   * to open a huge file.
   */
  if (state->load_lines == MAX_LOAD_LINES) {
      printf("Maximum number of lines for command files exceeded.\n");
      printf("Either you have an endless loop in files loading files,\n");
      printf("or your command files are too big. There are only so many buttons to map...\n");
      state->load_lines++;
      return;
  }
  state->load_lines++;

  if (args[0] == NULL) {
    return;
  }
  if (args[0][0] == '#') {
    return;
  }

  if (strcmp(args[0],"quit") == 0) {
    state->keep_looping = 0;
    return;
  }
  if (strcmp(args[0],"help") == 0) {
    printf("The following commands are recognized:\n");
    printf("\thelp - show this message\n");
    printf("\tmap - change a button/axis mapping in one of the modes\n");
    printf("\tenable/disable - control extra controller features\n");
    printf("\tlist - list all open devices\n");
    printf("\tassign - assign a device to a virtual slot\n");
    printf("\tslot - set a slot to be a gamepad or a keyboard/mouse\n");
    printf("\tload - opens a file and runs the commands inside\n");
    printf("\tquit - close down WiimoteGlue\n");
    printf("\tmodes - show recognized keywords for controller modes\n");
    printf("\tevents - show recognized keywords for input/output events\n");
    printf("\tfeatures - show recognized controller features to enable/disable\n");
    printf("\nCommands expecting arguments will show their usage formats.\n");
    return;
  }
  if (strcmp(args[0],"modes") == 0) {
    printf("A separate input mapping is maintained for each of the following modes.\n");
    printf("The mode used changes automatically when extensions are inserted/removed.\n");
    printf("\t\"wiimote\" - used when the wiimote has no extension. (Motion Plus is ignored.)\n");
    printf("\t\"nunchuk\" - used when a nunchuk is present.\n");
    printf("\t\"classic\" - used when a classic controller is present, or for a Wii U pro controller\n");
    printf("\t\"all\" - applies to all three modes.\n");
    return;
  }
  if (strcmp(args[0],"events") == 0) {
    printf("The recognized names for input buttons are:\n");
    printf("up, down, left, right, a, b, c, x, y, z, plus, minus, home, 1, 2, l, r, zl, zr, thumbl, thumbr\n\n");

    printf("The recognized names for input axes are:\n");
    printf("\taccel_x - wiimote acceleration x (tilt forward/back)\n");
    printf("\taccel_y - wiimote acceleration y (tilt left/right)\n");
    printf("\taccel_z - wiimote acceleration z\n");
    printf("\tir_x,ir_y - Infared \"pointer\" axes.\n");
    printf("\tn_accel_x - nunchuk acceleration x (tilt forward/back)\n");
    printf("\tn_accel_y - nunchuk acceleration y (tilt left/right)\n");
    printf("\tn_accel_z - nunchuk acceleration z\n");
    printf("\tn_x, n_y - nunchuck control stick axes\n");
    printf("\tleft_x, left_y - classic/pro left stick axes\n");
    printf("\tright_x, right_y - classic/pro right stick axes\n");
    /*Not implemented at the moment.
     * printf("\tbal_fl,bal_fr,bal_bl,bal_br - balance board front/back left/right axes\n");
     */
    printf("\tbal_x,bal_y - balance board center-of-gravity axes.\n");
    printf("\n");

    printf("The recognized names for the synthetic gamepad output buttons are:\n");
    printf("up, down, left, right, north, south, east, west, start, select, mode, tl, tr, tl2, tr2, thumbl, thumbr, none\n");
    printf("(See the Linux gamepad API for these definitions. North/south/etc. refer to the action face buttons.)\n\n");

    printf("When mapped to a keyboard/mouse, these button mappings are available:\n");
    printf("\tleft_click, right_click, middle_click, key_a, key_leftshift, etc.\n");
    printf("(look up uapi/linux/input.h for all key names, just make them lowercase)\n\n");

    printf("The recognized names for the output axes are:\n");
    printf("\tleft_x, left_y - left stick axes\n");
    printf("\tright_x, right_y - right stick axes\n");
    printf("\tnone - an ignored axis\n");

    printf("\tmouse_x, mouse_y - aliases for left_x, left_y\n");

    printf("Add \"invert\" at the end of an axis mapping to invert it.\n");


    return;
  }
  if (strcmp(args[0],"features") == 0) {
    printf("The recognized extra features are:\n");
    printf("\taccel - process and output acceleration axis mappings\n");
    printf("\tir - process the wiimotes infared pointer axes\n");
    printf("(currently no extra options for these features are recognized)\n");
    return;
  }
  if (strcmp(args[0],"map") == 0) {
    struct mode_mappings* maps;
    if (mode_name_check(args[1]) >= 0) {
      /*The first argument was a mode name.
       *We assume the gamepad mapping by default.
       */
      maps = lookup_mappings(state,"gamepad");
      update_mapping(state,maps,args[1],args[2],args[3],args[4]);
    } else {
      maps = lookup_mappings(state,args[1]);
      update_mapping(state,maps,args[2],args[3],args[4],args[5]);
    }
    return;
  }
  if (strcmp(args[0],"enable") == 0) {
    struct mode_mappings* maps;
    if (mode_name_check(args[1]) >= 0) {
      /*The first argument was a mode name.
       *We assume the gamepad mapping by default.
       */
      maps = lookup_mappings(state,"gamepad");
      toggle_setting(state,maps,1,args[1],args[2],args[3]);

    } else {
      maps = lookup_mappings(state,args[1]);
      toggle_setting(state,maps,1,args[2],args[3],args[4]);
    }
    return;
  }
  if (strcmp(args[0],"disable") == 0) {
    struct mode_mappings* maps;
    if (mode_name_check(args[1]) >= 0) {
      /*The first argument was a mode name.
       *We assume the gamepad mapping by default.
       */
      maps = lookup_mappings(state,"gamepad");
      toggle_setting(state,maps,0,args[1],args[2],args[3]);

    } else {
      maps = lookup_mappings(state,args[1]);
      toggle_setting(state,maps,0,args[2],args[3],args[4]);
    }
    return;
  }
  if (strcmp(args[0],"load") == 0) {
    wiimoteglue_load_command_file(state,args[1]);
    return;
  }
  if (strcmp(args[0],"slot") == 0) {
    change_slot(state,args[1],args[2]);
    /*Currently no way to lookup devices assigned
     *to a slot, other than iterating over the list.*/
    wiimoteglue_compute_all_device_maps(state,&state->dev_list);
    return;
  }
  if (strcmp(args[0],"assign") == 0) {
     assign_device(state,args[1],args[2]);
     compute_device_map(state,state->dev_list.dev);
     return;
  }
  if (strcmp(args[0],"list") == 0) {
    list_devices(&state->dev_list,args[1]);
    return;
  }


  printf("Command not recognized.\n");
}

void update_mapping(struct wiimoteglue_state *state, struct mode_mappings* maps, char *mode, char *in, char *out, char *opt) {
  struct event_map *mapping = NULL;


  if (maps == NULL || mode == NULL || in == NULL || out == NULL) {
    printf("Invalid command format.\n");
    printf("usage: map [gamepad|keyboardmouse] <mode> <wii input> <gamepad output> [invert]\n");
    printf("If the gamepad/keyboardmouse specifier is omitted, gamepad is assumed.\n");
    return;
  }

  if (strcmp(mode,"wiimote") == 0) {
    mapping = &maps->mode_no_ext;
  } else if (strcmp(mode,"nunchuk") == 0) {
    mapping = &maps->mode_nunchuk;
  } else if (strcmp(mode,"classic") == 0) {
    mapping = &maps->mode_classic;
  } else if (strcmp(mode,"all") == 0) {
    /*This is rather hack-ish. oh well.*/
    update_mapping(state,maps,"wiimote",in,out,opt);
    update_mapping(state,maps,"nunchuk",in,out,opt);
    update_mapping(state,maps,"classic",in,out,opt);
    return;
  }

  if (mapping == NULL) {
    printf("Controller mode \"%s\" not recognized.\n(Valid modes are \"wiimote\",\"nunchuk\", and \"classic\")\n",mode);
    return;
  }

  int *button = get_input_key(in,mapping->button_map);
  if (button != NULL) {
      int new_key = get_output_key(out);
      if (new_key == -2) {
	printf("Output button \"%s\" not recognized. See \"events\" for valid values.\n",out);
	return;
      }

      *button = new_key;
      return;
  }

  int *axis = get_input_axis(in,mapping);

  if (axis != NULL) {
    int new_axis = get_output_axis(out);
    if (new_axis == -2) {
      printf("Output axis \"%s\" not recognized. See \"events\" for valid values.\n",out);
      return;
    }

    axis[0] = new_axis;

    if (opt != NULL && strcmp(opt,"invert") == 0) {
      axis[1] = -abs(axis[1]);
    } else {
      axis[1] = abs(axis[1]);
    }
    return;
  }


  printf("Input event \"%s\" not recognized. See \"events\" for valid values.\n",in);
  printf("usage: map [gamepad|keyboardmouse] <mode> <wii input> <gamepad output> [invert]\n");
  return;


}

void toggle_setting(struct wiimoteglue_state *state, struct mode_mappings* maps, int active, char *mode, char *setting, char *opt) {
  struct event_map *mapping = NULL;

  if (maps == NULL || mode == NULL || setting == NULL) {
    printf("Invalid command format.\n");
    printf("usage: <enable|disable> [gamepad|keyboardmouse] <mode> <feature> [option]\n");
    return;
  }

  if (strcmp(mode,"wiimote") == 0) {
    mapping = &maps->mode_no_ext;
  } else if (strcmp(mode,"nunchuk") == 0) {
    mapping = &maps->mode_nunchuk;
  } else if (strcmp(mode,"classic") == 0) {
    mapping = &maps->mode_classic;
  } else if (strcmp(mode,"all") == 0) {
    /* also hackish*/
    toggle_setting(state,maps,active,"wiimote",setting,opt);
    toggle_setting(state,maps,active,"nunchuk",setting,opt);
    toggle_setting(state,maps,active,"classic",setting,opt);
    return;
  }

  if (mapping == NULL) {
    printf("Controller mode \"%s\" not recognized.\n(Valid modes are \"wiimote\",\"nunchuk\", and \"classic\")\n",mode);
    return;
  }

  if (strcmp(setting, "accel") == 0) {
    mapping->accel_active = active;
    wiimoteglue_update_wiimote_ifaces(&state->dev_list);
    return;
  }

  if (strcmp(setting, "ir") == 0) {
    int ir_count = 1;

    if (opt != NULL && strcmp(opt,"multiple") == 0) ir_count = 2;

    if (!active) ir_count = 0;

    /*currently multiple IR sources, or a single one are treated identically*/
    mapping->IR_count = ir_count;
    wiimoteglue_update_wiimote_ifaces(&state->dev_list);
    return;
  }

  printf("Feature \"%s\" not recognized.\n",setting);
  printf("usage: <enable|disable>  [gamepad|keyboardmouse] <mode> <feature> [option]\n");
}




int wiimoteglue_load_command_file(struct wiimoteglue_state *state, char *filename) {
  if (filename == NULL) {
    printf("usage: load <filename>\n");
    return 0;
  }
  int fd = open(filename,O_RDONLY);
  if (fd < 0) {
    printf("Failed to open file \'%s\'\n",filename);
    perror("Open");
    return -1;
  }
  printf("Reading commands from file \'%s\'\n",filename);
  int ret = 0;
  while (ret >= 0) {
    if (state->load_lines > MAX_LOAD_LINES) {
      /*exceeded number of lines read, start backing out.*/
      close(fd);
      return -2;
    }
    ret = wiimoteglue_handle_input(state, fd);
  }

  close(fd);
  return 0;

}

int change_slot(struct wiimoteglue_state *state, char *slotname, char *setting) {
  if (slotname == NULL || setting == NULL) {
    printf("usage: slot <slotnumber> <gamepad|keyboardmouse>\n");
    return -1;
  }

  int slotnumber = slotname[0] - '0'; /*HACK: single-digit atoi*/


  if (slotnumber < 1 || slotnumber > state->num_slots) {
    printf("\'%s\' is not a valid slot number.\n",slotname);
    return -1;
  }

  struct virtual_controller* slot = &state->slots[slotnumber];

  if (strcmp(setting, "gamepad") == 0) {
    printf("Setting slot %d to be a virtual gamepad.\n",slotnumber);
    change_slot_type(state,slot,SLOT_GAMEPAD);
    return 0;
  }

  if (strcmp(setting, "keyboardmouse") == 0) {
    printf("Setting slot %d to be a virtual keyboard/mouse combo.\n",slotnumber);
    change_slot_type(state,slot,SLOT_KEYBOARDMOUSE);
    return 0;
  }

  if (strcmp(setting, "list") == 0) {
    printf("Listing devices in slot...\n");
    list_devices(&slot->dev_list,NULL);
    return 0;
  }



  printf("\'%s\' was not a recognized slot command.\n",setting);
  printf("(try \"gamepad\" or \"keyboardmouse\")\n");

  return -1;
}

int assign_device(struct wiimoteglue_state *state, char *devname, char *slotname) {
  if (devname == NULL || slotname == NULL) {
    printf("usage: assign <device name|device address> <slot number|\"keyboardmouse\"|\"none\">\n");
    return 0;
  }
  struct wii_device* device = lookup_device(&state->dev_list,devname);
  if (device == NULL) {
    printf("\'%s\' did not match a device id or address.\n",devname);
    printf("Use \"list\" to see devices.\n");
    return -1;
  }
  struct virtual_controller* slot = NULL;
  int slotnumber = slotname[0] - '0'; /*HACK: single-digit atoi*/
  if (slotnumber >= 1 && slotnumber <= state->num_slots) {
    slot = &state->slots[slotnumber];
  }
  if (strcmp(slotname,"keyboardmouse") == 0) {
    slot = &state->slots[0];
  }
  if (slot == NULL && strcmp(slotname,"none") != 0) {
    printf("\'%s\' was not a valid slot.\n",slotname);
    printf("Valid choices are keyboardmouse, ");
    int i;
    for (i = 1; i <= state->num_slots; i++) {
      printf("%d, ",i);
    }
    printf("none\n");
    return -1;
  }

  int ret;
  /*Both of these functions handle NULL slots correctly*/
  printf("removing... %p %p\n",device->slot,device->slot_list);
  ret = remove_device_from_slot(device);
  if (ret < 0) {
    printf("ERROR REMOVING FROM SLOT %d\n",ret);
    return -1;
  }

  printf("adding... %p %p\n",device->slot,device->slot_list);

  ret = add_device_to_slot(state,device,slot);
  if (ret < 0) {
    printf("ERROR ADDING TO SLOT\n");
    return -1;
  }

  return 0;

}




int list_devices(struct wii_device_list *devlist, char *option) {
  if (devlist == NULL) {
    return -1;
  }
  struct wii_device_list* list_node = devlist->next;

  static char* wiimote = "Wii Remote";
  static char* pro = "Wii U Pro Controller";
  static char* board = "Balance Board";
  static char* unknown = "Unknown Device Type?";
  char* type = unknown;

  while (list_node != devlist && list_node != NULL) {
    struct wii_device* dev = list_node->dev;
    if (dev != NULL) {
      if (dev->type == REMOTE)
	type = wiimote;
      if (dev->type == PRO)
	type = pro;
      if (dev->type == BALANCE)
	type = board;

      printf("\n- %s (%s)\n",dev->id,dev->bluetooth_addr);
      printf("\t%s\n",type);

      if (dev->slot != NULL) {
	if (dev->slot->slot_number != 0) {
	  printf("\tAssigned to slot %d\n",dev->slot->slot_number);
	} else {
	  printf("\tAssigned to virtual keyboard/mouse\n");
	}
	printf("\t%p %p\n",dev->slot,dev->slot_list);
      } else {
	printf("\tNot assigned to any slot");
      }

    }

    list_node = list_node->next;
  }


  return 0;
}

struct wii_device * lookup_device(struct wii_device_list *devlist, char *name) {

  if (devlist == NULL) {
    return NULL;
  }

  struct wii_device_list* list_node = devlist->next;

  while (*KEEP_LOOPING && list_node != devlist && list_node != NULL) {


    if (list_node->dev != NULL) {
      if (strncmp(name,list_node->dev->id,32) == 0)
	return list_node->dev;
      if (strncmp(name,list_node->dev->bluetooth_addr,18) == 0)
	return list_node->dev;
    }

    list_node = list_node->next;
  }


  return NULL;
}



