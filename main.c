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

#define printf pspDebugScreenPrintf

#define MODULE_NAME "Psp Chat"
#define MAX_MESSAGE_LEN 4096

// PSP_MODULE_INFO is required
PSP_MODULE_INFO(MODULE_NAME, 0, 1, 0); //name, attributes, major version, minor version

//not sure what these should be yet, taken from example code
PSP_HEAP_THRESHOLD_SIZE_KB(1024);
PSP_HEAP_SIZE_KB(-2048);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_MAIN_THREAD_STACK_SIZE_KB(1024);

//taken from my prototype code, no poll() for now as I think I have to switch to select()
int connect_to_client() {
    int client_fd;
    struct sockaddr_in my_addr;
    int status;
    char recv_buf[MAX_MESSAGE_LEN];
    char send_buf[MAX_MESSAGE_LEN];
    
    client_fd = socket(PF_INET, SOCK_STREAM, 0);

    if (client_fd == -1) {
        perror("socket");
        exit(1);
    }

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(8080);
    my_addr.sin_addr.s_addr = inet_addr("192.168.1.40");
    memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);

    status = connect(client_fd, (struct sockaddr *)&my_addr, sizeof my_addr);
    
    if (status < 0) {
        perror("Connection failed");
        exit(1);
    }

    printf("Connection established!\nYou are now chatting. Type '/exit' to quit.\n");

    while (1) {
        int conn;
        char *send_buf_status;

        //printf("> ");

        conn = recv(client_fd, recv_buf, MAX_MESSAGE_LEN - 1, 0);

        if (conn == 0) {  //only check conn if you actually set it
            printf("Lost connection to the host.\n");
            break;
        }

        printf("%s", recv_buf);

    }

    close(client_fd);
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

/* Connect to an access point */  //taken from example code
int connect_to_apctl(int config)
{
	int err;
	int stateLast = -1;

	/* Connect using the first profile */
	err = sceNetApctlConnect(config);
	if (err != 0)
	{
		printf(MODULE_NAME ": sceNetApctlConnect returns %08X\n", err);
		return 0;
	}

	printf(MODULE_NAME ": Connecting...\n");
	while (1)
	{
		int state;
		err = sceNetApctlGetState(&state);
		if (err != 0)
		{
			printf(MODULE_NAME ": sceNetApctlGetState returns $%x\n", err);
			break;
		}
		if (state > stateLast)
		{
			printf("  connection state %d of 4\n", state);
			stateLast = state;
		}
		if (state == 4)
			break; // connected with static IP

		// wait a little before polling again
		sceKernelDelayThread(50 * 1000); // 50ms
	}
	printf(MODULE_NAME ": Connected!\n");

	if (err != 0)
	{
		return 0;
	}

	return 1;
}

//the user thread that handles networking, i presume. also taken from example code
int net_thread(SceSize args, void *argp)
{
	int err;
	do
	{
		if ((err = pspSdkInetInit()))
		{
			printf(MODULE_NAME ": Error, could not initialise the network %08X\n", err);
			break;
		}

		if (connect_to_apctl(3))
		{
			// connected, get my IPADDR and run test
			union SceNetApctlInfo info;

			if (sceNetApctlGetInfo(8, &info) != 0)
				strcpy(info.ip, "unknown IP");

			connect_to_client(); //change this since im running the client currently not server
		}
	} while (0);

	return 0;
}


int main(void)  {
    SceUID thread_id;

    setup_callbacks();

    sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	sceUtilityLoadNetModule(PSP_NET_MODULE_INET);

    pspDebugScreenInit();

    thread_id = sceKernelCreateThread("net_thread", net_thread, 0x11, 256 * 1024, PSP_THREAD_ATTR_USER, NULL); /*name, 
                                                                                                                thread function to actually run, 
                                                                                                                init priority (?, example says 0x11 is default ),
                                                                                                                stack size (example says 256kb is regular default),
                                                                                                                thread atrribute, this means user thread,
                                                                                                                options, no options*/

    if (thread_id < 0) {
        printf("Error, couldn't create thread.\n");
        sceKernelSleepThread();
    }

    sceKernelStartThread(thread_id, 0, NULL); //thread id from create thread, length of arguments in bytes, arguments

    sceKernelExitDeleteThread(0);

    return 0;
}