/*
 * service_a_nats.c
 * SERVICE A: Command sender (Client) using official NATS C client
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <nats/nats.h>

#define NATS_URL "nats://127.0.0.1:5800"
#define COMMAND_SUBJECT "freeswitch.api"

void send_command(natsConnection *conn, const char *command)
{
    natsSubscription *sub = NULL;
    natsMsg *reply = NULL;
    natsStatus s;
    char inbox[256];
    
    // Create unique inbox for response
    snprintf(inbox, sizeof(inbox), "_INBOX.%ld.%d", (long)time(NULL), rand());
    
    printf("\n📤 Sending command: %s\n", command);
    printf("   Reply inbox: %s\n", inbox);
    
    // Create subscription for response
    s = natsConnection_SubscribeSync(&sub, conn, inbox);
    if (s != NATS_OK) {
        printf("❌ Failed to create inbox: %s\n", natsStatus_GetText(s));
        return;
    }
    
    // Auto-unsubscribe after 1 message
    s = natsSubscription_AutoUnsubscribe(sub, 1);
    
    // Publish command with reply-to
    s = natsConnection_PublishRequest(conn, COMMAND_SUBJECT, inbox, 
                                      command, strlen(command));
    if (s != NATS_OK) {
        printf("❌ Publish failed: %s\n", natsStatus_GetText(s));
        natsSubscription_Destroy(sub);
        return;
    }
    
    printf("✓ Command published\n");
    printf("⏳ Waiting for response...\n");
    
    // Wait for response (5 second timeout)
    s = natsSubscription_NextMsg(&reply, sub, 5000);
    if (s == NATS_OK) {
        printf("\n✅ Response received:\n");
        printf("   %.*s\n", 
               natsMsg_GetDataLength(reply),
               natsMsg_GetData(reply));
        natsMsg_Destroy(reply);
    } else if (s == NATS_TIMEOUT) {
        printf("⚠️  Timeout: No response received\n");
    } else {
        printf("❌ Error waiting for response: %s\n", natsStatus_GetText(s));
    }
    
    natsSubscription_Destroy(sub);
}

int main(int argc, char **argv)
{
    natsConnection *conn = NULL;
    natsStatus s;
    
    printf("╔════════════════════════════════════════╗\n");
    printf("║  SERVICE A - Command Sender (Client)   ║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    
    srand(time(NULL));
    
    // Connect to NATS
    s = natsConnection_ConnectTo(&conn, NATS_URL);
    if (s != NATS_OK) {
        fprintf(stderr, "❌ Failed to connect to NATS: %s\n", natsStatus_GetText(s));
        return 1;
    }
    
    printf("✓ Connected to NATS (%s)\n", NATS_URL);
    
    if (argc > 1) {
        // Send single command from argument
        send_command(conn, argv[1]);
    } else {
        // Interactive mode
        printf("\nInteractive mode (type commands, 'quit' to exit):\n");
        printf("Examples: status, version, show channels\n\n");
        
        char command[512];
        while (1) {
            printf("> ");
            fflush(stdout);
            
            if (fgets(command, sizeof(command), stdin) == NULL)
                break;
            
            // Remove newline
            command[strcspn(command, "\n")] = 0;
            
            if (strlen(command) == 0)
                continue;
                
            if (strcmp(command, "quit") == 0 || strcmp(command, "exit") == 0)
                break;
            
            send_command(conn, command);
        }
    }
    
    // Cleanup
    printf("\n✓ Service A stopped\n");
    natsConnection_Destroy(conn);
    
    return 0;
}
