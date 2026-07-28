#include <pspuser.h>
#include <pspdebug.h>
#include <pspdisplay.h>
//audit later which ones are actually required
#include <arpa/inet.h>
#include <errno.h>
#include <pspnet_apctl.h>
#include <pspsdk.h>
#include <psputility.h>
#include <psptypes.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspnet_inet.h>
#include <pspnet.h>
#include <math.h>
#include <pspctrl.h>

#define printf pspDebugScreenPrintf

#define MODULE_NAME "PSPChat"
#define MAX_MESSAGE_LEN 4096
#define PORT 8080
#define SERVER_IP "192.168.1.41"

// PSP_MODULE_INFO is required
PSP_MODULE_INFO(MODULE_NAME, 0, 1, 0); //name, attributes, major version, minor version

//not sure what these should be yet, taken from net example code.
//PSP_HEAP_THRESHOLD_SIZE_KB(1024); //this seems to be the one that actually matters and increases available mem for external threads
//PSP_HEAP_SIZE_KB(-2048);
//PSP_MAIN_THREAD_STACK_SIZE_KB(1024);


int client_fd;

char recv_buf[MAX_MESSAGE_LEN];

int recv_thread(SceSize args, void *argp) { //thread entries have to be defined like this
    while (1) {
        int conn;
        //char *send_buf_status;

        //printf("> ");

        conn = recv(client_fd, recv_buf, MAX_MESSAGE_LEN - 1, 0);


        if (conn == 0) { 
            printf("Lost connection to the host.\n");
            break;
        } else if (conn < 0) {
            printf("something else happened bad, exiting");
            break;
        }

        recv_buf[conn] = '\0'; //recv doesnt null terminate, so terminate at the number of bytes you received

        printf("%s", recv_buf);

    }

    close(client_fd);

    sceKernelExitDeleteThread(0);

    return 0;
}

//decided on multithreading!
// cant use either poll or select for taking inputs multiplexed, have to look into either multithreading or non blocking in the main graphics rendering loop
//taken from my prototype code, no poll() for now as I think I have to switch to select()
static void connect_to_client() {
    SceUID thid;
    struct sockaddr_in my_addr;
    int status;
    //char send_buf[MAX_MESSAGE_LEN];
    
    client_fd = socket(PF_INET, SOCK_STREAM, 0);

    if (client_fd == -1) {
        printf("socket error: %d\n", sceNetInetGetErrno());
        close(client_fd);
        return;
    }

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT);
    my_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);

    status = connect(client_fd, (struct sockaddr *)&my_addr, sizeof my_addr);
    
    if (status < 0) {
        printf("connect error: ret=%d errno=%d\n", status, sceNetInetGetErrno());
        close(client_fd);
        return;
    } 

    printf("Connection established!\nYou are now chatting.\n");

    printf("Max Free Mem size: %d\n", sceKernelMaxFreeMemSize());
    printf("Max TOTAL Free Mem size: %d\n", sceKernelTotalFreeMemSize());

    thid = sceKernelCreateThread("recv_thread", recv_thread, 0x21, 32 * 1024, PSP_THREAD_ATTR_USER, NULL); /*name, 
                                                                                                                thread entry point function, 
                                                                                                                init priority (example says 0x11 is default, but using 0x21 so it is lower priority than basically everything else including main (i think)),
                                                                                                                stack size (example was using 256kb, im using 32kb),
                                                                                                                thread atrribute, this means user thread,
                                                                                                                options, no options*/
    if (thid < 0) {
        printf("Error, couldn't create thread. thid=%08X\n", thid);
        close(client_fd);
        return;
    }

    sceKernelStartThread(thid, 0, NULL);    //thread id from create thread, length of arguments in bytes, arguments. just using a global variable instead for the file descriptor lol

    printf("Max Free thread stack free size: %d\n", sceKernelGetThreadStackFreeSize(thid));

    SceCtrlData pad;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    while (1) {  //taken from controller input sample code, made it so (for now) the circle button guarantees a clean exit

        //pspDebugScreenSetXY(0, 2);
        sceCtrlReadBufferPositive(&pad, 1);

        //printf("Analog X = %3d, ", pad.Lx);
        //printf("Analog Y = %3d \n", pad.Ly);

        if (pad.Buttons != 0)
        {
            if (pad.Buttons & PSP_CTRL_CIRCLE)
            {
                printf("Circle pressed! Exiting. \n");
                close(client_fd);
                break;
            }
            if (pad.Buttons & PSP_CTRL_SQUARE)
            {
                printf("Square pressed! \n");
            }
            if (pad.Buttons & PSP_CTRL_TRIANGLE)
            {
                printf("Triangle pressed! \n");
            }
            
            if (pad.Buttons & PSP_CTRL_CROSS)
            {
                printf("Cross pressed! \n");
            }

            if (pad.Buttons & PSP_CTRL_UP)
            {
                printf("Up direction pad pressed! \n");
            }
            if (pad.Buttons & PSP_CTRL_DOWN)
            {
                printf("Down direction pad pressed! \n");
            }
            if (pad.Buttons & PSP_CTRL_LEFT)
            {
                printf("Left direction pad pressed! \n");
            }
            if (pad.Buttons & PSP_CTRL_RIGHT)
            {
                printf("Right direction pad pressed! \n");
            }
        }
    }

    return;
}

//next three functions are required for being able to exit the game with the home button, also taken from example code.
int exit_callback(int arg1, int arg2, void *common) {
    sceKernelExitGame();
    return 0;
}

int callback_thread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int setup_callbacks(void) {
    int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if(thid >= 0)
        sceKernelStartThread(thid, 0, 0);
    return thid;
}

//setup for graphics loop
static int running = 1;

static unsigned int __attribute__((aligned(16))) list[262144];

#define BUF_WIDTH (512)
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)
#define PIXEL_SIZE (4)
#define FRAME_SIZE (BUF_WIDTH * SCR_HEIGHT * PIXEL_SIZE)

//netdialog runs through gu, so have to set this up, also from netdialog sample code
void setupGu(){
    sceGuInit();

    //Set up buffers
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888,(void*)0,BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH,SCR_HEIGHT,(void*)0x88000,BUF_WIDTH);
    sceGuDepthBuffer((void*)0x110000,BUF_WIDTH);

    //Set up viewport
    sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);

    //Set some stuff
    sceGuDepthRange(65535, 0); //Use the full buffer for depth testing - buffer is reversed order

    sceGuDepthFunc(GU_GEQUAL); //Depth buffer is reversed, so GEQUAL instead of LEQUAL
    sceGuEnable(GU_DEPTH_TEST); //Enable depth testing

    sceGuFinish();
    sceGuDisplay(GU_TRUE);
}

//need to look into this but i think its the main drawing function if you wanted to draw a shape here. right now its just a gray background instead.
static void drawStuff(void)
{
	sceGuStart(GU_DIRECT, list);

	sceGuClearColor(0xff554433);
	sceGuClearDepth(0);
	sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
	
}

//taken from netdialog example code, for bringing up dialog screen that asks which ap to connect to
//cannot do it the old way since only netdialog allows wpa2 connections with ARK 4/5
int netDialog()
{
	int done = 0;

   	pspUtilityNetconfData data;

	memset(&data, 0, sizeof(data));
	data.base.size = sizeof(data);
	data.base.language = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
	data.base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
	data.base.graphicsThread = 17;
	data.base.accessThread = 19;
	data.base.fontThread = 18;
	data.base.soundThread = 16;
	data.action = PSP_NETCONF_ACTION_CONNECTAP;
	
	struct pspUtilityNetconfAdhoc adhocparam;
	memset(&adhocparam, 0, sizeof(adhocparam));
	data.adhocparam = &adhocparam;

	sceUtilityNetconfInitStart(&data);
	
	while(running)
	{
		drawStuff(); //have to draw something in the background, so its just gray for now

		switch(sceUtilityNetconfGetStatus())
		{
			case PSP_UTILITY_DIALOG_NONE:
				break;

			case PSP_UTILITY_DIALOG_VISIBLE:
				sceUtilityNetconfUpdate(1);
				break;

			case PSP_UTILITY_DIALOG_QUIT:
				sceUtilityNetconfShutdownStart();
				break;
				
			case PSP_UTILITY_DIALOG_FINISHED:
				done = 1;
				break;

			default:
				break;
		}

		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
		
		if(done)
			break;
	}
	
	return 1;
}

void netInit(void)
{
	sceNetInit(128*1024, 42, 4*1024, 42, 4*1024);
	
	sceNetInetInit();
	
	sceNetApctlInit(0x8000, 48);
}

void netTerm(void)
{
	sceNetApctlTerm();
	
	sceNetInetTerm();
	
	sceNetTerm();
}


int main(void)  {
    sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	sceUtilityLoadNetModule(PSP_NET_MODULE_INET);

    netInit();

    setup_callbacks();
    
    setupGu();

    netDialog();

    //both required for shutting down graphics, only so we can display debug screen for now
    sceGuDisplay(GU_FALSE);
    sceGuTerm();

    pspDebugScreenInit();

    union SceNetApctlInfo info;
    if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_IP, &info) == 0) //prints the ip of the psp after the netdialog is sucessful
        printf("IP: %s\n", info.ip);
    else
        printf("no IP - dialog cancelled?\n");

    
    connect_to_client(); //main loop, need to rename and split in to functions

    netTerm(); //terminate network connection

    sceKernelSleepThread(); //so that the output of the screen stays up after program finishes instead of exiting immediately

    return 0;
}