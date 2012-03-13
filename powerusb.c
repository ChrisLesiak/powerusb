#include <libusb-1.0/libusb.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define USB_VENDOR_ID 0x04d8
#define USB_PRODUCT_ID 0x003f
#define ENDPOINT_IN 0x81
#define ENDPOINT_OUT 0x01

#define CMD_GET_MODEL 0xaa
#define CMD_GET_FIRM_VER 0xa7
#define CMD_GET_STATE1 0xa1
#define CMD_GET_STATE2 0xa2
#define CMD_GET_STATE3 0xac

//#define DEBUG

#ifdef DEBUG
#define Dprintf printf
#else
#define Dprintf(fmt, ...) while(0){}
#endif

libusb_context *ctx = NULL;
libusb_device *dev;
libusb_device **devs;
struct libusb_device_handle *devh = NULL;
int state[4];

void usage(void)
{
  const char usage_string[] = 
    "command usage\n"
    "\tpowerusb get power [sec]\n"
    "\tpowerusb get state [1-3]\n"
    "\tpowerusb set [1-3] [on|off]\n";

  printf("%s",usage_string);
  exit(1);
}


int send_cmd(struct libusb_device_handle *devh,int cmd, uint8_t ret[2])
{
  int i;
  uint8_t buf[64];

  int size=0;

  Dprintf("send_cmd:%x\n",cmd);

  memset(buf, 0xff, sizeof(buf));
  buf[0] = cmd;
  
  if((libusb_interrupt_transfer(devh,ENDPOINT_OUT,buf, 
				sizeof(buf),&size, 1000)) < 0 ) {
    perror("libusb_interrupt_transfer");
    exit(1);
  }
  memset(buf, 0x00, sizeof(buf));

  if((libusb_interrupt_transfer(devh,ENDPOINT_IN,buf, 
				sizeof(buf),&size, 1000)) < 0 ) {
    perror("libusb_interrupt_transfer");
    exit(1);
  }

  Dprintf("send_cmd:read:");
  for(i=0;i<2;i++){
    ret[i] = buf[i];
    Dprintf("%02x",buf[i]);
  }
  Dprintf("\n");
  
  return 0;
}

void finalize(void){
  libusb_close(devh);
  libusb_exit(ctx);
}

int initialize(void){
  int r=1;
  int i=0;
  int cnt=0;
  /* libusb initialize*/
  if ((r = libusb_init(&ctx)) < 0) {
    perror("libusb_init\n");
    exit(1);
  } else {
    libusb_set_debug(ctx,3);
    Dprintf("init done\n");
  }  
  
  /* confirm powerusb device */
  /* list up all usb devices */
  if((libusb_get_device_list(ctx,&devs)) < 0) {
    perror("no usb device found");
    exit(1);
  }
  /* check every usb devices */
  while((dev =devs[i++]) != NULL) {
    struct libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(dev,&desc) < 0) {
      perror("failed to get device descriptor\n");
      return 1;
    }
    /* count how many PowerUSB device connected */
    if (desc.idVendor == USB_VENDOR_ID &&
	desc.idProduct == USB_PRODUCT_ID) {
      cnt++;
      Dprintf("PowerUSB device found\n");
    }
  }
  /* no PowerUSB found*/
  if (cnt == 0) {
    fprintf(stderr, "Power USB device not connected\n");
    exit(1);
  }
  /* multi-PowerUSB device found: return error*/
  if (cnt > 1) {
    /* FIXME */
    fprintf(stderr, "multi PowerUSB is not implemented yet\n");
    exit(1);
  }


  /* open powerusb device */
  if ((devh = libusb_open_device_with_vid_pid(ctx,USB_VENDOR_ID,
					      USB_PRODUCT_ID)) < 0 ) {
    perror("can't find PowerUSB device\n");
    finalize();
    exit(1);
  } else {
    Dprintf("PowerUSB device opened\n");
  }

  /* detach kernel driver if attached. */
  /* is kernel driver active?*/
  r = libusb_kernel_driver_active(devh,0);
  if (r == 1) {
    /*detaching kernel driver*/
    r = libusb_detach_kernel_driver(devh,0);
    if (r != 0) {
      perror("detaching kernel driver failed");
      exit(1);
    }
  }
  return 0;
}

int get_status(int a)
{
  uint8_t ret[2];

  if (( a < 0) | (a > 3))
    usage();

  switch(a){
  case 1:
    send_cmd(devh,CMD_GET_STATE1,ret);
    break;
  case 2:
    send_cmd(devh,CMD_GET_STATE2,ret);
    break;
  case 3:
    send_cmd(devh,CMD_GET_STATE3,ret);
    break;
  default:
    usage();
  }
  printf("Outlet%d status:",a);
  if (ret[0] == 0){
    printf("OFF\n");
  }else {
    printf("ON\n");
  }



  return 0;
}



int cmd_get(int argc,char **argv)
{
  if (argc < 4) 
    usage();

  if (!strcmp(argv[2],"power")) {
    printf("power\n");
  } else if (!strcmp(argv[2],"state")){
    if (!(!strcmp(argv[3],"1") | !strcmp(argv[3],"2") | !strcmp(argv[3],"3" )))
      usage();

    get_status(atoi(argv[3]));

  }else{
    usage();
  }

  return 0;
}
int cmd_set(int argc,char **argv)
{


  return 0;
}



int main(int argc, char **argv)
{
  uint8_t ret[2];
  int i;
 
  initialize();
  send_cmd(devh,CMD_GET_MODEL,ret);
  printf("Model:");
  switch (ret[0]){
  case 1:
    printf("Basic\n");
    break;
  case 2:
    printf("digIO\n");
    break;
  case 3:
    printf("watchdog\n");
    break;
  case 4:
    printf("smart\n");
    break;
  }
  send_cmd(devh,CMD_GET_FIRM_VER,ret);
  printf("firmware version: %d.%d\n",ret[0],ret[1]);

  send_cmd(devh,CMD_GET_STATE1,ret);
  state[1] = ret[0];

  send_cmd(devh,CMD_GET_STATE2,ret);
  state[2] = ret[0];

  send_cmd(devh,CMD_GET_STATE3,ret);
  state[3] = ret[0];

  for (i=1;i<4;i++){
    printf("Outlet%d:",i);
    switch(state[i]){
    case 0:
      printf("off\n");
      break;
    case 1:
      printf("on\n");
      break;
    }

  }  

  if (argc < 2) {
    usage();
    exit(1);
  }
  if (!strcmp(argv[1],"get")) {
      cmd_get(argc,argv);
  }else if (!strcmp(argv[1],"set")) {
      printf("set\n");
      cmd_set(argc,argv);
  }else {
    usage();
    exit(1);
  }


  finalize();
  exit(0);
}
