/*
 * show_modules_test.c
 * Simple test: Send "show modules" command to FreeSWITCH via NATS
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nats/nats.h>

#define NATS_URL "nats://127.0.0.1:5800"
#define API_SUBJECT "freeswitch.api"

int main(int argc, char **argv)
{
    natsConnection *conn = NULL;
    natsSubscription *sub = NULL;
    natsMsg *reply = NULL;
    natsStatus s;
    char inbox[256];
    const char *command = "{\"command\":\"show\",\"args\":\"modules\"}";
    
    printf("╔════════════════════════════════════════╗\n");
    printf("║     FreeSWITCH Show Modules Test      ║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    
    s = natsConnection_ConnectTo(&conn, NATS_URL);
    if (s != NATS_OK) {
        fprintf(stderr, "❌ Failed to connect to NATS: %s\n", natsStatus_GetText(s));
        return 1;
    }
    
    printf("✓ Connected to NATS (%s)\n", NATS_URL);
    
    snprintf(inbox, sizeof(inbox), "_INBOX.%ld", (long)time(NULL));
    
    s = natsConnection_SubscribeSync(&sub, conn, inbox);
    if (s != NATS_OK) {
        printf("❌ Failed to create inbox: %s\n", natsStatus_GetText(s));
        natsConnection_Destroy(conn);
        return 1;
    }
    
    natsSubscription_AutoUnsubscribe(sub, 1);
    
    printf("\n📤 Sending command: show modules\n");
    printf("   Subject: %s\n", API_SUBJECT);
    printf("   Payload: %s\n", command);
    printf("   Reply inbox: %s\n", inbox);
    
    s = natsConnection_PublishRequest(conn, API_SUBJECT, inbox, 
                                      command, strlen(command));
    if (s != NATS_OK) {
        printf("❌ Publish failed: %s\n", natsStatus_GetText(s));
        natsSubscription_Destroy(sub);
        natsConnection_Destroy(conn);
        return 1;
    }
    
    printf("✓ Command published\n");
    printf("⏳ Waiting for response...\n\n");
    
    s = natsSubscription_NextMsg(&reply, sub, 5000);
    if (s == NATS_OK) {
        printf("✅ Response received:\n");
        printf("%.*s\n", 
               natsMsg_GetDataLength(reply),
               natsMsg_GetData(reply));
        natsMsg_Destroy(reply);
    } else if (s == NATS_TIMEOUT) {
        printf("⚠️  Timeout: No response received after 5 seconds\n");
    } else {
        printf("❌ Error waiting for response: %s\n", natsStatus_GetText(s));
    }
    
    natsSubscription_Destroy(sub);
    natsConnection_Destroy(conn);
    
    return (s == NATS_OK) ? 0 : 1;
}
