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
//#include <intraFont.h>
#include "font.c"

#define printf pspDebugScreenPrintf

#define MODULE_NAME "PSPChat"
#define MAX_MESSAGE_LEN 1024
#define PORT 8080
#define SERVER_IP "192.168.1.33"

// PSP_MODULE_INFO is required
PSP_MODULE_INFO(MODULE_NAME, 0, 1, 0); //name, attributes, major version, minor version

PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER); //sets main thread as a user thread, might be optional now

//not sure what these should be yet, taken from net example code.
//PSP_HEAP_THRESHOLD_SIZE_KB(1024); //this seems to be the one that actually matters and increases available mem for external threads, but i just reduced the thread to 32kb instead
//PSP_HEAP_SIZE_KB(-2048);
//PSP_MAIN_THREAD_STACK_SIZE_KB(1024);

//next three functions are required for being able to exit the game with the home button, taken from example code.
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


int client_fd;
static int running = 1;

char recv_buf[MAX_MESSAGE_LEN]; //shared buffer so always use semaphore

SceUID semaid;


int recv_thread(SceSize args, void *argp) { //thread entries have to be defined like this
    while (running) {
        char local_buf[MAX_MESSAGE_LEN];
        int conn;
        //char *send_buf_status;

        conn = recv(client_fd, local_buf, MAX_MESSAGE_LEN - 1, 0);
        
        if (conn == 0) { 
            printf("Lost connection to the host.\n");
            break;
        } else if (conn < 0) {
            printf("something else happened bad, exiting");
            break;
        }

        local_buf[conn] = '\0'; //recv doesnt null terminate, so terminate at the number of bytes you received

        sceKernelWaitSema(semaid, 1, 0);
        memcpy(recv_buf, local_buf, conn + 1);
        sceKernelSignalSema(semaid, 1);

        //printf("%s", recv_buf);

    }

    close(client_fd);

    sceKernelExitDeleteThread(0);

    return 0;
}

//decided on multithreading!
// cant use either poll or select for taking inputs multiplexed, have to look into either multithreading or non blocking in the main graphics rendering loop
//taken from my prototype code, no poll() for now as I think I have to switch to select()
int setup_recv_thread() {
    SceUID thid;
    struct sockaddr_in my_addr;
    int status;
    //char send_buf[MAX_MESSAGE_LEN];
    
    client_fd = socket(PF_INET, SOCK_STREAM, 0);

    if (client_fd == -1) {
        printf("socket error: %d\n", sceNetInetGetErrno());
        close(client_fd);
        return -1;
    }

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT);
    my_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);

    status = connect(client_fd, (struct sockaddr *)&my_addr, sizeof my_addr);
    
    if (status < 0) {
        printf("connect error: ret=%d errno=%d\n", status, sceNetInetGetErrno());
        close(client_fd);
        return -1;
    } 

    printf("Connection established!\nYou are now chatting.\n");

    printf("Max Free Mem size: %d\n", sceKernelMaxFreeMemSize());
    printf("Max TOTAL Free Mem size: %d\n", sceKernelTotalFreeMemSize());

    thid = sceKernelCreateThread("recv_thread", recv_thread, 0x21, 32 * 1024, PSP_THREAD_ATTR_USER, NULL); /*name, 
                                                                                                                thread entry point function, 
                                                                                                                init priority (example says 0x11 is default, but using 0x21 so it is lower priority than basically everything else including main (i think)),
                                                                                                                stack size (example was using 256kb, using 32kb),
                                                                                                                thread atrribute, this means user thread,
                                                                                                                options, no options*/

    if (thid < 0) {
        printf("Error, couldn't create thread. thid=%08X\n", thid);
        close(client_fd);
        return -1;
    }

    sceKernelStartThread(thid, 0, NULL); //thread id from create thread, length of arguments in bytes, arguments. just using a global variable instead for the file descriptor lol
    
    printf("Max Free thread stack free size: %d\n", sceKernelGetThreadStackFreeSize(thid));

    return thid;
}

//from samples/gu/text example code
static int fontwidthtab[128] = {
	10, 10, 10, 10, 
	10, 10, 10, 10,
	10, 10, 10, 10, 
	10, 10, 10, 10,

	10, 10, 10, 10, 
	10, 10, 10, 10,
	10, 10, 10, 10,
	10, 10, 10, 10,

	10,  6,  8, 10, //   ! " #
	10, 10, 10,  6, // $ % & '
	10, 10, 10, 10, // ( ) * +
	 6, 10,  6, 10, // , - . /

	10, 10, 10, 10, // 0 1 2 3
	10, 10, 10, 10, // 6 5 8 7
	10, 10,  6,  6, // 10 9 : ;
	10, 10, 10, 10, // < = > ?

	16, 10, 10, 10, // @ A B C
	10, 10, 10, 10, // D E F G
	10,  6,  8, 10, // H I J K
	 8, 10, 10, 10, // L M N O

	10, 10, 10, 10, // P Q R S
	10, 10, 10, 12, // T U V W
	10, 10, 10, 10, // X Y Z [
	10, 10,  8, 10, // \ ] ^ _

	 6,  8,  8,  8, // ` a b c
	 8,  8,  6,  8, // d e f g
	 8,  6,  6,  8, // h i j k
	 6, 10,  8,  8, // l m n o

	 8,  8,  8,  8, // p q r s
	 8,  8,  8, 12, // t u v w
	 8,  8,  8, 10, // x y z {
	 8, 10,  8, 12  // | } ~  
};

//setup for graphics loop
static unsigned int __attribute__((aligned(16))) list[262144];

#define BUF_WIDTH (512)
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)
#define PIXEL_SIZE (4)
#define FRAME_SIZE (BUF_WIDTH * SCR_HEIGHT * PIXEL_SIZE)


//setting up graphics
/*static void setupGu()
{

        void* fbp0 = guGetStaticVramBuffer(BUF_WIDTH,SCR_HEIGHT,GU_PSM_8888);
        void* fbp1 = guGetStaticVramBuffer(BUF_WIDTH,SCR_HEIGHT,GU_PSM_8888);
        void* zbp = guGetStaticVramBuffer(BUF_WIDTH,SCR_HEIGHT,GU_PSM_4444);

		sceGuInit();

        sceGuStart(GU_DIRECT,list);
        sceGuDrawBuffer(GU_PSM_8888,fbp0,BUF_WIDTH);
        sceGuDispBuffer(SCR_WIDTH,SCR_HEIGHT,fbp1,BUF_WIDTH);
        sceGuDepthBuffer(zbp,BUF_WIDTH);
        sceGuOffset(2048 - (SCR_WIDTH/2),2048 - (SCR_HEIGHT/2));
        sceGuViewport(2048,2048,SCR_WIDTH,SCR_HEIGHT);
        sceGuDepthRange(65535,0);
        sceGuScissor(0,0,SCR_WIDTH,SCR_HEIGHT);
        sceGuEnable(GU_SCISSOR_TEST);
        sceGuDepthFunc(GU_GEQUAL);
        sceGuEnable(GU_DEPTH_TEST);
        sceGuFrontFace(GU_CW);
        sceGuShadeModel(GU_SMOOTH);
        sceGuEnable(GU_CULL_FACE);
        sceGuEnable(GU_TEXTURE_2D);
        sceGuEnable(GU_CLIP_PLANES);
        sceGuFinish();
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

        sceDisplayWaitVblankStart();
        sceGuDisplay(GU_TRUE);
}*/

//from samples/gu/text example code
/*
	This function draws a string on the screen
	The chars are handled as sprites.
	It supportes colors and blending.
	The fontwidth can be selected with the parameter fw, if it is 0 the best width for each char is selected.
*/
void drawString(const char* text, int x, int y, unsigned int color, int fw) {
	int len = (int)strlen(text);
	if(!len) {
		return;
	}

	typedef struct {
		float s, t;
		unsigned int c;
		float x, y, z;
	} VERT;

	VERT* v = sceGuGetMemory(sizeof(VERT) * 2 * len);

	int i;
	for(i = 0; i < len; i++) {
		unsigned char c = (unsigned char)text[i];
		if(c < 32) {
			c = 0;
		} else if(c >= 128) {
			c = 0;
		}

		int tx = (c & 0x0F) << 4;
		int ty = (c & 0xF0);

		VERT* v0 = &v[i*2+0];
		VERT* v1 = &v[i*2+1];
		
		v0->s = (float)(tx + (fw ? ((16 - fw) >> 1) : ((16 - fontwidthtab[c]) >> 1)));
		v0->t = (float)(ty);
		v0->c = color;
		v0->x = (float)(x);
		v0->y = (float)(y);
		v0->z = 0.0f;

		v1->s = (float)(tx + 16 - (fw ? ((16 - fw) >> 1) : ((16 - fontwidthtab[c]) >> 1)));
		v1->t = (float)(ty + 16);
		v1->c = color;
		v1->x = (float)(x + (fw ? fw : fontwidthtab[c]));
		v1->y = (float)(y + 16);
		v1->z = 0.0f;

		x += (fw ? fw : fontwidthtab[c]);
	}

	sceGumDrawArray(GU_SPRITES, 
		GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
		len * 2, 0, v
	);
}


void setupGu(){
	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888,(void*)0,BUF_WIDTH);
	sceGuDispBuffer(SCR_WIDTH,SCR_HEIGHT,(void*)FRAME_SIZE,BUF_WIDTH);
	sceGuDepthBuffer((void*)(FRAME_SIZE*2),BUF_WIDTH);
	sceGuOffset(2048 - (SCR_WIDTH/2),2048 - (SCR_HEIGHT/2));
	sceGuViewport(2048,2048,SCR_WIDTH,SCR_HEIGHT);
	sceGuDepthRange(0xc350,0x2710);
	sceGuScissor(0,0,SCR_WIDTH,SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDisable(GU_DEPTH_TEST);
	sceGuShadeModel(GU_SMOOTH);
	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(GU_PSM_8888, 0, 0, 0);
	sceGuTexImage(0, 256, 128, 256, font);
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	sceGuTexEnvColor(0x0);
	sceGuTexOffset(0.0f, 0.0f);
	sceGuTexScale(1.0f / 256.0f, 1.0f / 128.0f);
	sceGuTexWrap(GU_REPEAT, GU_REPEAT);
	sceGuTexFilter(GU_NEAREST, GU_NEAREST);
	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
	sceGuDisplay(GU_TRUE);
}


void start_frame() {
    sceGuStart(GU_DIRECT, list);
}

void end_frame() {
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

void term_gu() {
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
}

//for now, only affects the netdialog function. will remove eventually.
static void drawStuff(void)
{
	sceGuStart(GU_DIRECT, list);

	sceGuClearColor(0xFFA1A1A1);
	sceGuClearDepth(0);
	sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
	
}


typedef struct {
    unsigned short u, v;
    short x, y, z;
} Vertex;

void drawRect(float x, float y, float w, float h) {

    Vertex* vertices = (Vertex*)sceGuGetMemory(2 * sizeof(Vertex));

    vertices[0].x = x;
    vertices[0].y = y;

    vertices[1].x = x + w;
    vertices[1].y = y + h;

    sceGuColor(0xFF0000FF); // Red, colors are ABGR
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, vertices);
}

void main_loop(SceUID thid) {
    SceCtrlData pad;

    int frame_count = 0;
    char frame_string[32];

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    unsigned int screen_color = 0xFFFFFFFF;

    while (running) {  //taken from controller input sample code, made it so (for now) the circle button guarantees a clean exit
        start_frame();

        sceGuClearColor(screen_color);
        sceGuClear(GU_COLOR_BUFFER_BIT); //clearing the depth bit seems to remove the rectangle, so only doing the color bit. not sure yet.   

        //drawRect(216, 96, 34, 64);

        snprintf(frame_string, sizeof(frame_string), "%d", frame_count);

        drawString(frame_string,   0,  240, 0xFF000000, 0);

        sceKernelWaitSema(semaid, 1, 0);
        drawString(recv_buf,   0,  0, 0xFF000000, 0);
        sceKernelSignalSema(semaid, 1);

        /*drawString("Hello World in red",   0,  0, 0xFF0000FF, 0);
		drawString("Hello World in green", 0, 16, 0xFF00FF00, 0);
		drawString("Hello World in blue",  0, 32, 0xFFFF0000, 0);*/

        //pspDebugScreenSetXY(0, 2);
        sceCtrlReadBufferPositive(&pad, 1);

        //printf("Analog X = %3d, ", pad.Lx);
        //printf("Analog Y = %3d \n", pad.Ly);
        
        
        if (pad.Buttons != 0)
        {
            if (pad.Buttons & PSP_CTRL_CIRCLE) {
                //printf("Circle pressed! Exiting. \n");
                close(client_fd);
                sceKernelWaitThreadEnd(thid, NULL);
                break;
            }
            if (pad.Buttons & PSP_CTRL_SQUARE) {
                //strcpy(recv_buf, "Goodbye Buddy");
                screen_color = 0xFFFF0000;
                //printf("Square pressed! \n");
            }
            if (pad.Buttons & PSP_CTRL_TRIANGLE) {
                screen_color = 0xFF0000FF;
                //printf("Triangle pressed! \n");
            }
            
            if (pad.Buttons & PSP_CTRL_CROSS) {
                screen_color = 0xFF00FF00;
                //printf("Cross pressed! \n");
            }
            
            if (pad.Buttons & PSP_CTRL_DOWN)
            {
                screen_color = 0xFFFFFFFF;
                //printf("Down direction pad pressed! \n");
            }
            /*if (pad.Buttons & PSP_CTRL_UP)
            {
                printf("Up direction pad pressed! \n");
            }
            if (pad.Buttons & PSP_CTRL_LEFT)
            {
                printf("Left direction pad pressed! \n");
            }
            if (pad.Buttons & PSP_CTRL_RIGHT)
            {
                printf("Right direction pad pressed! \n");
            } */
        }

        end_frame();

        frame_count++;
    }
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
		drawStuff();

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

void print_ip() { 
    union SceNetApctlInfo info;
    if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_IP, &info) == 0) {
        printf("IP: %s\n", info.ip);
    } else {
        printf("no IP");
    } 
}

int main(void)  {
    SceUID thid;

    semaid = sceKernelCreateSema("MyMutex", 0, 1, 1, 0);

    sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	sceUtilityLoadNetModule(PSP_NET_MODULE_INET);

    netInit();

    setup_callbacks();
    
    setupGu();

    netDialog();

    //pspDebugScreenInit();
    
    thid = setup_recv_thread();

    //strcpy(recv_buf, "Hello Buddy");

    if (thid < 0) {
        term_gu();

        netTerm(); //terminate network connection

        sceKernelDeleteSema(semaid);

        sceKernelSleepThread();
    }

    main_loop(thid);

    term_gu();

    netTerm(); //terminate network connection

    sceKernelDeleteSema(semaid);

    sceKernelSleepThread(); //so that the output of the screen stays up after program finishes instead of exiting immediately

    return 0;
}